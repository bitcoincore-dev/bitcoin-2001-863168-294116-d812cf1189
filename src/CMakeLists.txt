# Copyright (c) 2022 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

set(CRC32C_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CRC32C_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(CRC32C_USE_GLOG OFF CACHE BOOL "" FORCE)
add_subdirectory(crc32c)

include(../cmake/subtree-leveldb.cmake)

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


# Stable, backwards-compatible consensus functionality
# also exposed as a shared library.
add_library(bitcoin_consensus STATIC EXCLUDE_FROM_ALL
  arith_uint256.cpp
  consensus/merkle.cpp
  consensus/tx_check.cpp
  hash.cpp
  primitives/block.cpp
  primitives/transaction.cpp
  pubkey.cpp
  script/interpreter.cpp
  script/script.cpp
  script/script_error.cpp
  uint256.cpp
  util/strencodings.cpp
)
target_include_directories(bitcoin_consensus
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)
target_link_libraries(bitcoin_consensus
  PRIVATE
    bitcoin_crypto
    secp256k1
)
