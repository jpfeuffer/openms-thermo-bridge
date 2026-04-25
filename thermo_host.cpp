#include <array>
#include <filesystem>
#include <iostream>
#include <string>

#include <dlfcn.h>
#include <limits.h>
#include <unistd.h>

#include <coreclr_delegates.h>
#include <hostfxr.h>
#include <nethost.h>

using hostfxr_initialize_for_runtime_config_fn =
    int(*)(const char_t*, const hostfxr_initialize_parameters*, hostfxr_handle*);
using hostfxr_get_runtime_delegate_fn =
    int(*)(hostfxr_handle, hostfxr_delegate_type, void**);
using hostfxr_close_fn = int(*)(hostfxr_handle);
using get_scan_count_fn = int(*)(void*);

namespace
{
std::filesystem::path get_executable_directory()
{
    std::array<char, PATH_MAX> buffer{};
    const auto length = readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
    if (length <= 0)
    {
        throw std::runtime_error("Unable to resolve executable path");
    }

    buffer[static_cast<std::size_t>(length)] = '\0';
    return std::filesystem::path(buffer.data()).parent_path();
}

void* load_library(const char* path)
{
    void* handle = dlopen(path, RTLD_LAZY | RTLD_LOCAL);
    if (handle == nullptr)
    {
        throw std::runtime_error(std::string("dlopen failed: ") + dlerror());
    }

    return handle;
}

void* get_export(void* library, const char* export_name)
{
    dlerror();
    void* symbol = dlsym(library, export_name);
    const char* error = dlerror();
    if (error != nullptr)
    {
        throw std::runtime_error(std::string("dlsym failed for ") + export_name + ": " + error);
    }

    return symbol;
}
} // namespace

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: thermo_host <path/to/file.raw>\n";
        return 1;
    }

    try
    {
        const std::filesystem::path publish_dir = get_executable_directory();
        const std::filesystem::path managed_dir = publish_dir / "managed";
        const std::filesystem::path runtime_config = managed_dir / "ThermoWrapperManaged.runtimeconfig.json";
        const std::filesystem::path assembly_path = managed_dir / "ThermoWrapperManaged.dll";

        std::array<char_t, PATH_MAX> hostfxr_path{};
        std::size_t hostfxr_path_size = hostfxr_path.size();
        if (get_hostfxr_path(hostfxr_path.data(), &hostfxr_path_size, nullptr) != 0)
        {
            throw std::runtime_error("get_hostfxr_path failed");
        }

        void* hostfxr_library = load_library(hostfxr_path.data());
        auto hostfxr_initialize = reinterpret_cast<hostfxr_initialize_for_runtime_config_fn>(
            get_export(hostfxr_library, "hostfxr_initialize_for_runtime_config"));
        auto hostfxr_get_delegate = reinterpret_cast<hostfxr_get_runtime_delegate_fn>(
            get_export(hostfxr_library, "hostfxr_get_runtime_delegate"));
        auto hostfxr_close = reinterpret_cast<hostfxr_close_fn>(
            get_export(hostfxr_library, "hostfxr_close"));

        hostfxr_handle context = nullptr;
        int rc = hostfxr_initialize(runtime_config.c_str(), nullptr, &context);
        if (rc != 0 || context == nullptr)
        {
            throw std::runtime_error("hostfxr_initialize_for_runtime_config failed with code " + std::to_string(rc));
        }

        load_assembly_and_get_function_pointer_fn load_assembly = nullptr;
        rc = hostfxr_get_delegate(
            context,
            hdt_load_assembly_and_get_function_pointer,
            reinterpret_cast<void**>(&load_assembly));
        hostfxr_close(context);

        if (rc != 0 || load_assembly == nullptr)
        {
            throw std::runtime_error("hostfxr_get_runtime_delegate failed with code " + std::to_string(rc));
        }

        get_scan_count_fn get_scan_count = nullptr;
        rc = load_assembly(
            assembly_path.c_str(),
            "ThermoWrapperManaged.RawBridge, ThermoWrapperManaged",
            "GetScanCount",
            UNMANAGEDCALLERSONLY_METHOD,
            nullptr,
            reinterpret_cast<void**>(&get_scan_count));

        if (rc != 0 || get_scan_count == nullptr)
        {
            throw std::runtime_error("load_assembly_and_get_function_pointer failed with code " + std::to_string(rc));
        }

        const int scan_count = get_scan_count(argv[1]);

        if (scan_count < 0)
        {
            if (scan_count == -1)
            {
                std::cerr << "Could not open RAW file\n";
            }
            else
            {
                std::cerr << "Managed bridge failed while reading RAW file\n";
            }

            return 1;
        }

        std::cout << "Scan count: " << scan_count << '\n';
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
