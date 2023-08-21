// Copyright (c) 2017-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <common/args.h>
#include <index/base.h>
#include <interfaces/chain.h>
#include <interfaces/handler.h>
#include <kernel/chain.h>
#include <logging.h>
#include <node/abort.h>
#include <node/blockstorage.h>
#include <node/context.h>
#include <node/database_args.h>
#include <node/interface_ui.h>
#include <shutdown.h>
#include <tinyformat.h>
#include <undo.h>
#include <util/thread.h>
#include <util/translation.h>
#include <validation.h> // For g_chainman
#include <warnings.h>

#include <string>
#include <utility>

constexpr uint8_t DB_BEST_BLOCK{'B'};

constexpr auto SYNC_LOG_INTERVAL{30s};
constexpr auto SYNC_LOCATOR_WRITE_INTERVAL{30s};

template <typename... Args>
void BaseIndex::FatalErrorf(const char* fmt, const Args&... args)
{
    auto message = tfm::format(fmt, args...);
    node::AbortNode(m_chain->context()->exit_status, message);
}

const CBlockIndex& BaseIndex::BlockIndex(const uint256& hash)
{
   return WITH_LOCK(cs_main, return *Assert(m_chainstate->m_blockman.LookupBlockIndex(hash)));
}

CBlockLocator GetLocator(interfaces::Chain& chain, const uint256& block_hash)
{
    CBlockLocator locator;
    bool found = chain.findBlock(block_hash, interfaces::FoundBlock().locator(locator));
    assert(found);
    assert(!locator.IsNull());
    return locator;
}

class BaseIndexNotifications : public interfaces::Chain::Notifications
{
public:
    BaseIndexNotifications(BaseIndex& index) : m_index(index) {}
    void blockConnected(const interfaces::BlockInfo& block) override;
    void chainStateFlushed(const CBlockLocator& locator) override;
    BaseIndex& m_index;
};

void BaseIndexNotifications::blockConnected(const interfaces::BlockInfo& block)
{
    if (m_index.IgnoreBlockConnected(block)) return;

    const CBlockIndex* pindex = &m_index.BlockIndex(block.hash);
    const CBlockIndex* best_block_index = m_index.m_best_block_index.load();
    if (block.chain_tip && best_block_index && best_block_index != pindex->pprev && !m_index.Rewind(best_block_index, pindex->pprev)) {
        m_index.FatalErrorf("%s: Failed to rewind index %s to a previous chain tip",
                   __func__, m_index.GetName());
        return;
    }

    if (!m_index.CustomAppend(block)) {
        m_index.FatalErrorf("%s: Failed to write block %s to index",
                   __func__, pindex->GetBlockHash().ToString());
        return;
    }

    // Setting the best block index is intentionally the last step of this
    // function, so BlockUntilSyncedToCurrentChain callers waiting for the
    // best block index to be updated can rely on the block being fully
    // processed, and the index object being safe to delete.
    m_index.SetBestBlockIndex(pindex);
}

void BaseIndexNotifications::chainStateFlushed(const CBlockLocator& locator)
{
    if (m_index.IgnoreChainStateFlushed(locator)) return;

    // No need to handle errors in Commit. If it fails, the error will be already be logged. The
    // best way to recover is to continue, as index cannot be corrupted by a missed commit to disk
    // for an advanced index state.
    m_index.Commit(locator);
}

BaseIndex::DB::DB(const fs::path& path, size_t n_cache_size, bool f_memory, bool f_wipe, bool f_obfuscate) :
    CDBWrapper{DBParams{
        .path = path,
        .cache_bytes = n_cache_size,
        .memory_only = f_memory,
        .wipe_data = f_wipe,
        .obfuscate = f_obfuscate,
        .options = [] { DBOptions options; node::ReadDatabaseArgs(gArgs, options); return options; }()}}
{}

bool BaseIndex::DB::ReadBestBlock(CBlockLocator& locator) const
{
    bool success = Read(DB_BEST_BLOCK, locator);
    if (!success) {
        locator.SetNull();
    }
    return success;
}

void BaseIndex::DB::WriteBestBlock(CDBBatch& batch, const CBlockLocator& locator)
{
    batch.Write(DB_BEST_BLOCK, locator);
}

BaseIndex::BaseIndex(std::unique_ptr<interfaces::Chain> chain, std::string name)
    : m_chain{std::move(chain)}, m_name{std::move(name)} {}

BaseIndex::~BaseIndex()
{
    //! Assert Stop() was called before this base destructor. Notification
    //! handlers call pure virtual methods like GetName(), so if they are still
    //! being called at this point, they would segfault.
    LOCK(m_mutex);
    assert(!m_handler);
}

