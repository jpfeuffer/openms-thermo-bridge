#include <openms_thermo_bridge/thermo_bridge.hpp>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include <coreclr_delegates.h>
#include <hostfxr.h>
#include <nethost.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#include <sys/sysctl.h>
#endif

#if defined(__linux__)
#include <limits.h>
#include <unistd.h>
#endif

#if defined(_WIN32)
#define OPENMS_THERMO_BRIDGE_CHAR_T_LITERAL(value) L##value
#else
#define OPENMS_THERMO_BRIDGE_CHAR_T_LITERAL(value) value
#endif

using hostfxr_initialize_for_runtime_config_fn =
    int(*)(const char_t*, const hostfxr_initialize_parameters*, hostfxr_handle*);
using hostfxr_get_runtime_delegate_fn =
    int(*)(hostfxr_handle, hostfxr_delegate_type, void**);
using hostfxr_close_fn = int(*)(hostfxr_handle);
using get_scan_count_fn = int(*)(void*);

namespace openms::thermo_bridge
{
namespace
{
#if defined(_WIN32)
using library_handle = HMODULE;
#else
using library_handle = void*;
#endif

std::filesystem::path executable_path()
{
#if defined(_WIN32)
    std::vector<wchar_t> buffer(MAX_PATH);
    while (true)
    {
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0)
        {
            throw bridge_error("Unable to resolve executable path");
        }
        if (length < buffer.size() - 1)
        {
            return std::filesystem::path(buffer.data(), buffer.data() + length);
        }
        buffer.resize(buffer.size() * 2);
    }
#elif defined(__APPLE__)
    std::vector<char> buffer(1024);
    std::uint32_t size = static_cast<std::uint32_t>(buffer.size());
    if (_NSGetExecutablePath(buffer.data(), &size) != 0)
    {
        buffer.resize(size);
        if (_NSGetExecutablePath(buffer.data(), &size) != 0)
        {
            throw bridge_error("Unable to resolve executable path");
        }
    }
    return std::filesystem::weakly_canonical(std::filesystem::path(buffer.data()));
#elif defined(__linux__)
    std::array<char, PATH_MAX> buffer{};
    const auto length = readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
    if (length <= 0)
    {
        throw bridge_error("Unable to resolve executable path");
    }

    buffer[static_cast<std::size_t>(length)] = '\0';
    return std::filesystem::path(buffer.data());
#else
    throw bridge_error("Executable path resolution is not implemented on this platform");
#endif
}

#if defined(_WIN32)
std::string last_error_message()
{
    const DWORD error_code = GetLastError();
    if (error_code == 0)
    {
        return "unknown error";
    }

    LPSTR message_buffer = nullptr;
    const DWORD message_length = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        error_code,
        0,
        reinterpret_cast<LPSTR>(&message_buffer),
        0,
        nullptr);

    std::string message = message_length > 0 ? std::string(message_buffer, message_length) : "unknown error";
    if (message_buffer != nullptr)
    {
        LocalFree(message_buffer);
    }

    while (!message.empty() && (message.back() == '\n' || message.back() == '\r'))
    {
        message.pop_back();
    }

    return message;
}
#endif

library_handle load_library(const std::filesystem::path& path)
{
#if defined(_WIN32)
    const HMODULE handle = LoadLibraryW(path.c_str());
    if (handle == nullptr)
    {
        throw bridge_error("LoadLibraryW failed: " + last_error_message());
    }
    return handle;
#else
    void* handle = dlopen(path.c_str(), RTLD_LAZY | RTLD_LOCAL);
    if (handle == nullptr)
    {
        throw bridge_error(std::string("dlopen failed: ") + dlerror());
    }
    return handle;
#endif
}

