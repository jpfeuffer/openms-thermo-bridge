# OpenMS Thermo Raw File Bridge

A CMake-packaged native C++ bridge that embeds the .NET runtime, calls the official ThermoFisher RawFileReader libraries through a managed shim, and exposes Thermo RAW scan counting to native consumers.

## What This Does
- Builds a native bridge library and the `thermo_host` CLI with CMake
- Publishes a managed bridge with an `UnmanagedCallersOnly` `GetScanCount` entry point
- Packages exported CMake targets for `add_subdirectory`/FetchContent and `find_package(... CONFIG)` workflows
- Runs a small Catch2-based test suite through CTest
- Supports Linux, macOS, and Windows builds

## Build
The project does **not** download the Thermo vendor packages by default.

### Apple Silicon macOS workaround
Thermo RawFileReader currently works in this repository on Apple Silicon macOS only through an `osx-x64`/Rosetta workaround. On Apple Silicon, CMake enables `OPENMS_THERMO_BRIDGE_OSX_ARM64_X64_WORKAROUND=ON` by default, forces `x86_64` native targets, and publishes the managed bridge with `dotnet publish --os osx -a x64`.

This workaround trades correctness for native performance: expect slower startup and RAW access under emulation. You can disable it with `-DOPENMS_THERMO_BRIDGE_OSX_ARM64_X64_WORKAROUND=OFF`, but current upstream Thermo packages are known to fail as native `osx-arm64` builds.

You also need an `osx-x64` .NET 8 SDK/runtime available to the build and the final executable. If it is not the default `dotnet` installation on your machine, set `DOTNET_ROOT_X64=/absolute/path/to/x64/dotnet` (or `DOTNET_ROOT`) so CMake and the embedded hostfxr loader can find the matching x64 `nethost` and runtime files.

If Apple Silicon support matters to you, please track or upvote the upstream report at https://github.com/thermofisherlsms/RawFileReader/issues/3?issue=fgcz%7Crawrr%7C75 and consider contacting Thermo support to request native `osx-arm64` binaries.

### Option 1: provide the vendor `.nupkg` files yourself
Place the following files in `vendor/thermo-feed` or point CMake at another directory with `-DOPENMS_THERMO_BRIDGE_VENDOR_DIR=/absolute/path`:
- `ThermoFisher.CommonCore.BackgroundSubtraction.8.0.6.nupkg`
- `ThermoFisher.CommonCore.Data.8.0.6.nupkg`
- `ThermoFisher.CommonCore.MassPrecisionEstimator.8.0.6.nupkg`
- `ThermoFisher.CommonCore.RandomAccessReaderPlugin.8.0.6.nupkg`
- `ThermoFisher.CommonCore.RawfileReader.8.0.6.nupkg`

Then configure and build:

```bash
cmake -S . -B build
cmake --build build --parallel
```

### Option 2: explicitly allow CMake to fetch the vendor packages
```bash
cmake -S . -B build \
  -DOPENMS_THERMO_BRIDGE_ENABLE_VENDOR_DOWNLOAD=ON
cmake --build build --parallel
```

For Linux convenience, `build_linux.sh` configures, builds, and runs the tests with the vendor-download option enabled.

## Test
```bash
ctest --test-dir build --output-on-failure
```

## Run
```bash
./build/thermo_host /path/to/file.raw
```

Expected output:

```text
Scan count: <number>
```

The public `ginkgotoxin-ms-positive.raw` sample used in CI currently reports:

```text
Scan count: 90
```

## CMake Consumption
### FetchContent / add_subdirectory
```cmake
FetchContent_Declare(OpenMSThermoBridge
  GIT_REPOSITORY https://github.com/jpfeuffer/openms-thermo-bridge.git
  GIT_TAG main)
FetchContent_MakeAvailable(OpenMSThermoBridge)

add_executable(my_tool main.cpp)
target_link_libraries(my_tool PRIVATE OpenMSThermoBridge::openms_thermo_bridge)
openms_thermo_bridge_copy_runtime_files(TARGET my_tool)
```

### Installed package / `find_package(... CONFIG)`
```cmake
find_package(OpenMSThermoBridge CONFIG REQUIRED)

add_executable(my_tool main.cpp)
target_link_libraries(my_tool PRIVATE OpenMSThermoBridge::openms_thermo_bridge)
openms_thermo_bridge_copy_runtime_files(TARGET my_tool)
```

## Requirements
- CMake 3.21+
- .NET 8 SDK with the platform-specific `nethost` pack installed
- A C++17 compiler
- Network access to `api.nuget.org` and, when the relevant options are enabled, `raw.githubusercontent.com`
- On Apple Silicon macOS, an `osx-x64` .NET 8 installation usable through Rosetta when the workaround is enabled

## Status
- [x] Linux, macOS, and Windows CMake builds
- [x] Native C++ bridge library and CLI
- [x] Catch2/CTest integration tests
- [x] Exported CMake package config
