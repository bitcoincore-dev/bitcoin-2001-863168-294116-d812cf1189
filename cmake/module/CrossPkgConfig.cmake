# Copyright (c) 2023-present The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/license/mit/.

find_package(PkgConfig REQUIRED)

function(cross_pkg_check_modules prefix)
  if(CMAKE_CROSSCOMPILING)
    set(pkg_config_path_saved "$ENV{PKG_CONFIG_PATH}")
    set(pkg_config_libdir_saved "$ENV{PKG_CONFIG_LIBDIR}")
    set(ENV{PKG_CONFIG_PATH} ${PKG_CONFIG_PATH})
    set(ENV{PKG_CONFIG_LIBDIR} ${PKG_CONFIG_LIBDIR})
    pkg_check_modules(${prefix} ${ARGN})
    set(ENV{PKG_CONFIG_PATH} ${pkg_config_path_saved})
    set(ENV{PKG_CONFIG_LIBDIR} ${pkg_config_libdir_saved})

    if(TARGET PkgConfig::${prefix})
      list(REMOVE_ITEM ${prefix}_STATIC_LIBRARIES ${${prefix}_LIBRARIES})
      target_link_libraries(PkgConfig::${prefix} INTERFACE
        ${${prefix}_STATIC_LIBRARIES}
      )
    endif()
  else()
    pkg_check_modules(${prefix} ${ARGN})
  endif()
endfunction()
