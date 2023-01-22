# Copyright (c) 2023-present The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/license/mit/.

include(GenerateMPCode)
generate_from_capnp(capnp/echo.capnp)
generate_from_capnp(capnp/init.capnp)

add_library(bitcoin_ipc STATIC EXCLUDE_FROM_ALL
  ${CMAKE_CURRENT_BINARY_DIR}/capnp/echo.capnp.c++
  ${CMAKE_CURRENT_BINARY_DIR}/capnp/echo.capnp.proxy-client.c++
  ${CMAKE_CURRENT_BINARY_DIR}/capnp/echo.capnp.proxy-server.c++
  ${CMAKE_CURRENT_BINARY_DIR}/capnp/echo.capnp.proxy-types.c++
  ${CMAKE_CURRENT_BINARY_DIR}/capnp/init.capnp.c++
  ${CMAKE_CURRENT_BINARY_DIR}/capnp/init.capnp.proxy-client.c++
  ${CMAKE_CURRENT_BINARY_DIR}/capnp/init.capnp.proxy-server.c++
  ${CMAKE_CURRENT_BINARY_DIR}/capnp/init.capnp.proxy-types.c++
  capnp/protocol.cpp
  interfaces.cpp
  process.cpp
)

target_include_directories(bitcoin_ipc
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
)

target_link_libraries(bitcoin_ipc
  PRIVATE
    core_interface
    PkgConfig::libmultiprocess
)