void* get_export(library_handle library, const char* export_name)
{
#if defined(_WIN32)
    void* symbol = reinterpret_cast<void*>(GetProcAddress(library, export_name));
    if (symbol == nullptr)
    {
        throw bridge_error(std::string("GetProcAddress failed for ") + export_name + ": " + last_error_message());
    }
    return symbol;
#else
    dlerror();
    void* symbol = dlsym(library, export_name);
    const char* error = dlerror();
    if (error != nullptr)
    {
        throw bridge_error(std::string("dlsym failed for ") + export_name + ": " + error);
    }
    return symbol;
#endif
}

std::filesystem::path hostfxr_path()
{
    std::array<char_t, 4096> buffer{};
    std::size_t buffer_size = buffer.size();
    if (get_hostfxr_path(buffer.data(), &buffer_size, nullptr) != 0)
    {
        throw bridge_error("get_hostfxr_path failed");
    }

    return std::filesystem::path(buffer.data());
}

std::filesystem::path discover_dotnet_root()
{
    // Prefer an explicit x64 root for Apple Silicon/Rosetta workflows.
#if defined(__APPLE__) && defined(__x86_64__)
    const char* dotnet_root_x64_env = std::getenv("DOTNET_ROOT_X64");
    if (dotnet_root_x64_env != nullptr && dotnet_root_x64_env[0] != '\0')
    {
        std::filesystem::path root(dotnet_root_x64_env);
        if (std::filesystem::is_directory(root))
        {
            return root;
        }
    }
#endif

    // Prefer the DOTNET_ROOT environment variable
    const char* dotnet_root_env = std::getenv("DOTNET_ROOT");
    if (dotnet_root_env != nullptr && dotnet_root_env[0] != '\0')
    {
        std::filesystem::path root(dotnet_root_env);
        if (std::filesystem::is_directory(root))
        {
            return root;
        }
    }

    // Fall back to the hostfxr location: hostfxr is typically at
    // <dotnet_root>/host/fxr/<version>/libhostfxr.{so,dylib,dll}
    try
    {
        const auto fxr = hostfxr_path();
        // Go up: fxr file -> version dir -> fxr dir -> host dir -> dotnet_root
        auto candidate = fxr.parent_path().parent_path().parent_path().parent_path();
        if (std::filesystem::is_directory(candidate / "shared"))
        {
            return candidate;
        }
    }
    catch (...)
    {
    }

    return {};
}

#if defined(__APPLE__)
bool is_running_under_rosetta()
{
    int translated = 0;
    std::size_t translated_size = sizeof(translated);
    if (sysctlbyname("sysctl.proc_translated", &translated, &translated_size, nullptr, 0) != 0)
    {
        return false;
    }

    return translated == 1;
}
#endif

void warn_about_emulation_if_needed()
{
#if defined(__APPLE__)
    static std::once_flag warning_once;
    std::call_once(warning_once, []()
    {
        if (is_running_under_rosetta())
        {
            std::cerr
                << "[thermo_bridge] warning: running through Rosetta because Thermo RawFileReader does not yet have "
                   "native osx-arm64 support. Expect slower performance under emulation. Track "
                   "https://github.com/thermofisherlsms/RawFileReader/issues/3?issue=fgcz%7Crawrr%7C75 and contact "
                   "Thermo support if native Apple Silicon support is important for your workflow.\n";
        }
    });
#endif
}

#if defined(_WIN32)
void HOSTFXR_CALLTYPE hostfxr_error_writer_callback(const char_t* message)
{
    std::wcerr << L"[hostfxr] " << message << L'\n';
}
#else
void HOSTFXR_CALLTYPE hostfxr_error_writer_callback(const char_t* message)
{
    std::cerr << "[hostfxr] " << message << '\n';
}
#endif

std::basic_string<char_t> to_char_t_path(const std::filesystem::path& path)
{
#if defined(_WIN32)
    return path.native();
#else
    return path.string();
#endif
}
} // namespace

std::filesystem::path default_managed_directory()
{
    return executable_path().parent_path() / "managed";
}

