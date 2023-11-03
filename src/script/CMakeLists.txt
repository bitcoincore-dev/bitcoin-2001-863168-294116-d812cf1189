# Copyright (c) 2023-present The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/license/mit/.

# Must be included before CMAKE_INSTALL_INCLUDEDIR is used.
include(GNUInstallDirs)

add_library(bitcoinconsensus
  ../support/cleanse.cpp
  bitcoinconsensus.cpp
  ${bitcoin_crypto_base_sources}
  ${bitcoin_consensus_sources}
)
target_compile_definitions(bitcoinconsensus
  PRIVATE
    BUILD_BITCOIN_INTERNAL
)
target_include_directories(bitcoinconsensus
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(bitcoinconsensus
  PRIVATE
    core
    secp256k1
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
install(FILES bitcoinconsensus.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

include(GeneratePkgConfigFile)
generate_pkg_config_file(${PROJECT_SOURCE_DIR}/libbitcoinconsensus.pc.in libbitcoinconsensus.pc)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libbitcoinconsensus.pc
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)

function(make_bitcoinconsensus_dll_available target)
  if(WIN32 AND BUILD_SHARED_LIBS)
    # The DLL must reside either in the same folder where the executable is
    # or somewhere in PATH. Using the former option.
    add_custom_command(
      TARGET ${target} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E create_symlink $<TARGET_FILE:bitcoinconsensus> $<TARGET_FILE_DIR:${target}>/$<TARGET_FILE_NAME:bitcoinconsensus>
      VERBATIM
    )
  endif()
endfunction()
