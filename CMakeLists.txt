# Copyright (c) 2023 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

# IMPORTANT: Changes which affect binary results may not be quietly gated
#            by CMake version.
#
# Ubuntu 20.04 LTS Focal Fossa, https://wiki.ubuntu.com/Releases, EOSS in April 2025:
#  - CMake 3.16.3, https://packages.ubuntu.com/focal/cmake
#
# Centos Stream 8, EOL in May 2024:
#  - CMake 3.20.2, http://mirror.centos.org/centos/8-stream/AppStream/x86_64/os/Packages/
#
# All policies known to the running version of CMake and introduced
# in the 3.28 version or earlier will be set to use NEW behavior.
# All policies introduced in later versions will be unset.
# See: https://cmake.org/cmake/help/latest/manual/cmake-policies.7.html
cmake_minimum_required(VERSION 3.16...3.28)

set(PACKAGE_NAME "Bitcoin Core")
set(CLIENT_VERSION_MAJOR 27)
set(CLIENT_VERSION_MINOR 99)
set(CLIENT_VERSION_BUILD 0)
set(CLIENT_VERSION_RC 0)
set(CLIENT_VERSION_IS_RELEASE "false")
set(COPYRIGHT_YEAR "2024")

project(BitcoinCore
  VERSION ${CLIENT_VERSION_MAJOR}.${CLIENT_VERSION_MINOR}.${CLIENT_VERSION_BUILD}
  DESCRIPTION "Bitcoin client software"
  HOMEPAGE_URL "https://bitcoincore.org/"
  LANGUAGES CXX ASM
)

set(PACKAGE_VERSION ${PROJECT_VERSION})
if(CLIENT_VERSION_RC GREATER 0)
  string(APPEND PACKAGE_VERSION "rc${CLIENT_VERSION_RC}")
endif()

set(COPYRIGHT_HOLDERS "The %s developers")
set(COPYRIGHT_HOLDERS_FINAL "The ${PACKAGE_NAME} developers")
set(PACKAGE_BUGREPORT "https://github.com/bitcoin/bitcoin/issues")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake/module)

set(configure_warnings)

# The core_interface library aims to encapsulate common build flags.
# It is intended to be a usage requirement for all other targets.
add_library(core_interface INTERFACE)

message("\n")
message("Configure summary")
message("=================")
get_directory_property(definitions COMPILE_DEFINITIONS)
string(REPLACE ";" " " definitions "${definitions}")
message("Preprocessor defined macros ........... ${definitions}")
message("C compiler ............................ ${CMAKE_C_COMPILER}")
message("CFLAGS ................................ ${CMAKE_C_FLAGS}")
message("C++ compiler .......................... ${CMAKE_CXX_COMPILER}")
message("CXXFLAGS .............................. ${CMAKE_CXX_FLAGS}")
include(GetTargetInterface)
get_target_interface(common_compile_options core_interface COMPILE_OPTIONS)
message("Common compile options ................ ${common_compile_options}")
get_target_interface(common_link_options core_interface LINK_OPTIONS)
message("Common link options ................... ${common_link_options}")
message("Linker flags for executables .......... ${CMAKE_EXE_LINKER_FLAGS}")
message("Linker flags for shared libraries ..... ${CMAKE_SHARED_LINKER_FLAGS}")
if(DEFINED CMAKE_BUILD_TYPE)
  message("Build type:")
  message(" - CMAKE_BUILD_TYPE ................... ${CMAKE_BUILD_TYPE}")
  string(TOUPPER "${CMAKE_BUILD_TYPE}" build_type)
  message(" - CFLAGS ............................. ${CMAKE_C_FLAGS_${build_type}}")
  message(" - CXXFLAGS ........................... ${CMAKE_CXX_FLAGS_${build_type}}")
  message(" - LDFLAGS for executables ............ ${CMAKE_EXE_LINKER_FLAGS_${build_type}}")
  message(" - LDFLAGS for shared libraries ....... ${CMAKE_SHARED_LINKER_FLAGS_${build_type}}")
else()
  message("Available configurations .............. ${CMAKE_CONFIGURATION_TYPES}")
  message("Debug configuration:")
  message(" - CFLAGS ............................. ${CMAKE_C_FLAGS_DEBUG}")
  message(" - CXXFLAGS ........................... ${CMAKE_CXX_FLAGS_DEBUG}")
  message(" - LDFLAGS for executables ............ ${CMAKE_EXE_LINKER_FLAGS_DEBUG}")
  message(" - LDFLAGS for shared libraries ....... ${CMAKE_SHARED_LINKER_FLAGS_DEBUG}")
  message("Release configuration:")
  message(" - CFLAGS ............................. ${CMAKE_C_FLAGS_RELEASE}")
  message(" - CXXFLAGS ........................... ${CMAKE_CXX_FLAGS_RELEASE}")
  message(" - LDFLAGS for executables ............ ${CMAKE_EXE_LINKER_FLAGS_RELEASE}")
  message(" - LDFLAGS for shared libraries ....... ${CMAKE_SHARED_LINKER_FLAGS_RELEASE}")
endif()
message("\n")
if(configure_warnings)
    message("  ******\n")
    foreach(warning IN LISTS configure_warnings)
      message(WARNING "${warning}")
    endforeach()
    message("  ******\n")
endif()

# We want all build properties to be encapsulated properly.
include(WarnAboutGlobalProperties)