// Read index best block, register for block connected and disconnected
// notifications, and determine where best block is relative to chain tip.
//
// If the chain tip and index best block are the same, block connected and
// disconnected notifications will be enabled after this call and the index will
// update as the ImportBlocks() function connects blocks and sends
// notifications. Otherwise, when the chain tip and index best block not the
// same, the index will stay idle until ImportBlocks() finishes and
// BaseIndex::StartBackgroundSync() is called after.
//
// If the node is being started for the first time, or if -reindex or
// -reindex-chainstate are used, the chain tip will be null at this point,
// meaning that no blocks are attached, even a genesis block. The best block
// locator will also be null if -reindex is used or if the index is new, but
// will be non-null if -reindex-chainstate is used. So -reindex will cause the
// index to be considered synced and rebuild right away as the chain is
// rebuilt, while -reindex-chainstate will cause the index to be idle until the
// chain is rebuilt and BaseIndex::StartBackgroundSync is called after.
//
// All of this just ensures that -reindex and -reindex-chainstate options both
// function efficiently. If -reindex is used, both the chainstate and index are
// wiped, and the index is considered synced right away and gets rebuilt at the
// same time as the chainstate. If -reindex-chainstate is used, only the
// chainstate is wiped, not the index, so the index will be considered not
// synced, and the chainstate will update first, and the index will start
// syncing after. So the most efficient thing should happen in both cases.
bool BaseIndex::Init()
{
    // m_chainstate member gives indexing code access to node internals. It is
    // removed in followup https://github.com/bitcoin/bitcoin/pull/24230
    m_chainstate = &m_chain->context()->chainman->ActiveChainstate();

    CBlockLocator locator;
    if (!GetDB().ReadBestBlock(locator)) {
        locator.SetNull();
    }

    auto options = CustomOptions();
    auto notifications = std::make_shared<BaseIndexNotifications>(*this);
    auto start_sync = [&](const interfaces::BlockInfo& block) {
        const auto block_key{block.height >= 0 ? std::make_optional(interfaces::BlockKey{block.hash, block.height}) : std::nullopt};
        if (!locator.IsNull() && !block_key) {
            return InitError(strprintf(Untranslated("%s: best block of the index not found. Please rebuild the index, or disable it until the node is synced."), GetName()));
        }

        assert(!m_best_block_index && !m_synced);
        SetBestBlockIndex(block_key ? &BlockIndex(block_key->hash) : nullptr);

        // Call CustomInit and set m_ready. It is important to call CustomInit
        // before setting m_ready to ensure that CustomInit is always called
        // before CustomAppend. CustomAppend calls from the notification thread
        // will start happening when m_ready is true.
        if (!CustomInit(block_key)) {
            return false;
        }
        // To prevent race conditions, m_ready = true needs to be set from the
        // validationinterface thread and the m_ready = true callback needs to
        // be queued while cs_main is held.
        //
        // To prevent older, stale notifications currently in the validation
        // queue from being processed, it is important setting m_ready = true
        // in queue after they processed.
        //
        // To prevent new notifications that may be happening in the background
        // from being lost, it is important to keep cs_main locked while calling
        // CallFunctionInValidationInterfaceQueue, so the new notifications will
        // be queued after the m_ready = true callback.
        m_synced = block.chain_tip;
        if (m_synced) {
            CallFunctionInValidationInterfaceQueue([this] { m_ready = true; });
        }
        return true;
    };
    auto handler = m_chain->attachChain(notifications, locator, options, start_sync);

    // Handler will be null if start_sync lambda above returned false.
    if (!handler) return false;

    LOCK(m_mutex);
    m_handler = std::move(handler);
    return true;
}

static const CBlockIndex* NextSyncBlock(const CBlockIndex* pindex_prev, CChain& chain) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    AssertLockHeld(cs_main);

    if (!pindex_prev) {
        return chain.Genesis();
    }

    const CBlockIndex* pindex = chain.Next(pindex_prev);
    if (pindex) {
        return pindex;
    }

    return chain.Next(chain.FindFork(pindex_prev));
}

