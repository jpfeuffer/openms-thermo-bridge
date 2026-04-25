include(FindPackageHandleStandardArgs)

set(_dotnet_host_rid)
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(ARM64|arm64|aarch64)$")
    set(_dotnet_host_rid win-arm64)
  else()
    set(_dotnet_host_rid win-x64)
  endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)$")
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

if(NOT _dotnet_host_rid)
  message(FATAL_ERROR "Unsupported platform for DotNetHost: ${CMAKE_SYSTEM_NAME}/${CMAKE_SYSTEM_PROCESSOR}")
endif()

set(_dotnet_host_roots)
foreach(candidate IN ITEMS
    "$ENV{DOTNET_ROOT}"
    "$ENV{DOTNET_ROOT(x86)}"
    "/usr/share/dotnet"
    "/usr/local/share/dotnet"
    "/opt/homebrew/share/dotnet"
    "C:/Program Files/dotnet"
    "C:/Program Files (x86)/dotnet")
  if(candidate)
    file(TO_CMAKE_PATH "${candidate}" candidate_path)
    list(APPEND _dotnet_host_roots "${candidate_path}")
  endif()
endforeach()
list(REMOVE_DUPLICATES _dotnet_host_roots)

set(_dotnet_host_candidates)
foreach(root IN LISTS _dotnet_host_roots)
  file(GLOB _root_candidates LIST_DIRECTORIES TRUE "${root}/packs/Microsoft.NETCore.App.Host.${_dotnet_host_rid}/*/runtimes/${_dotnet_host_rid}/native")
  list(APPEND _dotnet_host_candidates ${_root_candidates})
endforeach()
list(SORT _dotnet_host_candidates COMPARE NATURAL ORDER ASCENDING)
list(REVERSE _dotnet_host_candidates)

set(DotNetHost_INCLUDE_DIR)
set(DotNetHost_LIBRARY)
foreach(candidate IN LISTS _dotnet_host_candidates)
  if(EXISTS "${candidate}/nethost.h")
    if(WIN32)
      set(_candidate_library "${candidate}/nethost.lib")
    elseif(APPLE)
      set(_candidate_library "${candidate}/libnethost.dylib")
      if(NOT EXISTS "${_candidate_library}")
        set(_candidate_library "${candidate}/libnethost.a")
      endif()
    else()
      set(_candidate_library "${candidate}/libnethost.so")
      if(NOT EXISTS "${_candidate_library}")
        set(_candidate_library "${candidate}/libnethost.a")
      endif()
    endif()

    if(EXISTS "${_candidate_library}")
      set(DotNetHost_INCLUDE_DIR "${candidate}")
      set(DotNetHost_LIBRARY "${_candidate_library}")
      break()
    endif()
  endif()
endforeach()

find_package_handle_standard_args(DotNetHost
  REQUIRED_VARS DotNetHost_INCLUDE_DIR DotNetHost_LIBRARY
  FAIL_MESSAGE "Could not locate the .NET nethost SDK pack for ${_dotnet_host_rid}")

if(DotNetHost_FOUND AND NOT TARGET DotNetHost::nethost)
  add_library(DotNetHost::nethost UNKNOWN IMPORTED)
  set_target_properties(DotNetHost::nethost PROPERTIES
    IMPORTED_LOCATION "${DotNetHost_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${DotNetHost_INCLUDE_DIR}")
endif()
