#include <ipc/interfaces.h>

#include <chainparams.h>
#include <ipc/util.h>
#include <policy/policy.h>
#include <validation.h>

namespace ipc {
namespace local {
namespace {

class LockedStateImpl : public Chain::LockedState
{
public:
    int getHeight() { return ::chainActive.Height(); }
    int getBlockHeight(const uint256& hash) override
    {
        auto it = ::mapBlockIndex.find(hash);
        if (it != ::mapBlockIndex.end() && it->second) {
            if (::chainActive.Contains(it->second)) {
                return it->second->nHeight;
            }
        }
        return -1;
    }
    int getBlockDepth(const uint256& hash) override
    {
        int height = getBlockHeight(hash);
        return height < 0 ? 0 : ::chainActive.Height() - height + 1;
    }
    uint256 getBlockHash(int height) override { return ::chainActive[height]->GetBlockHash(); }
    int64_t getBlockTime(int height) override { return ::chainActive[height]->GetBlockTime(); }
    int64_t getBlockTimeMax(int height) override { return ::chainActive[height]->GetBlockTimeMax(); }
    int64_t getBlockMedianTimePast(int height) override { return ::chainActive[height]->GetMedianTimePast(); }
    bool blockHasTransactions(int height) override
    {
        CBlockIndex* block = ::chainActive[height];
        return block && (block->nStatus & BLOCK_HAVE_DATA) && block->nTx > 0;
    }
    bool readBlockFromDisk(int height, CBlock& block)
    {
        return ReadBlockFromDisk(block, ::chainActive[height], Params().GetConsensus());
    }
    double guessVerificationProgress(int height)
    {
        return GuessVerificationProgress(Params().TxData(), ::chainActive[height]);
    }
    int findEarliestAtLeast(int64_t time)
    {
        CBlockIndex* block = ::chainActive.FindEarliestAtLeast(time);
        return block ? block->nHeight : -1;
    }
    int64_t findLastBefore(int64_t time, int start_height)
    {
        CBlockIndex* block = ::chainActive[start_height];
        while (block && block->GetBlockTime() < time) {
            block = ::chainActive.Next(block);
        }
        return block ? block->nHeight : -1;
    }
    bool isPotentialTip(const uint256& hash)
    {
        if (::chainActive.Tip()->GetBlockHash() == hash) return true;
        auto it = ::mapBlockIndex.find(hash);
        return it != ::mapBlockIndex.end() && it->second->GetAncestor(::chainActive.Height()) == ::chainActive.Tip();
    }
    int findFork(const uint256& hash, int* height) override
    {
        const CBlockIndex *block{nullptr}, *fork{nullptr};
        auto it = ::mapBlockIndex.find(hash);
        if (it != ::mapBlockIndex.end()) {
            block = it->second;
            fork = ::chainActive.FindFork(block);
        }
        if (height) *height = block ? block->nHeight : -1;
        return fork ? fork->nHeight : -1;
    }
    CBlockLocator getLocator() override { return ::chainActive.GetLocator(); }
    int findLocatorFork(const CBlockLocator& locator) override
    {
        CBlockIndex* fork = FindForkInGlobalIndex(::chainActive, locator);
        return fork ? fork->nHeight : -1;
    }
    bool checkFinalTx(const CTransaction& tx) override { return CheckFinalTx(tx); }
    bool isWitnessEnabled() override { return ::IsWitnessEnabled(::chainActive.Tip(), ::Params().GetConsensus()); }
    bool acceptToMemoryPool(CTransactionRef tx, CValidationState& state) override
    {
        return AcceptToMemoryPool(::mempool, state, tx, true, nullptr, nullptr, false, ::maxTxFee);
    }
};

class LockingStateImpl : public LockedStateImpl, public CCriticalBlock
{
    using CCriticalBlock::CCriticalBlock;
};

class ChainImpl : public Chain
{
public:
    std::unique_ptr<Chain::LockedState> lockState(bool try_lock) override
    {
        auto result = MakeUnique<LockingStateImpl>(::cs_main, "cs_main", __FILE__, __LINE__, try_lock);
        if (try_lock && result && !*result) return {};
        // std::move necessary on some compilers due to conversion from
        // LockingStateImpl to LockedState pointer
        return std::move(result);
    }
    std::unique_ptr<Chain::LockedState> assumeLocked() override { return MakeUnique<LockedStateImpl>(); }
    bool findBlock(const uint256& hash, CBlock* block, int64_t* time) override
    {
        LOCK(cs_main);
        auto it = ::mapBlockIndex.find(hash);
        if (it == ::mapBlockIndex.end()) {
            return false;
        }
        if (block) {
            if (!ReadBlockFromDisk(*block, it->second, Params().GetConsensus())) {
                block->SetNull();
            }
        }
        if (time) {
            *time = it->second->GetBlockTime();
        }
        return true;
    }
    int64_t getVirtualTransactionSize(const CTransaction& tx) override { return GetVirtualTransactionSize(tx); }
    RBFTransactionState isRBFOptIn(const CTransaction& tx) override
    {
        LOCK(::mempool.cs);
        return IsRBFOptIn(tx, ::mempool);
    }
};

} // namespace

std::unique_ptr<Chain> MakeChain() { return MakeUnique<ChainImpl>(); }

} // namespace local
} // namespace ipc
