// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <kernel/mempool_persist.h>

#include <clientversion.h>
#include <consensus/amount.h>
#include <logging.h>
#include <primitives/transaction.h>
#include <serialize.h>
#include <shutdown.h>
#include <streams.h>
#include <sync.h>
#include <txmempool.h>
#include <uint256.h>
#include <util/fs.h>
#include <util/fs_helpers.h>
#include <util/serfloat.h>
#include <util/time.h>
#include <validation.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

using fsbridge::FopenFn;

namespace kernel {

static const uint64_t MEMPOOL_DUMP_VERSION = 1;
static constexpr uint64_t MEMPOOL_KNOTS_DUMP_VERSION = 0;

bool LoadMempoolKnots(CTxMemPool& pool, const fs::path& knots_filepath, FopenFn mockable_fopen_function)
{
    FILE* filestr{mockable_fopen_function(knots_filepath, "rb")};
    CAutoFile file(filestr, SER_DISK, CLIENT_VERSION);
    if (file.IsNull()) {
        // Typically missing if there's nothing to save
        return false;
    }

    try {
        uint64_t version;
        file >> version;
        if (version != MEMPOOL_KNOTS_DUMP_VERSION) {
            return false;
        }

        const unsigned int priority_deltas_count = ReadCompactSize(file);
        uint256 txid;
        uint64_t encoded_priority;
        for (unsigned int i = 0; i < priority_deltas_count; ++i) {
            Unserialize(file, txid);
            Unserialize(file, encoded_priority);
            const double priority = DecodeDouble(encoded_priority);
            pool.PrioritiseTransaction(txid, priority, 0);
        }
    } catch (const std::exception& e) {
        LogPrintf("Failed to deserialize mempool-knots data on disk: %s. Continuing anyway.\n", e.what());
        return false;
    }

    return true;
}

bool LoadMempool(CTxMemPool& pool, const fs::path& load_path, Chainstate& active_chainstate, FopenFn mockable_fopen_function)
{
    if (load_path.empty()) return false;

    FILE* filestr{mockable_fopen_function(load_path, "rb")};
    CAutoFile file(filestr, SER_DISK, CLIENT_VERSION);
    if (file.IsNull()) {
        LogPrintf("Failed to open mempool file from disk. Continuing anyway.\n");
        return false;
    }

    int64_t count = 0;
    int64_t expired = 0;
    int64_t failed = 0;
    int64_t already_there = 0;
    int64_t unbroadcast = 0;
    auto now = NodeClock::now();

    try {
        uint64_t version;
        file >> version;
        if (version != MEMPOOL_DUMP_VERSION) {
            return false;
        }
        uint64_t num;
        file >> num;
        while (num) {
            --num;
            CTransactionRef tx;
            int64_t nTime;
            int64_t nFeeDelta;
            file >> tx;
            file >> nTime;
            file >> nFeeDelta;

            CAmount amountdelta = nFeeDelta;
            if (amountdelta) {
                pool.PrioritiseTransaction(tx->GetHash(), amountdelta);
            }
            if (nTime > TicksSinceEpoch<std::chrono::seconds>(now - pool.m_expiry)) {
                LOCK(cs_main);
                const auto& accepted = AcceptToMemoryPool(active_chainstate, tx, nTime, /*bypass_limits=*/false, /*test_accept=*/false);
                if (accepted.m_result_type == MempoolAcceptResult::ResultType::VALID) {
                    ++count;
                } else {
                    // mempool may contain the transaction already, e.g. from
                    // wallet(s) having loaded it while we were processing
                    // mempool transactions; consider these as valid, instead of
                    // failed, but mark them as 'already there'
                    if (pool.exists(GenTxid::Txid(tx->GetHash()))) {
                        ++already_there;
                    } else {
                        ++failed;
                    }
                }
            } else {
                ++expired;
            }
            if (ShutdownRequested())
                return false;
        }
        std::map<uint256, CAmount> mapDeltas;
        file >> mapDeltas;

        for (const auto& i : mapDeltas) {
            pool.PrioritiseTransaction(i.first, i.second);
        }

        std::set<uint256> unbroadcast_txids;
        file >> unbroadcast_txids;
        unbroadcast = unbroadcast_txids.size();
        for (const auto& txid : unbroadcast_txids) {
            // Ensure transactions were accepted to mempool then add to
            // unbroadcast set.
            if (pool.get(txid) != nullptr) pool.AddUnbroadcastTx(txid);
        }
    } catch (const std::exception& e) {
        LogPrintf("Failed to deserialize mempool data on disk: %s. Continuing anyway.\n", e.what());
        return false;
    }

    auto knots_filepath = load_path;
    knots_filepath.replace_filename("mempool-knots.dat");
    LoadMempoolKnots(pool, knots_filepath, mockable_fopen_function);

    LogPrintf("Imported mempool transactions from disk: %i succeeded, %i failed, %i expired, %i already there, %i waiting for initial broadcast\n", count, failed, expired, already_there, unbroadcast);
    return true;
}

bool DumpMempool(const CTxMemPool& pool, const fs::path& dump_path, FopenFn mockable_fopen_function, bool skip_file_commit)
{
    auto start = SteadyClock::now();

    std::map<uint256, CAmount> mapDeltas;
    std::map<uint256, double> priority_deltas;
    std::vector<TxMempoolInfo> vinfo;
    std::set<uint256> unbroadcast_txids;

    static Mutex dump_mutex;
    LOCK(dump_mutex);

    {
        LOCK(pool.cs);
        for (const auto &i : pool.mapDeltas) {
            if (i.second.first) {   // priority delta
                priority_deltas[i.first] = i.second.first;
            }
            if (i.second.second) {  // fee delta
                mapDeltas[i.first] = i.second.second;
            }
        }
        vinfo = pool.infoAll();
        unbroadcast_txids = pool.GetUnbroadcastTxs();
    }

    auto mid = SteadyClock::now();

    try {
        FILE* filestr{mockable_fopen_function(dump_path + ".new", "wb")};
        if (!filestr) {
            return false;
        }

        CAutoFile file(filestr, SER_DISK, CLIENT_VERSION);

        uint64_t version = MEMPOOL_DUMP_VERSION;
        file << version;

        file << (uint64_t)vinfo.size();
        for (const auto& i : vinfo) {
            file << *(i.tx);
            file << int64_t{count_seconds(i.m_time)};
            file << int64_t{i.nFeeDelta};
            mapDeltas.erase(i.tx->GetHash());
        }

        file << mapDeltas;

        LogPrintf("Writing %d unbroadcast transactions to disk.\n", unbroadcast_txids.size());
        file << unbroadcast_txids;

        if (!skip_file_commit && !FileCommit(file.Get()))
            throw std::runtime_error("FileCommit failed");
        file.fclose();

        auto knots_filepath = dump_path;
        knots_filepath.replace_filename("mempool-knots.dat");
        if (priority_deltas.size()) {
            auto knots_tmppath = knots_filepath;
            knots_tmppath += ".new";

            FILE* filestr{mockable_fopen_function(knots_tmppath, "wb")};
            if (!filestr) return false;
            CAutoFile file(filestr, SER_DISK, CLIENT_VERSION);

            uint64_t version = MEMPOOL_KNOTS_DUMP_VERSION;
            file << version;

            WriteCompactSize(file, priority_deltas.size());
            for (const auto& [txid, priority] : priority_deltas) {
                Serialize(file, txid);
                const uint64_t encoded_priority = EncodeDouble(priority);
                Serialize(file, encoded_priority);
            }

            if (!FileCommit(file.Get())) throw std::runtime_error("FileCommit failed");
            file.fclose();
            if (!RenameOver(knots_tmppath, knots_filepath)) {
                throw std::runtime_error("Rename failed (mempool-knots.dat)");
            }
        } else {
            fs::remove(knots_filepath);
        }

        if (!RenameOver(dump_path + ".new", dump_path)) {
            throw std::runtime_error("Rename failed");
        }
        auto last = SteadyClock::now();

        LogPrintf("Dumped mempool: %gs to copy, %gs to dump\n",
                  Ticks<SecondsDouble>(mid - start),
                  Ticks<SecondsDouble>(last - mid));
    } catch (const std::exception& e) {
        LogPrintf("Failed to dump mempool: %s. Continuing anyway.\n", e.what());
        return false;
    }
    return true;
}

} // namespace kernel
