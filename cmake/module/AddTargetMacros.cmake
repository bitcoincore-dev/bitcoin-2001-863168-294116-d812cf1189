# Copyright (c) 2024-present The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/license/mit/.

include_guard(GLOBAL)

function(bitcoincore_add_library name)
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
