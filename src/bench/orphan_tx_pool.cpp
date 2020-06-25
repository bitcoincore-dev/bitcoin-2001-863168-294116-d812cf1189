// Copyright (c) 2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <bench/bench.h>

#include <amount.h>
#include <net.h>
#include <net_processing.h>
#include <primitives/transaction.h>
#include <random.h>
#include <script/script.h>
#include <sync.h>

#include <vector>

static constexpr CAmount CENT{1000000};
extern bool AddOrphanTx(const CTransactionRef& tx, NodeId peer);
extern unsigned int EvictOrphanTxs(unsigned int nMaxOrphans);

static void OrphanTxPool(benchmark::State& state)
{
    FastRandomContext rand;
    std::vector<CTransactionRef> txs;
    for (unsigned int i = 0; i < DEFAULT_MAX_ORPHAN_TRANSACTIONS; ++i) {
        CMutableTransaction tx;
        tx.vin.resize(1);
        tx.vin[0].prevout.n = 0;
        tx.vin[0].prevout.hash = rand.rand256();
        tx.vin[0].scriptSig << OP_1;
        tx.vout.resize(1);
        tx.vout[0].nValue = 1 * CENT;
        tx.vout[0].scriptPubKey << OP_1;
        txs.emplace_back(MakeTransactionRef(tx));
    }

    LOCK(g_cs_orphans);
    while (state.KeepRunning()) {
        for (const auto& tx : txs) {
            AddOrphanTx(tx, 0);
        }
        EvictOrphanTxs(0);
    }
}

BENCHMARK(OrphanTxPool, 10000);
