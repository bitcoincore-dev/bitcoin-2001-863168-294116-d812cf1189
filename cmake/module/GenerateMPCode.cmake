# Copyright (c) 2023-present The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/license/mit/.

function(target_capnp_sources target)
  foreach(capnp_file IN LISTS ARGN)
    add_custom_command(
      OUTPUT ${capnp_file}.c++ ${capnp_file}.h ${capnp_file}.proxy-client.c++ ${capnp_file}.proxy-types.h ${capnp_file}.proxy-server.c++ ${capnp_file}.proxy-types.c++ ${capnp_file}.proxy.h
      COMMAND ${MPGEN_PREFIX}/bin/mpgen ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/${capnp_file}
      DEPENDS ${capnp_file}
      VERBATIM
    )
    target_sources(${target}
      PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}/${capnp_file}.c++
        ${CMAKE_CURRENT_BINARY_DIR}/${capnp_file}.proxy-client.c++
        ${CMAKE_CURRENT_BINARY_DIR}/${capnp_file}.proxy-server.c++
        ${CMAKE_CURRENT_BINARY_DIR}/${capnp_file}.proxy-types.c++
    )
  endforeach()

  target_include_directories(${target}
    PUBLIC
      $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
  )

  target_link_libraries(${target}
    PRIVATE
      PkgConfig::libmultiprocess
  )
endfunction()
