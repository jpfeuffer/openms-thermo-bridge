#include <openms_thermo_bridge/thermo_bridge.hpp>

#include <array>
#include <filesystem>
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
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> buffer(size);
    if (_NSGetExecutablePath(buffer.data(), &size) != 0)
    {
        throw bridge_error("Unable to resolve executable path");
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

#if defined(_WIN32)
    return std::filesystem::path(buffer.data());
#else
    return std::filesystem::path(buffer.data());
#endif
}

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
    return get_scan_count(raw_file_path, default_managed_directory());
}

int get_scan_count(const std::filesystem::path& raw_file_path, const std::filesystem::path& managed_directory)
{
    const std::filesystem::path runtime_config = managed_directory / "ThermoWrapperManaged.runtimeconfig.json";
    const std::filesystem::path assembly_path = managed_directory / "ThermoWrapperManaged.dll";

    if (!std::filesystem::exists(runtime_config) || !std::filesystem::exists(assembly_path))
    {
        throw bridge_error("Managed bridge runtime files are missing. Copy the managed directory next to your executable.");
    }

    const library_handle hostfxr_library = load_library(hostfxr_path());
    auto hostfxr_initialize = reinterpret_cast<hostfxr_initialize_for_runtime_config_fn>(
        get_export(hostfxr_library, "hostfxr_initialize_for_runtime_config"));
    auto hostfxr_get_delegate = reinterpret_cast<hostfxr_get_runtime_delegate_fn>(
        get_export(hostfxr_library, "hostfxr_get_runtime_delegate"));
    auto hostfxr_close = reinterpret_cast<hostfxr_close_fn>(
        get_export(hostfxr_library, "hostfxr_close"));

    hostfxr_handle context = nullptr;
    const auto runtime_config_native = to_char_t_path(runtime_config);
    int rc = hostfxr_initialize(runtime_config_native.c_str(), nullptr, &context);
    if (rc != 0 || context == nullptr)
    {
        throw bridge_error("hostfxr_initialize_for_runtime_config failed with code " + std::to_string(rc));
    }

    load_assembly_and_get_function_pointer_fn load_assembly = nullptr;
    rc = hostfxr_get_delegate(
        context,
        hdt_load_assembly_and_get_function_pointer,
        reinterpret_cast<void**>(&load_assembly));
    hostfxr_close(context);

    if (rc != 0 || load_assembly == nullptr)
    {
        throw bridge_error("hostfxr_get_runtime_delegate failed with code " + std::to_string(rc));
    }

    get_scan_count_fn get_scan_count_entry = nullptr;
    const auto assembly_path_native = to_char_t_path(assembly_path);
    rc = load_assembly(
        assembly_path_native.c_str(),
        OPENMS_THERMO_BRIDGE_CHAR_T_LITERAL("ThermoWrapperManaged.RawBridge, ThermoWrapperManaged"),
        OPENMS_THERMO_BRIDGE_CHAR_T_LITERAL("GetScanCount"),
        UNMANAGEDCALLERSONLY_METHOD,
        nullptr,
        reinterpret_cast<void**>(&get_scan_count_entry));

    if (rc != 0 || get_scan_count_entry == nullptr)
    {
        throw bridge_error("load_assembly_and_get_function_pointer failed with code " + std::to_string(rc));
    }

    const std::string utf8_path = raw_file_path.u8string();
    const int scan_count = get_scan_count_entry(const_cast<char*>(utf8_path.c_str()));
    if (scan_count == -1)
    {
        throw bridge_error("Could not open RAW file");
    }
    if (scan_count < 0)
    {
        throw bridge_error("Managed bridge failed while reading RAW file");
    }

    return scan_count;
}
} // namespace openms::thermo_bridge
