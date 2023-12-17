# Copyright (c) 2023-present The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/license/mit/.

# Must be included before CMAKE_INSTALL_INCLUDEDIR is used.
include(GNUInstallDirs)

if(BUILD_SHARED_LIBS)
  add_library(consensus_for_external STATIC EXCLUDE_FROM_ALL
    ../support/cleanse.cpp
  )
  target_compile_definitions(consensus_for_external
    PUBLIC
      BUILD_BITCOIN_INTERNAL
  )
  target_link_libraries(consensus_for_external
    PRIVATE
      core
      bitcoin_consensus_sources
      bitcoin_crypto_sources
      secp256k1
  )
  add_library(bitcoinconsensus SHARED
    ../script/bitcoinconsensus.cpp
  )
  target_link_libraries(bitcoinconsensus
    PRIVATE
      consensus_for_external
  )
else()
  add_library(bitcoinconsensus STATIC
    ../script/bitcoinconsensus.cpp
    ../support/cleanse.cpp
  )
  target_compile_definitions(bitcoinconsensus
    PRIVATE
      BUILD_BITCOIN_INTERNAL
  )
  target_link_libraries(bitcoinconsensus
    PRIVATE
      bitcoin_consensus_sources
      bitcoin_crypto_sources
      secp256k1
  )
endif()

target_include_directories(bitcoinconsensus
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(bitcoinconsensus
  PRIVATE
    core
    external_lib_interface
)
set_target_properties(bitcoinconsensus PROPERTIES
  SOVERSION 0
  VERSION 0.0.0
)

install(TARGETS bitcoinconsensus
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(FILES ../script/bitcoinconsensus.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

include(GeneratePkgConfigFile)
generate_pkg_config_file(${PROJECT_SOURCE_DIR}/libbitcoinconsensus.pc.in libbitcoinconsensus.pc)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libbitcoinconsensus.pc
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)
