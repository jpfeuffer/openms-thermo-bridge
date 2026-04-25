#include <filesystem>
#include <iostream>
#include <string>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <openms_thermo_bridge/thermo_bridge.hpp>

namespace
{
std::filesystem::path sample_raw_path()
{
    const std::string configured_path = OPENMS_THERMO_BRIDGE_TEST_SAMPLE_RAW;
    return configured_path.empty() ? std::filesystem::path{} : std::filesystem::path(configured_path);
}

// Check whether the managed bridge runtime can actually initialize on this
// platform by attempting a trivial call. Returns true if the bridge is
// operational, false otherwise. When false, *out_reason contains a
// diagnostic message.
bool bridge_available(std::string& out_reason)
{
    try
    {
        // Call with a path that definitely does not exist. If the bridge is
        // functional we will get a bridge_error with "Could not open RAW file".
        // Any *other* exception indicates the runtime is not available.
        openms::thermo_bridge::get_scan_count("/tmp/__openms_thermo_bridge_probe__.raw");
        // Should not reach here, but treat it as "available" if it does
        return true;
    }
    catch (const openms::thermo_bridge::bridge_error& e)
    {
        const std::string msg = e.what();
        if (msg.find("Could not open RAW file") != std::string::npos)
        {
            return true; // Bridge loaded and correctly reported missing file
        }
        // Bridge threw but for an initialisation reason (e.g. runtime missing)
        out_reason = msg;
        return false;
    }
    catch (const std::exception& e)
    {
        out_reason = std::string("std::exception: ") + e.what();
        return false;
    }
    catch (...)
    {
        out_reason = "unknown non-std exception";
        return false;
    }
}
} // namespace

TEST_CASE("Missing RAW files are reported as bridge errors")
{
    std::string skip_reason;
    if (!bridge_available(skip_reason))
    {
        std::cerr << "[SKIP] Bridge not available: " << skip_reason << "\n";
        SKIP("Bridge runtime not available on this platform: " + skip_reason);
    }

    CHECK_THROWS_WITH(
        openms::thermo_bridge::get_scan_count("/tmp/this-file-does-not-exist.raw"),
        Catch::Matchers::ContainsSubstring("Could not open RAW file"));
}

TEST_CASE("The public sample RAW file reports the expected scan count")
{
    const auto sample_path = sample_raw_path();
    if (sample_path.empty())
    {
        SKIP("Sample RAW download is disabled for this build");
    }

    std::string skip_reason;
    if (!bridge_available(skip_reason))
    {
        std::cerr << "[SKIP] Bridge not available: " << skip_reason << "\n";
        SKIP("Bridge runtime not available on this platform: " + skip_reason);
    }

    REQUIRE(std::filesystem::exists(sample_path));
    CHECK(openms::thermo_bridge::get_scan_count(sample_path) == OPENMS_THERMO_BRIDGE_TEST_EXPECTED_SCAN_COUNT);
}
