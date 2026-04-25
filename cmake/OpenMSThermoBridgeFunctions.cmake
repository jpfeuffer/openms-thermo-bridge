include(CMakeParseArguments)

function(openms_thermo_bridge_vendor_package_names out_var)
  set(${out_var}
    "ThermoFisher.CommonCore.BackgroundSubtraction.${OPENMS_THERMO_BRIDGE_THERMO_VERSION}.nupkg"
    "ThermoFisher.CommonCore.Data.${OPENMS_THERMO_BRIDGE_THERMO_VERSION}.nupkg"
    "ThermoFisher.CommonCore.MassPrecisionEstimator.${OPENMS_THERMO_BRIDGE_THERMO_VERSION}.nupkg"
    "ThermoFisher.CommonCore.RandomAccessReaderPlugin.${OPENMS_THERMO_BRIDGE_THERMO_VERSION}.nupkg"
    "ThermoFisher.CommonCore.RawfileReader.${OPENMS_THERMO_BRIDGE_THERMO_VERSION}.nupkg"
    PARENT_SCOPE)
endfunction()

function(openms_thermo_bridge_copy_runtime_files)
  set(options)
  set(one_value_args TARGET MANAGED_DIR)
  cmake_parse_arguments(ARG "${options}" "${one_value_args}" "" ${ARGN})

  if(NOT ARG_TARGET)
    message(FATAL_ERROR "openms_thermo_bridge_copy_runtime_files requires TARGET")
  endif()
  if(NOT ARG_MANAGED_DIR)
    set(ARG_MANAGED_DIR "${OpenMSThermoBridge_MANAGED_DIR}")
  endif()
  if(NOT ARG_MANAGED_DIR)
    message(FATAL_ERROR "openms_thermo_bridge_copy_runtime_files requires MANAGED_DIR or OpenMSThermoBridge_MANAGED_DIR")
  endif()

  add_custom_command(TARGET ${ARG_TARGET} POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E rm -rf "$<TARGET_FILE_DIR:${ARG_TARGET}>/managed"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:${ARG_TARGET}>/managed"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${ARG_MANAGED_DIR}" "$<TARGET_FILE_DIR:${ARG_TARGET}>/managed"
    COMMENT "Copying managed bridge runtime files for ${ARG_TARGET}"
    VERBATIM)
endfunction()