int get_scan_count(const std::filesystem::path& raw_file_path)
{
    try
    {
        return get_scan_count(raw_file_path, default_managed_directory());
    }
    catch (const bridge_error&)
    {
        throw;
    }
    catch (const std::exception& e)
    {
        throw bridge_error(std::string("Thermo bridge internal error: ") + e.what());
    }
    catch (...)
    {
        throw bridge_error("Thermo bridge failed with an unknown internal error");
    }
}

int get_scan_count(const std::filesystem::path& raw_file_path, const std::filesystem::path& managed_directory)
{
    warn_about_emulation_if_needed();

    const std::filesystem::path runtime_config = managed_directory / "ThermoWrapperManaged.runtimeconfig.json";
    const std::filesystem::path assembly_path = managed_directory / "ThermoWrapperManaged.dll";

    if (!std::filesystem::exists(runtime_config) || !std::filesystem::exists(assembly_path))
    {
        throw bridge_error("Managed bridge runtime files are missing (managed_dir=" + managed_directory.string()
            + ", runtime_config_exists=" + (std::filesystem::exists(runtime_config) ? "yes" : "no")
            + ", assembly_exists=" + (std::filesystem::exists(assembly_path) ? "yes" : "no") + ")");
    }

    std::filesystem::path fxr_path;
    try
    {
        fxr_path = hostfxr_path();
        std::cerr << "[thermo_bridge] hostfxr path: " << fxr_path << "\n";
    }
    catch (const bridge_error&)
    {
        throw;
    }
    catch (...)
    {
        throw bridge_error("get_hostfxr_path failed with an unknown exception");
    }

    library_handle hostfxr_library = nullptr;
    try
    {
        hostfxr_library = load_library(fxr_path);
    }
    catch (const bridge_error&)
    {
        throw;
    }
    catch (...)
    {
        throw bridge_error("Failed to load hostfxr library (unknown error)");
    }

    auto hostfxr_initialize = reinterpret_cast<hostfxr_initialize_for_runtime_config_fn>(
        get_export(hostfxr_library, "hostfxr_initialize_for_runtime_config"));
    auto hostfxr_get_delegate = reinterpret_cast<hostfxr_get_runtime_delegate_fn>(
        get_export(hostfxr_library, "hostfxr_get_runtime_delegate"));
    auto hostfxr_close = reinterpret_cast<hostfxr_close_fn>(
        get_export(hostfxr_library, "hostfxr_close"));

    // Register an error writer so hostfxr diagnostic messages go to stderr.
    // This export may not exist in all hostfxr versions, so failure is not fatal.
    try
    {
        auto set_error_writer = reinterpret_cast<hostfxr_set_error_writer_fn>(
            get_export(hostfxr_library, "hostfxr_set_error_writer"));
        if (set_error_writer != nullptr)
        {
            set_error_writer(&hostfxr_error_writer_callback);
        }
    }
    catch (...)
    {
        // hostfxr_set_error_writer not available; ignore
    }

    // Resolve dotnet_root and pass it explicitly so hostfxr can find the
    // shared frameworks even when .NET is installed in a non-default location
    // (e.g. /Users/runner/.dotnet on macOS GitHub Actions runners).
    const auto dotnet_root = discover_dotnet_root();
    const auto dotnet_root_native = to_char_t_path(dotnet_root);
    const auto host_path_native = to_char_t_path(executable_path());

    hostfxr_initialize_parameters init_params{};
    init_params.size = sizeof(init_params);
    init_params.host_path = host_path_native.c_str();
    if (!dotnet_root.empty())
    {
        init_params.dotnet_root = dotnet_root_native.c_str();
        std::cerr << "[thermo_bridge] dotnet_root: " << dotnet_root << "\n";
    }
    else
    {
        std::cerr << "[thermo_bridge] dotnet_root: <not resolved, using hostfxr default>\n";
    }

    hostfxr_handle context = nullptr;
    const auto runtime_config_native = to_char_t_path(runtime_config);
    std::cerr << "[thermo_bridge] runtime_config: " << runtime_config << "\n";
    int rc = 0;
    try
    {
        rc = hostfxr_initialize(runtime_config_native.c_str(), &init_params, &context);
    }
    catch (...)
    {
        throw bridge_error("hostfxr_initialize_for_runtime_config threw an unexpected exception");
    }
    std::cerr << "[thermo_bridge] hostfxr_initialize rc=" << rc << " context=" << (context ? "valid" : "null") << "\n";
    // Return codes: 0 = Success, 1 = Success_HostAlreadyInitialized,
    // 2 = Success_DifferentRuntimeProperties.  Only negative values are errors.
    if (rc < 0)
    {
        throw bridge_error("hostfxr_initialize_for_runtime_config failed with code " + std::to_string(rc));
    }
    if (context == nullptr)
    {
        throw bridge_error("hostfxr_initialize_for_runtime_config returned null context (code " + std::to_string(rc) + ")");
    }

    load_assembly_and_get_function_pointer_fn load_assembly = nullptr;
    try
    {
        rc = hostfxr_get_delegate(
            context,
            hdt_load_assembly_and_get_function_pointer,
            reinterpret_cast<void**>(&load_assembly));
    }
    catch (...)
    {
        hostfxr_close(context);
        throw bridge_error("hostfxr_get_runtime_delegate threw an unexpected exception");
    }
    hostfxr_close(context);
    std::cerr << "[thermo_bridge] hostfxr_get_delegate rc=" << rc << "\n";

    if (rc != 0 || load_assembly == nullptr)
    {
        throw bridge_error("hostfxr_get_runtime_delegate failed with code " + std::to_string(rc));
    }

    get_scan_count_fn get_scan_count_entry = nullptr;
    const auto assembly_path_native = to_char_t_path(assembly_path);
    std::cerr << "[thermo_bridge] loading assembly: " << assembly_path << "\n";
    try
    {
        rc = load_assembly(
            assembly_path_native.c_str(),
            OPENMS_THERMO_BRIDGE_CHAR_T_LITERAL("ThermoWrapperManaged.RawBridge, ThermoWrapperManaged"),
            OPENMS_THERMO_BRIDGE_CHAR_T_LITERAL("GetScanCount"),
            UNMANAGEDCALLERSONLY_METHOD,
            nullptr,
            reinterpret_cast<void**>(&get_scan_count_entry));
    }
    catch (...)
    {
        throw bridge_error("load_assembly_and_get_function_pointer threw an unexpected exception");
    }
    std::cerr << "[thermo_bridge] load_assembly rc=" << rc << "\n";

    if (rc != 0 || get_scan_count_entry == nullptr)
    {
        throw bridge_error("load_assembly_and_get_function_pointer failed with code " + std::to_string(rc));
    }

    const auto utf8_string = raw_file_path.u8string();
    std::vector<char> utf8_path(utf8_string.begin(), utf8_string.end());
    utf8_path.push_back('\0');

    std::cerr << "[thermo_bridge] calling GetScanCount for: " << raw_file_path << "\n";
    int scan_count = 0;
    try
    {
        scan_count = get_scan_count_entry(utf8_path.data());
    }
    catch (...)
    {
        throw bridge_error("Managed GetScanCount threw an unexpected exception");
    }
    std::cerr << "[thermo_bridge] GetScanCount returned: " << scan_count << "\n";

    if (scan_count == -1)
    {
        throw bridge_error("Could not open RAW file");
    }
    if (scan_count == -3)
    {
        throw bridge_error("Thermo RawFileReader is not supported on this platform");
    }
    if (scan_count < 0)
    {
        throw bridge_error("Managed bridge failed while reading RAW file (code " + std::to_string(scan_count) + ")");
    }

    return scan_count;
}
} // namespace openms::thermo_bridge
