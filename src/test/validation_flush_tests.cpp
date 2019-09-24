// Copyright (c) 2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
#include <txmempool.h>
#include <validation.h>
#include <sync.h>
#include <test/setup_common.h>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(validation_flush_tests, BasicTestingSetup)

//! Test utilities for detecting when we need to flush the coins cache based
//! on estimated memory usage.
//!
//! @sa CChainState::GetCoinsCacheSizeState()
//!
BOOST_AUTO_TEST_CASE(getcoinscachesizestate)
{
    BlockManager blockman{};
    CChainState chainstate{blockman};
    chainstate.InitCoinsDB(/*cache_size_bytes*/ 1 << 10, /*in_memory*/ true, /*should_wipe*/ false);
    WITH_LOCK(::cs_main, chainstate.InitCoinsCache());
    CTxMemPool tx_pool{};

    LOCK(::cs_main);
    auto& view = chainstate.CoinsTip();

    //! Create and add a Coin with DynamicMemoryUsage of 80 bytes to the given view.
    auto add_coin = [](CCoinsViewCache& coins_view) -> COutPoint {
        Coin newcoin;
        uint256 txid = InsecureRand256();
        COutPoint outp{txid, 0};
        newcoin.nHeight = 1;
        newcoin.out.nValue = InsecureRand32();
        newcoin.out.scriptPubKey.assign((uint32_t)56, 1);
        coins_view.AddCoin(outp, std::move(newcoin), false);

        return outp;
    };

    auto print_view_mem_usage = [](CCoinsViewCache& view) {
        BOOST_TEST_MESSAGE("CCoinsViewCache memory usage: " << view.DynamicMemoryUsage());
    };

    // Without any coins in the cache, we shouldn't need to flush.
    BOOST_CHECK_EQUAL(chainstate.GetCoinsCacheSizeState(tx_pool), CoinsCacheSizeState::OK);

    constexpr size_t MAX_COINS_CACHE_BYTES = 1024;

    print_view_mem_usage(view);
    BOOST_CHECK_EQUAL(view.DynamicMemoryUsage(), 32);

    // We should be able to add 4 coins to the cache before going CRITICAL.
    // This is contingent not only on the dynamic memory usage of the Coins
    // that we're adding (80 bytes per), but also on how much memory the
    // cacheCoins (unordered_map) preallocates.
    //
    // I came up with the 4 count by examining the printed memory usage of the
    // CCoinsCacheView, so it's sort of arbitrary - but it shouldn't change
    // unless we somehow change the way the cacheCoins map allocates memory.
    //
    for (int i{0}; i < 4; ++i) {
        COutPoint res = add_coin(view);

        print_view_mem_usage(view);
        BOOST_CHECK_EQUAL(view.AccessCoin(res).DynamicMemoryUsage(), 80);
        BOOST_CHECK_EQUAL(
            chainstate.GetCoinsCacheSizeState(tx_pool, MAX_COINS_CACHE_BYTES, 0),
            CoinsCacheSizeState::OK);
    }

    // Adding an additional coin will push us over the edge to CRITICAL.
    COutPoint res = add_coin(view);
    print_view_mem_usage(view);

    BOOST_CHECK_EQUAL(
        chainstate.GetCoinsCacheSizeState(tx_pool, MAX_COINS_CACHE_BYTES, 0),
        CoinsCacheSizeState::CRITICAL);

    // Passing non-zero max mempool usage should allow us more headroom.
    BOOST_CHECK_EQUAL(
        chainstate.GetCoinsCacheSizeState(tx_pool, MAX_COINS_CACHE_BYTES, 1 << 10),
        CoinsCacheSizeState::OK);

    for (int i{0}; i < 3; ++i) {
        add_coin(view);

        print_view_mem_usage(view);
        BOOST_CHECK_EQUAL(
            chainstate.GetCoinsCacheSizeState(tx_pool, MAX_COINS_CACHE_BYTES, 1 << 10),
            CoinsCacheSizeState::OK);
    }

    // Adding another coin with the additional mempool room will put us >90%
    // but not yet critical.
    add_coin(view);

    print_view_mem_usage(view);
    float usage_percentage = (float)view.DynamicMemoryUsage() / (MAX_COINS_CACHE_BYTES + (1 << 10));
    BOOST_TEST_MESSAGE("CoinsTip usage percentage: " << usage_percentage);
    BOOST_CHECK(usage_percentage >= 0.9);
    BOOST_CHECK(usage_percentage < 1);
    BOOST_CHECK_EQUAL(
        chainstate.GetCoinsCacheSizeState(tx_pool, MAX_COINS_CACHE_BYTES, 1 << 10),
        CoinsCacheSizeState::LARGE);

    // Using the default max_* values permits way more coins to be added.
    for (int i{0}; i < 1000; ++i) {
        add_coin(view);

        print_view_mem_usage(view);
        BOOST_CHECK_EQUAL(
            chainstate.GetCoinsCacheSizeState(tx_pool),
            CoinsCacheSizeState::OK);
    }

    // Flushing the view doesn't take us back to OK because cacheCoins has
    // preallocated memory that doesn't get reclaimed even after flush.

    BOOST_CHECK_EQUAL(
        chainstate.GetCoinsCacheSizeState(tx_pool, MAX_COINS_CACHE_BYTES, 0),
        CoinsCacheSizeState::CRITICAL);

    view.SetBestBlock(InsecureRand256());
    BOOST_CHECK(view.Flush());
    print_view_mem_usage(view);

    BOOST_CHECK_EQUAL(
        chainstate.GetCoinsCacheSizeState(tx_pool, MAX_COINS_CACHE_BYTES, 0),
        CoinsCacheSizeState::CRITICAL);
}

BOOST_AUTO_TEST_SUITE_END()
