// Copyright (c) 2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_INIT_CACHES_H
#define BITCOIN_INIT_CACHES_H

#include <cstddef> // for size_t
#include <cstdint> // for int64_t

class ArgsManager;

struct CacheSizes {
    int64_t block_tree_db_cache_size;
    int64_t coin_db_cache_size;
    int64_t coin_cache_usage_size;
    int64_t tx_index_cache_size;
    int64_t filter_index_cache_size;
};
void CalculateCacheSizes(const ArgsManager& args, CacheSizes& cache_sizes, size_t n_indexes = 0);

#endif // BITCOIN_INIT_CACHES_H
