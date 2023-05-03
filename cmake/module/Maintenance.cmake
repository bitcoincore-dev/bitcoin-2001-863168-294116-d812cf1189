# Copyright (c) 2023-present The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/license/mit/.

include_guard(GLOBAL)

function(setup_split_debug_script)
  if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    set(OBJCOPY ${CMAKE_OBJCOPY})
    set(STRIP ${CMAKE_STRIP})
    if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.20)
      configure_file(
        contrib/devtools/split-debug.sh.in split-debug.sh
        FILE_PERMISSIONS OWNER_READ OWNER_EXECUTE
                         GROUP_READ GROUP_EXECUTE
                         WORLD_READ
        @ONLY
      )
    else()
      configure_file(
        contrib/devtools/split-debug.sh.in split-debug.sh
        @ONLY
      )
    endif()
  endif()
endfunction()

function(add_maintenance_targets)
  if(NOT PYTHON_COMMAND)
    return()
  endif()

  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(exe_format MACHO)
  elseif(WIN32)
    set(exe_format PE)
  else()
    set(exe_format ELF)
  endif()
  if(CMAKE_CROSSCOMPILING)
    list(JOIN DEPENDS_C_COMPILER_WITH_LAUNCHER " " c_compiler_command)
  else()
    set(c_compiler_command ${CMAKE_C_COMPILER})
  endif()
  add_custom_target(test-security-check
    COMMAND ${CMAKE_COMMAND} -E env CC=${c_compiler_command} CFLAGS=${CMAKE_C_FLAGS} LDFLAGS=${CMAKE_EXE_LINKER_FLAGS} ${PYTHON_COMMAND} ${CMAKE_SOURCE_DIR}/contrib/devtools/test-security-check.py TestSecurityChecks.test_${exe_format}
    COMMAND ${CMAKE_COMMAND} -E env CC=${c_compiler_command} CFLAGS=${CMAKE_C_FLAGS} LDFLAGS=${CMAKE_EXE_LINKER_FLAGS} ${PYTHON_COMMAND} ${CMAKE_SOURCE_DIR}/contrib/devtools/test-symbol-check.py TestSymbolChecks.test_${exe_format}
    VERBATIM
  )

  foreach(target IN ITEMS bitcoind bitcoin-cli bitcoin-tx bitcoin-util bitcoin-wallet test_bitcoin bench_bitcoin)
    if(TARGET ${target})
      # Not using the TARGET_FILE generator expression because it creates excessive
      # target-level dependencies in the following custom targets.
      list(APPEND executables $<TARGET_FILE_DIR:${target}>/$<TARGET_FILE_NAME:${target}>)
    endif()
  endforeach()

  add_custom_target(check-symbols
    COMMAND ${CMAKE_COMMAND} -E echo "Running symbol and dynamic library checks..."
    COMMAND ${PYTHON_COMMAND} ${CMAKE_SOURCE_DIR}/contrib/devtools/symbol-check.py ${executables}
    VERBATIM
  )

  if(HARDENING)
    add_custom_target(check-security
      COMMAND ${CMAKE_COMMAND} -E echo "Checking binary security..."
      COMMAND ${PYTHON_COMMAND} ${CMAKE_SOURCE_DIR}/contrib/devtools/security-check.py ${executables}
      VERBATIM
    )
  else()
    add_custom_target(check-security)
  endif()
endfunction()

function(add_windows_deploy_target)
  if(MINGW AND TARGET bitcoin-qt AND TARGET bitcoind AND TARGET bitcoin-cli AND TARGET bitcoin-tx AND TARGET bitcoin-wallet AND TARGET bitcoin-util AND TARGET test_bitcoin)
    # TODO: Consider replacing this code with the CPack NSIS Generator.
    #       See https://cmake.org/cmake/help/latest/cpack_gen/nsis.html
    include(GenerateSetupNsi)
    generate_setup_nsi()
    add_custom_command(
      OUTPUT ${CMAKE_BINARY_DIR}/bitcoin-win64-setup.exe
      COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/release
      COMMAND ${CMAKE_STRIP} $<TARGET_FILE:bitcoin-qt> -o ${CMAKE_BINARY_DIR}/release/$<TARGET_FILE_NAME:bitcoin-qt>
      COMMAND ${CMAKE_STRIP} $<TARGET_FILE:bitcoind> -o ${CMAKE_BINARY_DIR}/release/$<TARGET_FILE_NAME:bitcoind>
      COMMAND ${CMAKE_STRIP} $<TARGET_FILE:bitcoin-cli> -o ${CMAKE_BINARY_DIR}/release/$<TARGET_FILE_NAME:bitcoin-cli>
      COMMAND ${CMAKE_STRIP} $<TARGET_FILE:bitcoin-tx> -o ${CMAKE_BINARY_DIR}/release/$<TARGET_FILE_NAME:bitcoin-tx>
      COMMAND ${CMAKE_STRIP} $<TARGET_FILE:bitcoin-wallet> -o ${CMAKE_BINARY_DIR}/release/$<TARGET_FILE_NAME:bitcoin-wallet>
      COMMAND ${CMAKE_STRIP} $<TARGET_FILE:bitcoin-util> -o ${CMAKE_BINARY_DIR}/release/$<TARGET_FILE_NAME:bitcoin-util>
      COMMAND ${CMAKE_STRIP} $<TARGET_FILE:test_bitcoin> -o ${CMAKE_BINARY_DIR}/release/$<TARGET_FILE_NAME:test_bitcoin>
      COMMAND makensis -V2 ${CMAKE_BINARY_DIR}/bitcoin-win64-setup.nsi
      VERBATIM
    )
    add_custom_target(deploy DEPENDS ${CMAKE_BINARY_DIR}/bitcoin-win64-setup.exe)
  endif()
endfunction()
