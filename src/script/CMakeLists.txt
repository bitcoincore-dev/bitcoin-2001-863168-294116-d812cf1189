# Copyright (c) 2022 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

add_library(bitcoinconsensus SHARED "")
target_sources(bitcoinconsensus
  PRIVATE
    bitcoinconsensus.cpp
    ../support/cleanse.cpp
)
target_compile_definitions(bitcoinconsensus
  PRIVATE
    BUILD_BITCOIN_INTERNAL
)
target_include_directories(bitcoinconsensus
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
)
target_link_libraries(bitcoinconsensus
  PRIVATE
    bitcoin_consensus
)
set_target_properties(bitcoinconsensus PROPERTIES
  PUBLIC_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/bitcoinconsensus.h"
)

set(prefix ${CMAKE_INSTALL_PREFIX})
set(exec_prefix "\${prefix}")
set(libdir "\${exec_prefix}/lib")
set(includedir "\${prefix}/include")
configure_file(${CMAKE_SOURCE_DIR}/libbitcoinconsensus.pc.in libbitcoinconsensus.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libbitcoinconsensus.pc DESTINATION lib/pkgconfig)
