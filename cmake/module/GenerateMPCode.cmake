# Copyright (c) 2023-present The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/license/mit/.

macro(generate_from_capnp capnp_relpath)
  add_custom_command(
    OUTPUT ${capnp_relpath}.c++ ${capnp_relpath}.h ${capnp_relpath}.proxy-client.c++ ${capnp_relpath}.proxy-types.h ${capnp_relpath}.proxy-server.c++ ${capnp_relpath}.proxy-types.c++ ${capnp_relpath}.proxy.h
    COMMAND ${MPGEN_PREFIX}/bin/mpgen ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/${capnp_relpath}
    DEPENDS ${capnp_relpath}
    VERBATIM
  )
endmacro()
