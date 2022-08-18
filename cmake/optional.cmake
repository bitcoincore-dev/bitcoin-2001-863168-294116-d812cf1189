# Copyright (c) 2022 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

# Optional features and packages.

if(CCACHE)
  find_program(PROG_CCACHE ccache)
  if(PROG_CCACHE)
    set(CMAKE_C_COMPILER_LAUNCHER ${PROG_CCACHE})
    set(CMAKE_CXX_COMPILER_LAUNCHER ${PROG_CCACHE})
  elseif(CCACHE STREQUAL ON)
    message(FATAL_ERROR "ccache requested, but not found.")
  endif()
  mark_as_advanced(PROG_CCACHE)
endif()
