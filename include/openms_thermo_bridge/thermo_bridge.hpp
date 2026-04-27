#pragma once

#include <filesystem>
#include <stdexcept>

namespace openms::thermo_bridge
{
class bridge_error : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

std::filesystem::path default_managed_directory();
int get_scan_count(const std::filesystem::path& raw_file_path);
int get_scan_count(const std::filesystem::path& raw_file_path, const std::filesystem::path& managed_directory);
} // namespace openms::thermo_bridge
