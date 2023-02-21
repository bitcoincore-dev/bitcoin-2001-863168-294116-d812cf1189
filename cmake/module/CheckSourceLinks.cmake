# Copyright (c) 2023 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

include(CheckCXXSourceCompiles)

# This avoids running the linker.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

macro(check_cxx_source_links source)
  set(CMAKE_TRY_COMPILE_TARGET_TYPE EXECUTABLE)
  check_cxx_source_compiles("${source}" ${ARGN})
  set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
endmacro()
