cmake_minimum_required(VERSION 3.21)

project(OpenMSThermoBridge VERSION 0.1.0 LANGUAGES CXX)

include(CTest)
include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

option(OPENMS_THERMO_BRIDGE_BUILD_CLI "Build the thermo_host CLI executable." ON)
option(OPENMS_THERMO_BRIDGE_ENABLE_VENDOR_DOWNLOAD "Download Thermo vendor packages during the build." OFF)
set(OPENMS_THERMO_BRIDGE_VENDOR_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/thermo-feed" CACHE PATH "Directory containing the Thermo vendor .nupkg files.")
set(OPENMS_THERMO_BRIDGE_THERMO_COMMIT "db08ff32cbb626d259b6d4d98492c83d6f2f6871" CACHE STRING "RawFileReader commit used for vendor package snapshots.")
set(OPENMS_THERMO_BRIDGE_THERMO_VERSION "8.0.6" CACHE STRING "Thermo RawFileReader package version.")
set(OPENMS_THERMO_BRIDGE_SAMPLE_RAW_URL "https://raw.githubusercontent.com/HegemanLab/small-thermo-raw-file-examples/cfac6a25b44913273dd458d5f5db9b20b0c1107d/ginkgotoxin-ms-positive.raw" CACHE STRING "Public sample RAW file used by the test suite.")
option(OPENMS_THERMO_BRIDGE_DOWNLOAD_TEST_DATA "Download public sample RAW data when BUILD_TESTING is enabled." ON)
set(OPENMS_THERMO_BRIDGE_PREBUILT_MANAGED_DIR "" CACHE PATH
  "Path to a pre-built managed output directory (e.g. unpacked release artifact). When set, dotnet publish is skipped.")
option(OPENMS_THERMO_BRIDGE_DOWNLOAD_PREBUILT_MANAGED
  "Automatically download the pre-built managed artifact for the current platform from GitHub Releases." OFF)
set(_openms_thermo_bridge_osx_arm64_workaround_default OFF)
if(APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)$")
  set(_openms_thermo_bridge_osx_arm64_workaround_default ON)
endif()
option(
  OPENMS_THERMO_BRIDGE_OSX_ARM64_X64_WORKAROUND
  "On Apple Silicon, build/publish x64 artifacts so Thermo RawFileReader can run through Rosetta."
  ${_openms_thermo_bridge_osx_arm64_workaround_default})

set(OPENMS_THERMO_BRIDGE_DOTNET_PUBLISH_ARGS)
unset(OPENMS_THERMO_BRIDGE_DOTNET_HOST_RID_OVERRIDE CACHE)
if(APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)$")
  if(OPENMS_THERMO_BRIDGE_OSX_ARM64_X64_WORKAROUND)
    set(CMAKE_OSX_ARCHITECTURES "x86_64" CACHE STRING
      "Build the native bridge as x86_64 on Apple Silicon so it can run via Rosetta with Thermo RawFileReader." FORCE)
    set(OPENMS_THERMO_BRIDGE_DOTNET_PUBLISH_ARGS --os osx -a x64)
    set(OPENMS_THERMO_BRIDGE_DOTNET_HOST_RID_OVERRIDE osx-x64 CACHE INTERNAL
      "Override the nethost RID when using the Apple Silicon x64/Rosetta workaround.")
    message(WARNING
      "Apple Silicon workaround enabled: OpenMS Thermo Bridge will build and publish osx-x64 artifacts so Thermo "
      "RawFileReader can run through Rosetta. Performance may be reduced under emulation. Please track or upvote "
      "https://github.com/thermofisherlsms/RawFileReader/issues/3?issue=fgcz%7Crawrr%7C75 and contact Thermo support "
      "if native osx-arm64 support matters for your workflow.")
  else()
    message(WARNING
      "Native osx-arm64 builds are known to fail with Thermo RawFileReader. Enable "
      "OPENMS_THERMO_BRIDGE_OSX_ARM64_X64_WORKAROUND for the Rosetta-based x64 workaround.")
  endif()
endif()

find_package(DotNetHost REQUIRED)

set(OPENMS_THERMO_BRIDGE_MANAGED_DIR "${CMAKE_CURRENT_BINARY_DIR}/openms_thermo_bridge_managed")
set(OPENMS_THERMO_BRIDGE_TEST_DATA_DIR "${CMAKE_CURRENT_BINARY_DIR}/testdata")
set(OPENMS_THERMO_BRIDGE_NUGET_CONFIG "${CMAKE_CURRENT_BINARY_DIR}/OpenMSThermoBridge.NuGet.config")

include(OpenMSThermoBridgeFunctions)

# ---------------------------------------------------------------------------
# Determine the platform identifier used for release artifact zip file names.
# ---------------------------------------------------------------------------
if(WIN32)
  set(_openms_thermo_bridge_platform "win-x64")
elseif(APPLE)
  # Always use osx-x64 artifacts (Rosetta workaround applies on Apple Silicon).
  set(_openms_thermo_bridge_platform "osx-x64")
else()
  set(_openms_thermo_bridge_platform "linux-x64")
endif()

# ---------------------------------------------------------------------------
# Auto-download pre-built managed artifacts when requested.
# ---------------------------------------------------------------------------
if(OPENMS_THERMO_BRIDGE_DOWNLOAD_PREBUILT_MANAGED AND NOT OPENMS_THERMO_BRIDGE_PREBUILT_MANAGED_DIR)
  set(_openms_thermo_bridge_version "${PROJECT_VERSION}")
  set(_openms_thermo_bridge_zip_name
    "openms-thermo-bridge-managed-${_openms_thermo_bridge_platform}-${_openms_thermo_bridge_version}.zip")
  set(_openms_thermo_bridge_zip_url
    "https://github.com/jpfeuffer/openms-thermo-bridge/releases/download/v${_openms_thermo_bridge_version}/${_openms_thermo_bridge_zip_name}")
  set(_openms_thermo_bridge_zip_dest
    "${CMAKE_CURRENT_BINARY_DIR}/prebuilt-managed/${_openms_thermo_bridge_zip_name}")
  set(_openms_thermo_bridge_extract_dir
    "${CMAKE_CURRENT_BINARY_DIR}/prebuilt-managed/extracted")

  if(APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)$")
    message(STATUS
      "OpenMS Thermo Bridge: downloading pre-built osx-x64 managed artifact for use through Rosetta on Apple Silicon.")
  else()
    message(STATUS
      "OpenMS Thermo Bridge: downloading pre-built ${_openms_thermo_bridge_platform} managed artifact from GitHub Releases.")
  endif()

  file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/prebuilt-managed")
  if(NOT EXISTS "${_openms_thermo_bridge_zip_dest}")
    file(DOWNLOAD
      "${_openms_thermo_bridge_zip_url}"
      "${_openms_thermo_bridge_zip_dest}"
      STATUS _openms_thermo_bridge_dl_status
      TLS_VERIFY ON)
    list(GET _openms_thermo_bridge_dl_status 0 _openms_thermo_bridge_dl_code)
    list(GET _openms_thermo_bridge_dl_status 1 _openms_thermo_bridge_dl_msg)
    if(NOT _openms_thermo_bridge_dl_code EQUAL 0)
      message(FATAL_ERROR
        "OpenMS Thermo Bridge: failed to download pre-built managed artifact from '${_openms_thermo_bridge_zip_url}': "
        "${_openms_thermo_bridge_dl_msg}. "
        "Either provide the artifact manually via OPENMS_THERMO_BRIDGE_PREBUILT_MANAGED_DIR or ensure a .NET 8 SDK is "
        "available so the managed bridge can be built from source.")
    endif()
  endif()

  file(MAKE_DIRECTORY "${_openms_thermo_bridge_extract_dir}")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar xf "${_openms_thermo_bridge_zip_dest}"
    WORKING_DIRECTORY "${_openms_thermo_bridge_extract_dir}"
    RESULT_VARIABLE _openms_thermo_bridge_extract_result)
  if(NOT _openms_thermo_bridge_extract_result EQUAL 0)
    message(FATAL_ERROR
      "OpenMS Thermo Bridge: failed to extract '${_openms_thermo_bridge_zip_dest}'.")
  endif()

  # Store the resolved directory in a local variable; do not force-overwrite the
  # user-visible cache entry to avoid silently discarding manual overrides on
  # subsequent reconfigures.
  set(_openms_thermo_bridge_resolved_prebuilt_dir "${_openms_thermo_bridge_extract_dir}")
