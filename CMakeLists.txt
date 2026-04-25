cmake_minimum_required(VERSION 3.21)

project(OpenMSThermoBridge VERSION 0.1.0 LANGUAGES CXX)

include(CTest)
include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_program(DOTNET_EXECUTABLE NAMES dotnet REQUIRED)
find_package(DotNetHost REQUIRED)

option(OPENMS_THERMO_BRIDGE_BUILD_CLI "Build the thermo_host CLI executable." ON)
option(OPENMS_THERMO_BRIDGE_ENABLE_VENDOR_DOWNLOAD "Download Thermo vendor packages during the build." OFF)
set(OPENMS_THERMO_BRIDGE_VENDOR_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/thermo-feed" CACHE PATH "Directory containing the Thermo vendor .nupkg files.")
set(OPENMS_THERMO_BRIDGE_THERMO_COMMIT "db08ff32cbb626d259b6d4d98492c83d6f2f6871" CACHE STRING "RawFileReader commit used for vendor package snapshots.")
set(OPENMS_THERMO_BRIDGE_THERMO_VERSION "8.0.6" CACHE STRING "Thermo RawFileReader package version.")
set(OPENMS_THERMO_BRIDGE_SAMPLE_RAW_URL "https://raw.githubusercontent.com/HegemanLab/small-thermo-raw-file-examples/cfac6a25b44913273dd458d5f5db9b20b0c1107d/ginkgotoxin-ms-positive.raw" CACHE STRING "Public sample RAW file used by the test suite.")
option(OPENMS_THERMO_BRIDGE_DOWNLOAD_TEST_DATA "Download public sample RAW data when BUILD_TESTING is enabled." ON)

set(OPENMS_THERMO_BRIDGE_MANAGED_DIR "${CMAKE_CURRENT_BINARY_DIR}/openms_thermo_bridge_managed")
set(OPENMS_THERMO_BRIDGE_TEST_DATA_DIR "${CMAKE_CURRENT_BINARY_DIR}/testdata")
set(OPENMS_THERMO_BRIDGE_MANAGED_FILES
    "${OPENMS_THERMO_BRIDGE_MANAGED_DIR}/ThermoWrapperManaged.dll"
    "${OPENMS_THERMO_BRIDGE_MANAGED_DIR}/ThermoWrapperManaged.runtimeconfig.json")
set(OpenMSThermoBridge_MANAGED_DIR "${OPENMS_THERMO_BRIDGE_MANAGED_DIR}" CACHE INTERNAL "Managed runtime directory for OpenMSThermoBridge.")

include(OpenMSThermoBridgeFunctions)

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
  COMMAND "${DOTNET_EXECUTABLE}" publish "${CMAKE_CURRENT_SOURCE_DIR}/ThermoWrapperManaged.csproj"
          -c Release
          --nologo
          --self-contained false
          -o "${OPENMS_THERMO_BRIDGE_MANAGED_DIR}"
          --source "${OPENMS_THERMO_BRIDGE_VENDOR_DIR}"
          --source "https://api.nuget.org/v3/index.json"
  DEPENDS openms_thermo_bridge_vendor_packages
          "${CMAKE_CURRENT_SOURCE_DIR}/ThermoWrapperManaged.csproj"
          "${CMAKE_CURRENT_SOURCE_DIR}/ThermoWrapperManaged.cs"
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  COMMENT "Publishing managed Thermo bridge"
  VERBATIM)

add_custom_target(openms_thermo_bridge_managed ALL DEPENDS ${OPENMS_THERMO_BRIDGE_MANAGED_FILES})

add_library(openms_thermo_bridge src/thermo_bridge.cpp)
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

  include(Catch)
  catch_discover_tests(thermo_bridge_tests)
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
