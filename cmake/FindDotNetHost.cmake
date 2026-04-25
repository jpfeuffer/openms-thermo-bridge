include(FindPackageHandleStandardArgs)

if(DEFINED OPENMS_THERMO_BRIDGE_DOTNET_HOST_RID_OVERRIDE
   AND NOT OPENMS_THERMO_BRIDGE_DOTNET_HOST_RID_OVERRIDE STREQUAL "")
  set(_dotnet_host_rid "${OPENMS_THERMO_BRIDGE_DOTNET_HOST_RID_OVERRIDE}")
else()
  set(_dotnet_host_rid)
  if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(ARM64|arm64|aarch64)$")
      set(_dotnet_host_rid win-arm64)
    else()
      set(_dotnet_host_rid win-x64)
    endif()
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    if(CMAKE_OSX_ARCHITECTURES MATCHES "(^|;)x86_64(;|$)")
      set(_dotnet_host_rid osx-x64)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)$")
      set(_dotnet_host_rid osx-arm64)
    else()
      set(_dotnet_host_rid osx-x64)
    endif()
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)$")
      set(_dotnet_host_rid linux-arm64)
    else()
      set(_dotnet_host_rid linux-x64)
    endif()
  endif()
endif()

if(NOT _dotnet_host_rid)
  message(FATAL_ERROR "Unsupported platform for DotNetHost: ${CMAKE_SYSTEM_NAME}/${CMAKE_SYSTEM_PROCESSOR}")
endif()

set(_dotnet_host_roots)
if(APPLE AND _dotnet_host_rid STREQUAL "osx-x64" AND DEFINED ENV{DOTNET_ROOT_X64} AND NOT "$ENV{DOTNET_ROOT_X64}" STREQUAL "")
  file(TO_CMAKE_PATH "$ENV{DOTNET_ROOT_X64}" _dotnet_root_x64_path)
  list(APPEND _dotnet_host_roots "${_dotnet_root_x64_path}")
endif()
if(DEFINED ENV{DOTNET_ROOT} AND NOT "$ENV{DOTNET_ROOT}" STREQUAL "")
  file(TO_CMAKE_PATH "$ENV{DOTNET_ROOT}" _dotnet_root_path)
  list(APPEND _dotnet_host_roots "${_dotnet_root_path}")
endif()
if(WIN32 AND DEFINED ENV{DOTNET_ROOT_x86} AND NOT "$ENV{DOTNET_ROOT_x86}" STREQUAL "")
  file(TO_CMAKE_PATH "$ENV{DOTNET_ROOT_x86}" _dotnet_root_x86_path)
  list(APPEND _dotnet_host_roots "${_dotnet_root_x86_path}")
endif()
foreach(candidate IN ITEMS
    "$ENV{HOME}/.dotnet-x64"
    "$ENV{HOME}/.dotnet/x64"
    "/usr/share/dotnet"
    "/usr/local/share/dotnet"
    "/usr/local/share/dotnet/x64"
    "/opt/homebrew/share/dotnet"
    "/opt/homebrew/share/dotnet/x64"
    "C:/Program Files/dotnet"
    "C:/Program Files (x86)/dotnet")
  file(TO_CMAKE_PATH "${candidate}" candidate_path)
  list(APPEND _dotnet_host_roots "${candidate_path}")
endforeach()
list(REMOVE_DUPLICATES _dotnet_host_roots)

set(_dotnet_host_candidates)
foreach(root IN LISTS _dotnet_host_roots)
  file(GLOB _root_candidates LIST_DIRECTORIES TRUE "${root}/packs/Microsoft.NETCore.App.Host.${_dotnet_host_rid}/*/runtimes/${_dotnet_host_rid}/native")
list(APPEND _dotnet_host_candidates ${_root_candidates})
endforeach()
list(SORT _dotnet_host_candidates COMPARE NATURAL ORDER DESCENDING)

set(DotNetHost_INCLUDE_DIR)
set(DotNetHost_LIBRARY)
set(DotNetHost_RUNTIME_LIBRARY)
foreach(candidate IN LISTS _dotnet_host_candidates)
  if(EXISTS "${candidate}/nethost.h")
    if(WIN32)
      set(_candidate_library "${candidate}/nethost.lib")
      set(_candidate_runtime "${candidate}/nethost.dll")
    elseif(APPLE)
      set(_candidate_library "${candidate}/libnethost.a")
      if(NOT EXISTS "${_candidate_library}")
        set(_candidate_library "${candidate}/libnethost.dylib")
      endif()
      set(_candidate_runtime "${candidate}/libnethost.dylib")
    else()
      set(_candidate_library "${candidate}/libnethost.a")
      if(NOT EXISTS "${_candidate_library}")
        set(_candidate_library "${candidate}/libnethost.so")
      endif()
      set(_candidate_runtime "${candidate}/libnethost.so")
    endif()

    if(EXISTS "${_candidate_library}")
      set(DotNetHost_INCLUDE_DIR "${candidate}")
      set(DotNetHost_LIBRARY "${_candidate_library}")
      if(EXISTS "${_candidate_runtime}" AND NOT DotNetHost_LIBRARY MATCHES "\\.(a|lib)$")
        set(DotNetHost_RUNTIME_LIBRARY "${_candidate_runtime}")
      elseif(WIN32 AND EXISTS "${_candidate_runtime}")
        set(DotNetHost_RUNTIME_LIBRARY "${_candidate_runtime}")
      else()
        unset(DotNetHost_RUNTIME_LIBRARY)
      endif()
      break()
    endif()
  endif()
endforeach()

find_package_handle_standard_args(DotNetHost
  REQUIRED_VARS DotNetHost_INCLUDE_DIR DotNetHost_LIBRARY
  FAIL_MESSAGE "Could not locate the .NET nethost SDK pack for ${_dotnet_host_rid}. For the Apple Silicon Rosetta workaround, install an osx-x64 .NET SDK/runtime and set DOTNET_ROOT_X64 (or DOTNET_ROOT) to that x64 installation.")

if(DotNetHost_FOUND AND NOT TARGET DotNetHost::nethost)
  if(WIN32 AND DotNetHost_RUNTIME_LIBRARY)
    add_library(DotNetHost::nethost SHARED IMPORTED)
    set_target_properties(DotNetHost::nethost PROPERTIES
      IMPORTED_IMPLIB "${DotNetHost_LIBRARY}"
      IMPORTED_LOCATION "${DotNetHost_RUNTIME_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${DotNetHost_INCLUDE_DIR}")
  else()
    add_library(DotNetHost::nethost UNKNOWN IMPORTED)
    set_target_properties(DotNetHost::nethost PROPERTIES
      IMPORTED_LOCATION "${DotNetHost_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${DotNetHost_INCLUDE_DIR}")
  endif()
endif()
