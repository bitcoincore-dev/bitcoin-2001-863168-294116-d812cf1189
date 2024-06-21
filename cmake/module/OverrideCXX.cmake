# Copyright (c) 2024-present The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/license/mit/.

if(NOT "${APPEND_CPPFLAGS}${APPEND_CXXFLAGS}" STREQUAL "")
  set(CMAKE_CXX_COMPILE_OBJECT
    "<CMAKE_CXX_COMPILER> <DEFINES> <INCLUDES> <FLAGS> ${APPEND_CPPFLAGS} ${APPEND_CXXFLAGS} -o <OBJECT> -c <SOURCE>"
  )
endif()

if(NOT "${APPEND_LDFLAGS}" STREQUAL "")
  set(CMAKE_CXX_LINK_EXECUTABLE
    "<CMAKE_CXX_COMPILER> <FLAGS> <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> ${APPEND_LDFLAGS} <OBJECTS> -o <TARGET> <LINK_LIBRARIES>"
  )
endif()
