#include <iostream>

#include <openms_thermo_bridge/thermo_bridge.hpp>

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: thermo_host <path/to/file.raw>\n";
        return 1;
    }

    try
    {
        const int scan_count = openms::thermo_bridge::get_scan_count(argv[1]);
        std::cout << "Scan count: " << scan_count << '\n';
        return 0;
    }
    catch (const openms::thermo_bridge::bridge_error& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