void BaseIndex::ThreadSync()
{
    const CBlockIndex* pindex = m_best_block_index.load();
    if (!m_synced) {
        std::chrono::steady_clock::time_point last_log_time{0s};
        std::chrono::steady_clock::time_point last_locator_write_time{0s};
        while (true) {
            if (m_interrupt) {
                SetBestBlockIndex(pindex);
                // No need to handle errors in Commit. If it fails, the error will be already be
                // logged. The best way to recover is to continue, as index cannot be corrupted by
                // a missed commit to disk for an advanced index state.
                Commit(GetLocator(*m_chain, pindex->GetBlockHash()));
                return;
            }

            {
                LOCK(cs_main);
                const CBlockIndex* pindex_next = NextSyncBlock(pindex, m_chainstate->m_chain);
                if (!pindex_next) {
                    SetBestBlockIndex(pindex);
                    m_synced = true;
                    CallFunctionInValidationInterfaceQueue([this] { m_ready = true; });
                    // No need to handle errors in Commit. See rationale above.
                    Commit(GetLocator(*m_chain, pindex->GetBlockHash()));
                    break;
                }
                if (pindex_next->pprev != pindex && !Rewind(pindex, pindex_next->pprev)) {
                    FatalErrorf("%s: Failed to rewind index %s to a previous chain tip",
                               __func__, GetName());
                    return;
                }
                pindex = pindex_next;
            }

            auto current_time{std::chrono::steady_clock::now()};
            if (last_log_time + SYNC_LOG_INTERVAL < current_time) {
                LogPrintf("Syncing %s with block chain from height %d\n",
                          GetName(), pindex->nHeight);
                last_log_time = current_time;
            }

            if (last_locator_write_time + SYNC_LOCATOR_WRITE_INTERVAL < current_time) {
                SetBestBlockIndex(pindex->pprev);
                last_locator_write_time = current_time;
                // No need to handle errors in Commit. See rationale above.
                Commit(GetLocator(*m_chain, pindex->GetBlockHash()));
            }

            CBlock block;
            interfaces::BlockInfo block_info = kernel::MakeBlockInfo(pindex);
            if (!m_chainstate->m_blockman.ReadBlockFromDisk(block, *pindex)) {
                FatalErrorf("%s: Failed to read block %s from disk",
                           __func__, pindex->GetBlockHash().ToString());
                return;
            } else {
                block_info.data = &block;
            }
            if (!CustomAppend(block_info)) {
                FatalErrorf("%s: Failed to write block %s to index database",
                           __func__, pindex->GetBlockHash().ToString());
                return;
            }
        }
    }

    if (pindex) {
        LogPrintf("%s is enabled at height %d\n", GetName(), pindex->nHeight);
    } else {
        LogPrintf("%s is enabled\n", GetName());
    }
}

bool BaseIndex::Commit(const CBlockLocator& locator)
{
    // Don't commit anything if we haven't indexed any block yet
    // (this could happen if init is interrupted).
    bool ok = !locator.IsNull();
    if (ok) {
        CDBBatch batch(GetDB());
        ok = CustomCommit(batch);
        if (ok) {
            GetDB().WriteBestBlock(batch, locator);
            ok = GetDB().WriteBatch(batch);
        }
    }
    if (!ok) {
        return error("%s: Failed to commit latest %s state", __func__, GetName());
    }
    return true;
}

bool BaseIndex::Rewind(const CBlockIndex* current_tip, const CBlockIndex* new_tip)
{
    assert(current_tip == m_best_block_index);
    assert(current_tip->GetAncestor(new_tip->nHeight) == new_tip);

    CBlock block;
    CBlockUndo block_undo;

    for (const CBlockIndex* iter_tip = current_tip; iter_tip != new_tip; iter_tip = iter_tip->pprev) {
        interfaces::BlockInfo block_info = kernel::MakeBlockInfo(iter_tip);
        if (CustomOptions().disconnect_data) {
            if (!m_chainstate->m_blockman.ReadBlockFromDisk(block, *iter_tip)) {
                return error("%s: Failed to read block %s from disk",
                             __func__, iter_tip->GetBlockHash().ToString());
            }
            block_info.data = &block;
	}
        if (CustomOptions().disconnect_undo_data && iter_tip->nHeight > 0) {
            if (!m_chainstate->m_blockman.UndoReadFromDisk(block_undo, *iter_tip)) {
                return false;
            }
            block_info.undo_data = &block_undo;
        }
        if (!CustomRemove(block_info)) {
            return false;
        }
    }

    // In the case of a reorg, ensure persisted block locator is not stale.
    // Pruning has a minimum of 288 blocks-to-keep and getting the index
    // out of sync may be possible but a users fault.
    // In case we reorg beyond the pruned depth, ReadBlockFromDisk would
    // throw and lead to a graceful shutdown
    SetBestBlockIndex(new_tip);
    if (!Commit(GetLocator(*m_chain, new_tip->GetBlockHash()))) {
        // If commit fails, revert the best block index to avoid corruption.
        SetBestBlockIndex(current_tip);
        return false;
    }

    return true;
}

