#include <filesystem>
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
} // namespace

TEST_CASE("Missing RAW files are reported as bridge errors")
{
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

    REQUIRE(std::filesystem::exists(sample_path));
    CHECK(openms::thermo_bridge::get_scan_count(sample_path) == OPENMS_THERMO_BRIDGE_TEST_EXPECTED_SCAN_COUNT);
}
