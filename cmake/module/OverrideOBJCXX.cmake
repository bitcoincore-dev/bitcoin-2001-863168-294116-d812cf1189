# Copyright (c) 2024-present The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/license/mit/.

if(NOT "${APPEND_CPPFLAGS}${APPEND_CXXFLAGS}" STREQUAL "")
  set(CMAKE_OBJCXX_COMPILE_OBJECT
    "<CMAKE_OBJCXX_COMPILER> <DEFINES> <INCLUDES> -x objective-c++ <FLAGS> ${APPEND_CPPFLAGS} ${APPEND_CXXFLAGS} -o <OBJECT> -c <SOURCE>"
  )
endif()
