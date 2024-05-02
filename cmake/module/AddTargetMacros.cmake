# Copyright (c) 2024-present The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/license/mit/.

include_guard(GLOBAL)

# Functions in this module are drop-in replacements for CMake's add_library
# and add_executable functions. They are mandatory for use in the Bitcoin
# Core project (except for imported and interface libraries), because they
# handle APPEND_CPPFLAGS, APPEND_CFLAGS, APPEND_CXXFLAGS and APPEND_LDFLAGS.

macro(target_append_flags_interface target)
  # The following command links append_flags_interface to a given target
  # at the very end of the configuring process, which ensures that
  # APPEND_CPPFLAGS, APPEND_CFLAGS, APPEND_CXXFLAGS and APPEND_LDFLAGS will
  # be added last making it possible to negate particular options added by
  # the build system.
  cmake_language(EVAL CODE "
    cmake_language(DEFER
      DIRECTORY ${PROJECT_SOURCE_DIR}
      CALL target_link_libraries ${target} PRIVATE append_flags_interface
    )
  ")
endmacro()

function(add_library_append_flags name)
  cmake_parse_arguments(PARSE_ARGV 1 FWD "" "" "")
  add_library("${name}" ${FWD_UNPARSED_ARGUMENTS})
  if(name MATCHES "^crc32c|^leveldb|^minisketch|^secp256k1")
    target_link_libraries("${name}" PRIVATE core_base_interface)
  else()
    target_link_libraries("${name}" PRIVATE core_interface)
  endif()
  target_append_flags_interface("${name}")
endfunction()
