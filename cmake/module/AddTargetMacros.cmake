# Copyright (c) 2024-present The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/license/mit/.

include_guard(GLOBAL)

# Functions in this module are drop-in replacements for CMake's add_library
# and add_executable functions. They are mandatory for use in the Bitcoin
# Core project, except for imported and interface libraries.

function(subtree_add_library name)
  if(NOT name MATCHES "^crc32c|^leveldb|^minisketch|^secp256k1")
    message(FATAL_ERROR "${CMAKE_CURRENT_FUNCTION}() was called to add a non-subtree target \"${name}\".")
  endif()
  cmake_parse_arguments(PARSE_ARGV 1 FWD "" "" "")
  add_library("${name}" ${FWD_UNPARSED_ARGUMENTS})
  target_link_libraries("${name}" PRIVATE core_base_interface)
  cmake_language(EVAL CODE "
    cmake_language(DEFER
      DIRECTORY ${PROJECT_SOURCE_DIR}
      CALL target_link_libraries ${name} PRIVATE extra_flags_interface
    )
  ")
endfunction()

function(bitcoincore_add_library name)
  if(name MATCHES "^crc32c|^leveldb|^minisketch|^secp256k1")
    message(FATAL_ERROR "${CMAKE_CURRENT_FUNCTION}() was called to add a subtree target \"${name}\".")
  endif()
  cmake_parse_arguments(PARSE_ARGV 1 FWD "" "" "")
  add_library("${name}" ${FWD_UNPARSED_ARGUMENTS})
  target_link_libraries("${name}" PRIVATE core_interface)
  cmake_language(EVAL CODE "
    cmake_language(DEFER
      DIRECTORY ${PROJECT_SOURCE_DIR}
      CALL target_link_libraries ${name} PRIVATE extra_flags_interface
    )
  ")
endfunction()

function(bitcoincore_add_executable name)
  cmake_parse_arguments(PARSE_ARGV 1 FWD "" "" "")
  add_executable("${name}" ${FWD_UNPARSED_ARGUMENTS})
  target_link_libraries("${name}" PRIVATE core_interface)
  cmake_language(EVAL CODE "
    cmake_language(DEFER
      DIRECTORY ${PROJECT_SOURCE_DIR}
      CALL target_link_libraries ${name} PRIVATE extra_flags_interface
    )
  ")
endfunction()
