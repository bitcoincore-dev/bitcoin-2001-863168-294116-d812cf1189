# Copyright (c) 2023-present The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/license/mit/.

target_sources(test_bitcoin
  PRIVATE
# Fixtures:
    init_test_fixture.cpp
    wallet_test_fixture.cpp
# Tests:
    coinselector_tests.cpp
    $<$<BOOL:${WITH_BDB}>:${CMAKE_CURRENT_SOURCE_DIR}/db_tests.cpp>
    feebumper_tests.cpp
    group_outputs_tests.cpp
    init_tests.cpp
    ismine_tests.cpp
    psbt_wallet_tests.cpp
    rpc_util_tests.cpp
    scriptpubkeyman_tests.cpp
    spend_tests.cpp
    wallet_crypto_tests.cpp
    wallet_tests.cpp
    wallet_transaction_tests.cpp
    walletdb_tests.cpp
    walletload_tests.cpp
)
target_link_libraries(test_bitcoin bitcoin_wallet)
