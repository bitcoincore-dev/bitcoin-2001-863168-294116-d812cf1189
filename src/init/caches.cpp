// Copyright (c) 2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <init/caches.h>

#include <txdb.h>
#include <util/system.h>
#include <validation.h>

void CalculateCacheSizes(const ArgsManager& args, CacheSizes& cache_sizes, size_t n_indexes)
{
    int64_t nTotalCache = (args.GetIntArg("-dbcache", nDefaultDbCache) << 20);
    nTotalCache = std::max(nTotalCache, nMinDbCache << 20); // total cache cannot be less than nMinDbCache
    nTotalCache = std::min(nTotalCache, nMaxDbCache << 20); // total cache cannot be greater than nMaxDbcache
    int64_t nBlockTreeDBCache = std::min(nTotalCache / 8, nMaxBlockDBCache << 20);
    nTotalCache -= nBlockTreeDBCache;
    int64_t nTxIndexCache = std::min(nTotalCache / 8, args.GetBoolArg("-txindex", DEFAULT_TXINDEX) ? nMaxTxIndexCache << 20 : 0);
    nTotalCache -= nTxIndexCache;
    int64_t filter_index_cache = 0;
    if (n_indexes > 0) {
        int64_t max_cache = std::min(nTotalCache / 8, max_filter_index_cache << 20);
        filter_index_cache = max_cache / n_indexes;
        nTotalCache -= filter_index_cache * n_indexes;
    }
    int64_t nCoinDBCache = std::min(nTotalCache / 2, (nTotalCache / 4) + (1 << 23)); // use 25%-50% of the remainder for disk cache
    nCoinDBCache = std::min(nCoinDBCache, nMaxCoinsDBCache << 20); // cap total coins db cache
    nTotalCache -= nCoinDBCache;
    int64_t nCoinCacheUsage = nTotalCache; // the rest goes to in-memory cache

    cache_sizes.block_tree_db_cache_size = nBlockTreeDBCache;
    cache_sizes.coin_db_cache_size = nCoinDBCache;
    cache_sizes.coin_cache_usage_size = nCoinCacheUsage;
    cache_sizes.tx_index_cache_size = nTxIndexCache;
    cache_sizes.filter_index_cache_size = filter_index_cache;
}
