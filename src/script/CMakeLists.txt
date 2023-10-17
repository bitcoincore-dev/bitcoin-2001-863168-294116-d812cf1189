# Copyright (c) 2023-present The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/license/mit/.

# Must be included before CMAKE_INSTALL_INCLUDEDIR is used.
include(GNUInstallDirs)

add_library(bitcoinconsensus
  bitcoinconsensus.cpp
  $<TARGET_OBJECTS:bitcoin_consensus>
  $<TARGET_OBJECTS:bitcoin_crypto>
  ../support/cleanse.cpp
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
    $<TARGET_NAME_IF_EXISTS:link_ssp>
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
  if(WIN32 AND TARGET bitcoinconsensus AND BUILD_SHARED_LIBS)
    # The DLL must reside either in the same folder where the executable is
    # or somewhere in PATH. Using the former option.
    add_custom_command(
      TARGET ${target} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E create_symlink $<TARGET_FILE:bitcoinconsensus> ${CMAKE_CURRENT_BINARY_DIR}/$<TARGET_FILE_NAME:bitcoinconsensus>
      VERBATIM
    )
  endif()
endfunction()
