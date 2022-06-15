# Copyright (c) 2022 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

set(WITH_NATPMP "auto" CACHE STRING "Enable NAT-PMP ([auto], yes, no). \"auto\" means \"yes\" if libnatpmp is found")
option(ENABLE_NATPMP_DEFAULT "If NAT-PMP is enabled, turn it on at startup" OFF)

set(OPTIONAL_PACKAGE_OPTION_VALUES auto yes no)
foreach(optional_package_option WITH_NATPMP)
  if(NOT ${optional_package_option} IN_LIST OPTIONAL_PACKAGE_OPTION_VALUES)
    message(FATAL_ERROR "${optional_package_option} value is \"${${optional_package_option}}\", but must be one of \"auto\", \"yes\" or \"no\".")
  endif()
endforeach()

if(NOT WITH_NATPMP STREQUAL no)
  find_library(LIBNATPMP_LIBRARY natpmp)
  if(LIBNATPMP_LIBRARY STREQUAL LIBNATPMP_LIBRARY-NOTFOUND)
    if(WITH_NATPMP STREQUAL yes)
      message(FATAL_ERROR "libnatpmp requested, but not found.")
    else()
      message(WARNING "libnatpmp not found, disabling.\nTo skip libnatpmp check, use \"-DWITH_NATPMP=no\".")
    endif()
  else()
    message(STATUS "Found libnatpmp: ${LIBNATPMP_LIBRARY}")
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