endif()

if(_openms_thermo_bridge_resolved_prebuilt_dir OR OPENMS_THERMO_BRIDGE_PREBUILT_MANAGED_DIR)
  if(_openms_thermo_bridge_resolved_prebuilt_dir)
    set(_openms_thermo_bridge_prebuilt_path "${_openms_thermo_bridge_resolved_prebuilt_dir}")
  else()
    set(_openms_thermo_bridge_prebuilt_path "${OPENMS_THERMO_BRIDGE_PREBUILT_MANAGED_DIR}")
  endif()

  # ---------------------------------------------------------------------------
  # Use a pre-built managed directory (skip dotnet publish).
  # ---------------------------------------------------------------------------
  foreach(_required_file
      "ThermoWrapperManaged.dll"
      "ThermoWrapperManaged.runtimeconfig.json")
    if(NOT EXISTS "${_openms_thermo_bridge_prebuilt_path}/${_required_file}")
      message(FATAL_ERROR
        "OpenMS Thermo Bridge: pre-built managed directory '${_openms_thermo_bridge_prebuilt_path}' "
        "does not contain the required file '${_required_file}'.")
    endif()
  endforeach()

  if(APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)$" AND OPENMS_THERMO_BRIDGE_OSX_ARM64_X64_WORKAROUND)
    message(STATUS
      "OpenMS Thermo Bridge: using pre-built osx-x64 managed artifacts from "
      "'${_openms_thermo_bridge_prebuilt_path}' through Rosetta on Apple Silicon; "
      "performance may be reduced under emulation.")
  else()
    message(STATUS
      "OpenMS Thermo Bridge: using pre-built managed artifacts from '${_openms_thermo_bridge_prebuilt_path}'.")
  endif()

  set(OPENMS_THERMO_BRIDGE_MANAGED_DIR "${_openms_thermo_bridge_prebuilt_path}")
  set(OpenMSThermoBridge_MANAGED_DIR "${OPENMS_THERMO_BRIDGE_MANAGED_DIR}" CACHE INTERNAL "Managed runtime directory for OpenMSThermoBridge.")

  # Create a no-op target so the rest of the build graph is unchanged.
  add_custom_target(openms_thermo_bridge_managed ALL)
