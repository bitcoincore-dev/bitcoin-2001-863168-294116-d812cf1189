# Copyright (c) 2023 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

# Optional features and packages.

if(CCACHE)
  find_program(CCACHE_COMMAND ccache)
  mark_as_advanced(CCACHE_COMMAND)
  if(CCACHE_COMMAND)
    if(MSVC)
      if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.24)
        # ccache >= 4.8 requires compile batching turned off that is available since CMake 3.24.
        # See https://github.com/ccache/ccache/wiki/MS-Visual-Studio
        set(MSVC_CCACHE_WRAPPER_CONTENT "\"${CCACHE_COMMAND}\" \"${CMAKE_CXX_COMPILER}\"")
        set(MSVC_CCACHE_WRAPPER_FILENAME wrapped-cl.bat)
        file(WRITE ${CMAKE_BINARY_DIR}/${MSVC_CCACHE_WRAPPER_FILENAME} "${MSVC_CCACHE_WRAPPER_CONTENT} %*")
        list(APPEND CMAKE_VS_GLOBALS
          "CLToolExe=${MSVC_CCACHE_WRAPPER_FILENAME}"
          "CLToolPath=${CMAKE_BINARY_DIR}"
          "DebugInformationFormat=OldStyle"
        )
        set(CMAKE_VS_NO_COMPILE_BATCHING ON)
      else()
        message(FATAL_ERROR "ccache requested and found, but CMake >= 3.24 is required to use it properly.")
      endif()
    else()
      list(APPEND CMAKE_C_COMPILER_LAUNCHER ${CCACHE_COMMAND})
      list(APPEND CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_COMMAND})
    endif()
  else()
    message(FATAL_ERROR "ccache requested, but not found.")
  endif()
endif()

if(WITH_NATPMP)
  find_package(NATPMP MODULE REQUIRED)
endif()

if(WITH_MINIUPNPC)
  find_package(MiniUPnPc MODULE REQUIRED)
endif()

if(WITH_ZMQ)
  if(MSVC)
    find_package(ZeroMQ CONFIG REQUIRED)
  else()
    # The ZeroMQ project has provided config files since v4.2.2.
    # TODO: Switch to find_package(ZeroMQ) at some point in the future.
    include(CrossPkgConfig)
    cross_pkg_check_modules(libzmq REQUIRED IMPORTED_TARGET libzmq>=4)
    target_compile_definitions(PkgConfig::libzmq INTERFACE
      $<$<PLATFORM_ID:Windows>:ZMQ_STATIC>
    )
    target_link_libraries(PkgConfig::libzmq INTERFACE
      $<$<PLATFORM_ID:Windows>:iphlpapi;ws2_32>
    )
  endif()
endif()

if(WITH_USDT)
  find_path(SystemTap_INCLUDE_DIR
    NAMES sys/sdt.h
    REQUIRED
  )
  mark_as_advanced(SystemTap_INCLUDE_DIR)

  include(CMakePushCheckState)
  cmake_push_check_state(RESET)
  include(CheckCXXSourceCompiles)
  set(CMAKE_REQUIRED_INCLUDES ${SystemTap_INCLUDE_DIR})
  check_cxx_source_compiles("
    #include <sys/sdt.h>

    int main()
    {
      DTRACE_PROBE(context, event);
      int a, b, c, d, e, f, g;
      DTRACE_PROBE7(context, event, a, b, c, d, e, f, g);
    }
    " HAVE_USDT_H
  )
  cmake_pop_check_state()

  if(HAVE_USDT_H)
    target_include_directories(core_interface INTERFACE
      ${SystemTap_INCLUDE_DIR}
    )
    set(ENABLE_TRACING TRUE)
  else()
    message(FATAL_ERROR "sys/sdt.h found, but could not be compiled.")
  endif()
endif()

if(ENABLE_WALLET)
  if(WITH_SQLITE)
    if(VCPKG_TARGET_TRIPLET)
      # Use of the `unofficial::` namespace is a vcpkg package manager convention.
      find_package(unofficial-sqlite3 CONFIG REQUIRED)
    else()
      find_package(SQLite3 3.7.17 REQUIRED)
    endif()
    set(USE_SQLITE ON)
  endif()

  if(WITH_BDB)
    find_package(BerkeleyDB 4.8 MODULE REQUIRED)
    set(USE_BDB ON)
    if(NOT BerkeleyDB_VERSION VERSION_EQUAL 4.8)
      message(WARNING "Found Berkeley DB (BDB) other than 4.8.")
      if(WARN_INCOMPATIBLE_BDB)
        message(WARNING "BDB (legacy) wallets opened by this build would not be portable!\n"
                        "If this is intended, pass \"-DWARN_INCOMPATIBLE_BDB=OFF\".\n"
                        "Passing \"-DWITH_BDB=OFF\" will suppress this warning.\n")
      else()
        message(WARNING "BDB (legacy) wallets opened by this build will not be portable!")
      endif()
    endif()
  endif()
endif()