bool BaseIndex::IgnoreBlockConnected(const interfaces::BlockInfo& block)
{
    if (!m_ready) {
        return true;
    }

    const CBlockIndex* pindex = &BlockIndex(block.hash);
    const CBlockIndex* best_block_index = m_best_block_index.load();
    if (!best_block_index) {
        if (pindex->nHeight != 0) {
            FatalErrorf("%s: First block connected is not the genesis block (height=%d)",
                       __func__, pindex->nHeight);
            return true;
        }
    } else {
        // To allow handling reorgs, this only checks that the new block
        // connects to ancestor of the current best block, instead of checking
        // that it connects to directly to the current block. If there is a
        // reorg, Rewind call below will remove existing blocks from the index
        // before adding the new one.
        assert(best_block_index->GetAncestor(pindex->nHeight - 1) == pindex->pprev);
    }
    return false;
}

bool BaseIndex::IgnoreChainStateFlushed(const CBlockLocator& locator)
{
    if (!m_ready) {
        return true;
    }

    const uint256& locator_tip_hash = locator.vHave.front();
    const CBlockIndex* locator_tip_index;
    {
        LOCK(cs_main);
        locator_tip_index = m_chainstate->m_blockman.LookupBlockIndex(locator_tip_hash);
    }

    if (!locator_tip_index) {
        FatalErrorf("%s: First block (hash=%s) in locator was not found",
                   __func__, locator_tip_hash.ToString());
        return true;
    }

    // This checks that ChainStateFlushed callbacks are received after BlockConnected. The check may fail
    // immediately after the sync thread catches up and sets m_synced. Consider the case where
    // there is a reorg and the blocks on the stale branch are in the ValidationInterface queue
    // backlog even after the sync thread has caught up to the new chain tip. In this unlikely
    // event, log a warning and let the queue clear.
    const CBlockIndex* best_block_index = m_best_block_index.load();
    if (best_block_index->GetAncestor(locator_tip_index->nHeight) != locator_tip_index) {
        LogPrintf("%s: WARNING: Locator contains block (hash=%s) not on known best "
                  "chain (tip=%s); not writing index locator\n",
                  __func__, locator_tip_hash.ToString(),
                  best_block_index->GetBlockHash().ToString());
        return true;
    }
    return false;
}

bool BaseIndex::BlockUntilSyncedToCurrentChain() const
{
    AssertLockNotHeld(cs_main);

    if (!m_synced) {
        return false;
    }

    {
        // Skip the queue-draining stuff if we know we're caught up with
        // m_chain.Tip().
        LOCK(cs_main);
        const CBlockIndex* chain_tip = m_chainstate->m_chain.Tip();
        const CBlockIndex* best_block_index = m_best_block_index.load();
        if (best_block_index->GetAncestor(chain_tip->nHeight) == chain_tip) {
            return true;
        }
    }

    LogPrintf("%s: %s is catching up on block notifications\n", __func__, GetName());
    SyncWithValidationInterfaceQueue();
    return true;
}

void BaseIndex::Interrupt()
{
    m_interrupt();
}

bool BaseIndex::StartBackgroundSync()
{
    if (WITH_LOCK(m_mutex, return !m_handler)) throw std::logic_error("Error: Cannot start a non-initialized index");

    m_thread_sync = std::thread(&util::TraceThread, GetName(), [this] { ThreadSync(); });
    return true;
}

void BaseIndex::Stop()
{
    WITH_LOCK(m_mutex, m_handler.reset());

    if (m_thread_sync.joinable()) {
        m_thread_sync.join();
    }
}

IndexSummary BaseIndex::GetSummary() const
{
    IndexSummary summary{};
    summary.name = GetName();
    summary.synced = m_synced;
    if (const auto& pindex = m_best_block_index.load()) {
        summary.best_block_height = pindex->nHeight;
        summary.best_block_hash = pindex->GetBlockHash();
    } else {
        summary.best_block_height = 0;
        summary.best_block_hash = m_chain->getBlockHash(0);
    }
    return summary;
}

void BaseIndex::SetBestBlockIndex(const CBlockIndex* block)
{
    assert(!m_chainstate->m_blockman.IsPruneMode() || AllowPrune());

    if (AllowPrune() && block) {
        node::PruneLockInfo prune_lock;
        prune_lock.height_first = block->nHeight;
        WITH_LOCK(::cs_main, m_chainstate->m_blockman.UpdatePruneLock(GetName(), prune_lock));
    }

    // Intentionally set m_best_block_index as the last step in this function,
    // after updating prune locks above, and after making any other references
    // to *this, so the BlockUntilSyncedToCurrentChain function (which checks
    // m_best_block_index as an optimization) can be used to wait for the last
    // BlockConnected notification and safely assume that prune locks are
    // updated and that the index object is safe to delete.
    m_best_block_index = block;
}