else()
  # ---------------------------------------------------------------------------
  # Build the managed bridge from source using dotnet publish.
  # ---------------------------------------------------------------------------
  find_program(DOTNET_EXECUTABLE NAMES dotnet REQUIRED)

  set(OPENMS_THERMO_BRIDGE_MANAGED_WARNING_COMMAND)
  if(APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)$" AND OPENMS_THERMO_BRIDGE_OSX_ARM64_X64_WORKAROUND)
    set(OPENMS_THERMO_BRIDGE_MANAGED_WARNING_COMMAND
      COMMAND "${CMAKE_COMMAND}" -E cmake_echo_color --yellow
              "Building osx-x64 managed artifacts for Rosetta compatibility on Apple Silicon; performance may be reduced under emulation.")
  endif()
  set(OPENMS_THERMO_BRIDGE_MANAGED_FILES
      "${OPENMS_THERMO_BRIDGE_MANAGED_DIR}/ThermoWrapperManaged.dll"
      "${OPENMS_THERMO_BRIDGE_MANAGED_DIR}/ThermoWrapperManaged.runtimeconfig.json")
  set(OpenMSThermoBridge_MANAGED_DIR "${OPENMS_THERMO_BRIDGE_MANAGED_DIR}" CACHE INTERNAL "Managed runtime directory for OpenMSThermoBridge.")

  file(TO_CMAKE_PATH "${OPENMS_THERMO_BRIDGE_VENDOR_DIR}" OPENMS_THERMO_BRIDGE_VENDOR_DIR_CMAKE_PATH)
  file(CONFIGURE
    OUTPUT "${OPENMS_THERMO_BRIDGE_NUGET_CONFIG}"
    CONTENT "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<configuration>\n  <packageSources>\n    <clear />\n    <add key=\"openms-thermo-bridge-vendor\" value=\"@OPENMS_THERMO_BRIDGE_VENDOR_DIR_CMAKE_PATH@\" />\n    <add key=\"nuget.org\" value=\"https://api.nuget.org/v3/index.json\" />\n  </packageSources>\n</configuration>\n"
    @ONLY)

  openms_thermo_bridge_vendor_package_names(OPENMS_THERMO_BRIDGE_VENDOR_PACKAGE_NAMES)
  set(OPENMS_THERMO_BRIDGE_VENDOR_PACKAGE_FILES)
  foreach(package_name IN LISTS OPENMS_THERMO_BRIDGE_VENDOR_PACKAGE_NAMES)
    list(APPEND OPENMS_THERMO_BRIDGE_VENDOR_PACKAGE_FILES "${OPENMS_THERMO_BRIDGE_VENDOR_DIR}/${package_name}")
  endforeach()

  add_custom_target(openms_thermo_bridge_vendor_packages
    COMMAND "${CMAKE_COMMAND}"
            -DOPENMS_THERMO_BRIDGE_ENABLE_VENDOR_DOWNLOAD=${OPENMS_THERMO_BRIDGE_ENABLE_VENDOR_DOWNLOAD}
            -DOPENMS_THERMO_BRIDGE_VENDOR_DIR=${OPENMS_THERMO_BRIDGE_VENDOR_DIR}
            -DOPENMS_THERMO_BRIDGE_THERMO_COMMIT=${OPENMS_THERMO_BRIDGE_THERMO_COMMIT}
            -DOPENMS_THERMO_BRIDGE_THERMO_VERSION=${OPENMS_THERMO_BRIDGE_THERMO_VERSION}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/DownloadThermoPackages.cmake"
    BYPRODUCTS ${OPENMS_THERMO_BRIDGE_VENDOR_PACKAGE_FILES}
    COMMENT "Preparing Thermo vendor packages"
    VERBATIM)

  add_custom_command(
    OUTPUT ${OPENMS_THERMO_BRIDGE_MANAGED_FILES}
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${OPENMS_THERMO_BRIDGE_MANAGED_DIR}"
    ${OPENMS_THERMO_BRIDGE_MANAGED_WARNING_COMMAND}
    COMMAND "${DOTNET_EXECUTABLE}" publish "${CMAKE_CURRENT_SOURCE_DIR}/ThermoWrapperManaged.csproj"
            -c Release
            --nologo
            --self-contained false
            ${OPENMS_THERMO_BRIDGE_DOTNET_PUBLISH_ARGS}
            -o "${OPENMS_THERMO_BRIDGE_MANAGED_DIR}"
            --configfile "${OPENMS_THERMO_BRIDGE_NUGET_CONFIG}"
    DEPENDS openms_thermo_bridge_vendor_packages
            "${CMAKE_CURRENT_SOURCE_DIR}/ThermoWrapperManaged.csproj"
            "${CMAKE_CURRENT_SOURCE_DIR}/ThermoWrapperManaged.cs"
            "${OPENMS_THERMO_BRIDGE_NUGET_CONFIG}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Publishing managed Thermo bridge"
    VERBATIM)

  add_custom_target(openms_thermo_bridge_managed ALL DEPENDS ${OPENMS_THERMO_BRIDGE_MANAGED_FILES})
