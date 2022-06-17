# Copyright (c) 2022 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

# Optional features and packages.

set(USE_CCACHE "auto" CACHE STRING "Use ccache for building ([auto], yes, no). \"auto\" means \"yes\" if ccache is found")

set(WITH_NATPMP "auto" CACHE STRING "Enable NAT-PMP ([auto], yes, no). \"auto\" means \"yes\" if libnatpmp is found")
option(ENABLE_NATPMP_DEFAULT "If NAT-PMP is enabled, turn it on at startup" OFF)

set(WITH_MINIUPNPC "auto" CACHE STRING "Enable UPNP ([auto], yes, no). \"auto\" means \"yes\" if libminiupnpc is found")
option(ENABLE_UPNP_DEFAULT "If UPNP is enabled, turn it on at startup" OFF)

set(WITH_ZMQ "auto" CACHE STRING "Enable ZMQ notifications ([auto], yes, no). \"auto\" means \"yes\" if libzmq is found")

set(OPTION_VALUES auto yes no)
foreach(option USE_CCACHE WITH_NATPMP WITH_MINIUPNPC WITH_ZMQ)
  if(NOT ${option} IN_LIST OPTION_VALUES)
    message(FATAL_ERROR "${option} value is \"${${option}}\", but must be one of \"auto\", \"yes\" or \"no\".")
  endif()
endforeach()

if(NOT USE_CCACHE STREQUAL no)
  find_program(CCACHE ccache)
  if(CCACHE)
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE})
    add_compile_options(-fmacro-prefix-map=${CMAKE_SOURCE_DIR}=. -fdebug-prefix-map=${CMAKE_SOURCE_DIR}=.)
  elseif(USE_CCACHE STREQUAL yes)
    message(FATAL_ERROR "ccache requested, but not found.")
  endif()
endif()

if(NOT WITH_NATPMP STREQUAL no)
  find_library(LIBNATPMP_LIBRARY natpmp)
  if(LIBNATPMP_LIBRARY STREQUAL LIBNATPMP_LIBRARY-NOTFOUND)
    if(WITH_NATPMP STREQUAL yes)
      message(FATAL_ERROR "libnatpmp requested, but not found.")
    else()
      message(WARNING "libnatpmp not found, disabling.\nTo skip libnatpmp check, use \"-DWITH_NATPMP=no\".")
      set(WITH_NATPMP no)
    endif()
  else()
    message(STATUS "Found libnatpmp: ${LIBNATPMP_LIBRARY}")
    set(WITH_NATPMP yes)
    if(ENABLE_NATPMP_DEFAULT)
      set(USE_NATPMP 1)
    else()
      set(USE_NATPMP 0)
    endif()
    add_library(natpmp UNKNOWN IMPORTED)
    set_target_properties(natpmp PROPERTIES
      IMPORTED_LOCATION ${LIBNATPMP_LIBRARY}
      INTERFACE_COMPILE_DEFINITIONS USE_NATPMP=${USE_NATPMP}
    )
    if(CMAKE_SYSTEM_NAME STREQUAL Windows)
      set_property(
        TARGET natpmp
        APPEND
        PROPERTY INTERFACE_COMPILE_DEFINITIONS NATPMP_STATICLIB
      )
    endif()
  endif()
endif()

if(NOT WITH_MINIUPNPC STREQUAL no)
  find_library(LIBMINIUPNPC_LIBRARY miniupnpc)
  if(LIBMINIUPNPC_LIBRARY STREQUAL LIBMINIUPNPC_LIBRARY-NOTFOUND)
    if(WITH_MINIUPNPC STREQUAL yes)
      message(FATAL_ERROR "libminiupnpc requested, but not found.")
    else()
      message(WARNING "libminiupnpc not found, disabling.\nTo skip libminiupnpc check, use \"-DWITH_MINIUPNPC=no\".")
      set(WITH_MINIUPNPC no)
    endif()
  else()
    message(STATUS "Found libminiupnpc: ${LIBMINIUPNPC_LIBRARY}")
    set(WITH_MINIUPNPC yes)
    if(ENABLE_UPNP_DEFAULT)
      set(USE_UPNP 1)
    else()
      set(USE_UPNP 0)
    endif()
    add_library(miniupnpc UNKNOWN IMPORTED)
    set_target_properties(miniupnpc PROPERTIES
      IMPORTED_LOCATION ${LIBMINIUPNPC_LIBRARY}
      INTERFACE_COMPILE_DEFINITIONS USE_UPNP=${USE_UPNP}
    )
    if(CMAKE_SYSTEM_NAME STREQUAL Windows)
      set_property(
        TARGET miniupnpc
        APPEND
        PROPERTY INTERFACE_COMPILE_DEFINITIONS MINIUPNP_STATICLIB
      )
    endif()
  endif()
endif()

include(FindPkgConfig)
if(NOT WITH_ZMQ STREQUAL no)
  pkg_check_modules(libzmq REQUIRED libzmq>=4 IMPORTED_TARGET)
  if(libzmq_FOUND)
    set(WITH_ZMQ yes)
  else()
    if(WITH_ZMQ STREQUAL yes)
      message(FATAL_ERROR "libzmq requested, but not found.")
    else()
      message(WARNING "libzmq not found, disabling.\nTo skip libzmq check, use \"-DWITH_ZMQ=no\".")
      set(WITH_ZMQ no)
    endif()
  endif()
endif()
