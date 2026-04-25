#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "OpenMSThermoBridge::openms_thermo_bridge" for configuration "Debug"
set_property(TARGET OpenMSThermoBridge::openms_thermo_bridge APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(OpenMSThermoBridge::openms_thermo_bridge PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "CXX"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/lib/libopenms_thermo_bridge.a"
  )

list(APPEND _cmake_import_check_targets OpenMSThermoBridge::openms_thermo_bridge )
list(APPEND _cmake_import_check_files_for_OpenMSThermoBridge::openms_thermo_bridge "${_IMPORT_PREFIX}/lib/libopenms_thermo_bridge.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
