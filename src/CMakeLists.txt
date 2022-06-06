# Copyright (c) 2022 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

configure_file(../cmake/bitcoin-config.h.in config/bitcoin-config.h @ONLY)
include_directories(${CMAKE_BINARY_DIR}/src)
add_definitions(-DHAVE_CONFIG_H)

add_subdirectory(crypto)
add_subdirectory(univalue)
add_subdirectory(util)

# Home for common functionality shared by different executables and libraries.
# Similar to `bitcoin_util` library, but higher-level.
add_library(bitcoin_common STATIC EXCLUDE_FROM_ALL
  base58.cpp
  bech32.cpp
  chainparams.cpp
  coins.cpp
  common/bloom.cpp
  compressor.cpp
  core_read.cpp
  core_write.cpp
  deploymentinfo.cpp
  external_signer.cpp
  init/common.cpp
  key.cpp
  key_io.cpp
  merkleblock.cpp
  net_types.cpp
  netaddress.cpp
  netbase.cpp
  net_permissions.cpp
  outputtype.cpp
  policy/feerate.cpp
  policy/policy.cpp
  protocol.cpp
  psbt.cpp
  rpc/rawtransaction_util.cpp
  rpc/external_signer.cpp
  rpc/util.cpp
  scheduler.cpp
  script/descriptor.cpp
  script/miniscript.cpp
  script/sign.cpp
  script/signingprovider.cpp
  script/standard.cpp
  warnings.cpp
)
target_include_directories(bitcoin_common
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)
target_link_libraries(bitcoin_common
  PRIVATE
    univalue
    secp256k1
)
