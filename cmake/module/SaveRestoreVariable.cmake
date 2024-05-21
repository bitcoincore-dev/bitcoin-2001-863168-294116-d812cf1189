# Copyright (c) 2024-present The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/license/mit/.

include_guard(GLOBAL)

macro(save_variable var)
  unset(_saved_${var})
  if(DEFINED ${var})
    set(_saved_${var} ${${var}})
  endif()
endmacro()

macro(restore_variable var)
  if(DEFINED _saved_${var})
    set(${var} ${_saved_${var}})
    unset(_saved_${var})
  else()
    unset(${var})
  endif()
endmacro()