endif()

add_library(openms_thermo_bridge src/thermo_bridge.cpp src/cv_mapping.cpp)
add_library(OpenMSThermoBridge::openms_thermo_bridge ALIAS openms_thermo_bridge)

target_compile_features(openms_thermo_bridge PUBLIC cxx_std_17)
target_include_directories(openms_thermo_bridge
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(openms_thermo_bridge PUBLIC DotNetHost::nethost)
if(NOT WIN32)
  target_link_libraries(openms_thermo_bridge PRIVATE ${CMAKE_DL_LIBS})
endif()
add_dependencies(openms_thermo_bridge openms_thermo_bridge_managed)

if(OPENMS_THERMO_BRIDGE_BUILD_CLI)
  add_executable(thermo_host src/thermo_host_main.cpp)
  target_link_libraries(thermo_host PRIVATE openms_thermo_bridge)
  openms_thermo_bridge_copy_runtime_files(TARGET thermo_host)
  install(TARGETS thermo_host RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

if(BUILD_TESTING)
  find_package(Catch2 3 CONFIG QUIET)
  if(NOT Catch2_FOUND)
    include(FetchContent)
    FetchContent_Declare(
      Catch2
      GIT_REPOSITORY https://github.com/catchorg/Catch2.git
      GIT_TAG v3.8.1)
    FetchContent_MakeAvailable(Catch2)
  endif()

  file(MAKE_DIRECTORY "${OPENMS_THERMO_BRIDGE_TEST_DATA_DIR}")
  set(OPENMS_THERMO_BRIDGE_SAMPLE_RAW_FILE "${OPENMS_THERMO_BRIDGE_TEST_DATA_DIR}/ginkgotoxin-ms-positive.raw")
  if(OPENMS_THERMO_BRIDGE_DOWNLOAD_TEST_DATA)
    file(DOWNLOAD
      "${OPENMS_THERMO_BRIDGE_SAMPLE_RAW_URL}"
      "${OPENMS_THERMO_BRIDGE_SAMPLE_RAW_FILE}"
      STATUS OPENMS_THERMO_BRIDGE_SAMPLE_DOWNLOAD_STATUS
      TLS_VERIFY ON)
    list(GET OPENMS_THERMO_BRIDGE_SAMPLE_DOWNLOAD_STATUS 0 OPENMS_THERMO_BRIDGE_SAMPLE_DOWNLOAD_CODE)
    list(GET OPENMS_THERMO_BRIDGE_SAMPLE_DOWNLOAD_STATUS 1 OPENMS_THERMO_BRIDGE_SAMPLE_DOWNLOAD_MESSAGE)
    if(NOT OPENMS_THERMO_BRIDGE_SAMPLE_DOWNLOAD_CODE EQUAL 0)
      message(FATAL_ERROR "Failed to download sample RAW file: ${OPENMS_THERMO_BRIDGE_SAMPLE_DOWNLOAD_MESSAGE}")
    endif()
  endif()

  add_executable(thermo_bridge_tests tests/test_thermo_bridge.cpp)
  target_compile_features(thermo_bridge_tests PRIVATE cxx_std_17)
  target_link_libraries(thermo_bridge_tests PRIVATE openms_thermo_bridge Catch2::Catch2WithMain)
  target_compile_definitions(thermo_bridge_tests PRIVATE
    OPENMS_THERMO_BRIDGE_TEST_SAMPLE_RAW="$<IF:$<BOOL:${OPENMS_THERMO_BRIDGE_DOWNLOAD_TEST_DATA}>,${OPENMS_THERMO_BRIDGE_SAMPLE_RAW_FILE},>"
    OPENMS_THERMO_BRIDGE_TEST_EXPECTED_SCAN_COUNT=90)
  openms_thermo_bridge_copy_runtime_files(TARGET thermo_bridge_tests)

  add_executable(cv_mapping_tests tests/test_cv_mapping.cpp)
  target_compile_features(cv_mapping_tests PRIVATE cxx_std_17)
  target_link_libraries(cv_mapping_tests PRIVATE openms_thermo_bridge Catch2::Catch2WithMain)

  include(Catch)
  catch_discover_tests(thermo_bridge_tests)
  catch_discover_tests(cv_mapping_tests)
endif()

install(TARGETS openms_thermo_bridge EXPORT OpenMSThermoBridgeTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/" DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY "${OPENMS_THERMO_BRIDGE_MANAGED_DIR}/" DESTINATION "${CMAKE_INSTALL_LIBDIR}/openms_thermo_bridge/managed")

install(EXPORT OpenMSThermoBridgeTargets
  FILE OpenMSThermoBridgeTargets.cmake
  NAMESPACE OpenMSThermoBridge::
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/OpenMSThermoBridge")

set(OPENMS_THERMO_BRIDGE_CONFIG_MANAGED_DIR "\${PACKAGE_PREFIX_DIR}/${CMAKE_INSTALL_LIBDIR}/openms_thermo_bridge/managed")
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/OpenMSThermoBridgeHelpers.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/OpenMSThermoBridgeHelpers.cmake"
  @ONLY)

configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/OpenMSThermoBridgeConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/OpenMSThermoBridgeConfig.cmake"
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/OpenMSThermoBridge")

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/OpenMSThermoBridgeConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion)

install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/OpenMSThermoBridgeConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/OpenMSThermoBridgeConfigVersion.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/OpenMSThermoBridgeHelpers.cmake"
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/FindDotNetHost.cmake"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/OpenMSThermoBridge")
