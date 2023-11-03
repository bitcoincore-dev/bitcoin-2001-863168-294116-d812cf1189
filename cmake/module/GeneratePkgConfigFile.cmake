# Copyright (c) 2023-present The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/license/mit/.

function(generate_pkg_config_file in_file out_file)
  set(prefix ${CMAKE_INSTALL_PREFIX})
  set(exec_prefix "\${prefix}")
  set(libdir "\${exec_prefix}/${CMAKE_INSTALL_LIBDIR}")
  set(includedir "\${prefix}/${CMAKE_INSTALL_INCLUDEDIR}")
  set(PACKAGE_NAME ${PROJECT_NAME})
  set(PACKAGE_VERSION ${PROJECT_VERSION})
  configure_file(${in_file} ${out_file} @ONLY)
endfunction()
