#include <ipc/interfaces.h>

#include <chainparams.h>
#include <ipc/util.h>
#include <net.h>
#include <policy/fees.h>
#include <policy/policy.h>
#include <rpc/mining.h>
#include <rpc/server.h>
#include <ui_interface.h>
#include <validation.h>
#include <validationinterface.h>

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif
#ifdef ENABLE_WALLET
#include <wallet/wallet.h>
#define CHECK_WALLET(x) x
#else
#define CHECK_WALLET(x) throw std::logic_error("Wallet function called in non-wallet build.")
#endif

#include <future>

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
        return AcceptToMemoryPool(::mempool, state, tx, true, NULL, NULL, false, ::maxTxFee);
    }
};

class LockingStateImpl : public LockedStateImpl, public CCriticalBlock
{
    using CCriticalBlock::CCriticalBlock;
};

class HandlerImpl : public Handler, private CValidationInterface
{
public:
    HandlerImpl(Chain::Notifications& notifications) : m_notifications(notifications)
    {
        RegisterValidationInterface(this);
    }
    ~HandlerImpl() override
    {
        // Don't call UnregisterValidationInterface here because it would try to
        // access virtual methods on this object which can't be accessed during
        // destruction. Also UnregisterAllValidationInterfaces is already called
        // at this point, so unregistering this object would be redundant.
    }
    void disconnect() override { UnregisterValidationInterface(this); }
    void TransactionAddedToMempool(const CTransactionRef& tx) override
    {
        m_notifications.TransactionAddedToMempool(tx);
    }
    void TransactionRemovedFromMempool(const CTransactionRef& tx) override
    {
        m_notifications.TransactionRemovedFromMempool(tx);
    }
    void BlockConnected(const std::shared_ptr<const CBlock>& block,
        const CBlockIndex* index,
        const std::vector<CTransactionRef>& tx_conflicted) override
    {
        m_notifications.BlockConnected(index->nHeight, index->GetBlockHash(), *block, tx_conflicted);
    }
    void BlockDisconnected(const std::shared_ptr<const CBlock>& block) override
    {
        m_notifications.BlockDisconnected(*block);
    }
    void SetBestChain(const CBlockLocator& locator) override { m_notifications.SetBestChain(locator); }
    void Inventory(const uint256& hash) override { m_notifications.Inventory(hash); }
    void ResendWalletTransactions(int64_t best_block_time, CConnman* connman) override
    {
        m_notifications.ResendWalletTransactions(best_block_time);
    }
    Chain::Notifications& m_notifications;
};

class ChainImpl : public Chain
{
public:
    std::unique_ptr<Chain::LockedState> lockState(bool try_lock) override
    {
        auto result = MakeUnique<LockingStateImpl>(::cs_main, "cs_main", __FILE__, __LINE__, try_lock);
        if (try_lock && result && !*result) return {};
        return result;
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
    bool hasDescendantsInMempool(const uint256& txid) override
    {
        LOCK(::mempool.cs);
        auto it_mp = ::mempool.mapTx.find(txid);
        return it_mp != ::mempool.mapTx.end() && it_mp->GetCountWithDescendants() > 1;
    }
    bool relayTransaction(const uint256& txid) override
    {
        if (g_connman) {
            CInv inv(MSG_TX, txid);
            g_connman->ForEachNode([&inv](CNode* node) { node->PushInventory(inv); });
            return true;
        }
        return false;
    }
    bool transactionWithinChainLimit(const uint256& txid, size_t chain_limit) override
    {
        return ::mempool.TransactionWithinChainLimit(txid, chain_limit);
    }
    bool checkChainLimits(CTransactionRef tx) override
    {
        LockPoints lp;
        CTxMemPoolEntry entry(tx, 0, 0, 0, false, 0, lp);
        CTxMemPool::setEntries setAncestors;
        size_t nLimitAncestors = GetArg("-limitancestorcount", DEFAULT_ANCESTOR_LIMIT);
        size_t nLimitAncestorSize = GetArg("-limitancestorsize", DEFAULT_ANCESTOR_SIZE_LIMIT) * 1000;
        size_t nLimitDescendants = GetArg("-limitdescendantcount", DEFAULT_DESCENDANT_LIMIT);
        size_t nLimitDescendantSize = GetArg("-limitdescendantsize", DEFAULT_DESCENDANT_SIZE_LIMIT) * 1000;
        std::string errString;
        if (!::mempool.CalculateMemPoolAncestors(entry, setAncestors, nLimitAncestors, nLimitAncestorSize,
                nLimitDescendants, nLimitDescendantSize, errString)) {
            return false;
        }
        return true;
    }
    CFeeRate getMinPoolFeeRate() override
    {
        return ::mempool.GetMinFee(GetArg("-maxmempool", DEFAULT_MAX_MEMPOOL_SIZE) * 1000000);
    }
    CFeeRate getMinRelayFeeRate() override { return ::minRelayTxFee; }
    CFeeRate getIncrementalRelayFeeRate() override { return ::incrementalRelayFee; }
    CFeeRate getDustRelayFeeRate() override { return ::dustRelayFee; }
    CFeeRate getMaxDiscardFeeRate() override { CHECK_WALLET(return GetDiscardRate(::feeEstimator)); }
    CAmount getMaxTxFee() override { return ::maxTxFee; }
    CAmount getMinTxFee(unsigned int tx_bytes, const CCoinControl& coin_control, FeeCalculation* calc) override
    {
        CHECK_WALLET(return CWallet::GetMinimumFee(tx_bytes, coin_control, ::mempool, ::feeEstimator, calc));
    }
    CAmount getRequiredTxFee(unsigned int tx_bytes) override
    {
        CHECK_WALLET(return CWallet::GetRequiredFee(tx_bytes));
    }
    bool getPruneMode() override { return ::fPruneMode; }
    bool p2pEnabled() override { return g_connman != nullptr; }
    int64_t getAdjustedTime() override { return GetAdjustedTime(); }
    void initMessage(const std::string& message) override { ::uiInterface.InitMessage(message); }
    void initWarning(const std::string& message) override { InitWarning(message); }
    bool initError(const std::string& message) override { return InitError(message); }
    UniValue generateBlocks(std::shared_ptr<CReserveScript> coinbase_script,
        int num_blocks,
        uint64_t max_tries,
        bool keep_script) override
    {
        return ::generateBlocks(coinbase_script, num_blocks, max_tries, keep_script);
    }
    unsigned int parseConfirmTarget(const UniValue& value) { return ParseConfirmTarget(value); }
    std::unique_ptr<Handler> handleNotifications(Notifications& notifications) override
    {
        return MakeUnique<HandlerImpl>(notifications);
    }
    void waitForNotifications() override
    {
        std::promise<void> promise;
        CallFunctionInValidationInterfaceQueue([&promise] { promise.set_value(); });
        promise.get_future().wait();
    }
    std::unique_ptr<Handler> handleRpc(const CRPCCommand& command) override;

    //! Map of RPC method name to forwarder. If multiple clients provide
    //! implementations of the same RPC method, the RPC forwarder can dispatch
    //! between them.
    class RpcForwarder;
    std::map<Ref<const std::string>, RpcForwarder> m_rpc_forwarders;
};

//! Forwarder for one RPC method.
class ChainImpl::RpcForwarder
{
public:
    RpcForwarder(const CRPCCommand& command) : m_command(command)
    {
        m_command.actor = [this](const JSONRPCRequest& request) { return forwardRequest(request); };
    }

    bool registerForwarder() { return ::tableRPC.appendCommand(m_command.name, &m_command); }

    void addCommand(const CRPCCommand& command) { m_commands.emplace_back(&command); }

    void removeCommand(const CRPCCommand& command)
    {
        m_commands.erase(std::remove(m_commands.begin(), m_commands.end(), &command));
    }

    UniValue forwardRequest(const JSONRPCRequest& request) const
    {
        // Simple forwarding of RPC requests. This just sends the request to the
        // first client that registered a handler for the RPC method. If the
        // handler throws a wallet not found exception, this will retry
        // forwarding to the next handler (if any).
        //
        // This forwarding mechanism could be made more efficient (peeking
        // inside the RPC request for wallet name and sending it directly to the
        // right handler), but right now all wallets are in-process so there is
        // only ever a single handler, and in the future it isn't clear if we
        // will want we want to keep forwarding RPCs at all (clients could just
        // listen for their own RPCs).
        for (auto it = m_commands.begin(); it != m_commands.end();) {
            const CRPCCommand& command = **it++;
            try {
                return command.actor(request);
            } catch (const UniValue& e) {
                if (it != m_commands.end()) {
                    const UniValue& code = e["code"];
                    if (code.isNum() && code.get_int() == RPC_WALLET_NOT_FOUND) {
                        continue;
                    }
                }
                throw;
            }
        }

        // This will only be reached if m_commands is empty. (Because the RPC
        // server provides an appendCommand, but no removeCommand method, it
        // will keep sending requests here even if there are no clients left to
        // forward to.)
        throw JSONRPCError(RPC_METHOD_NOT_FOUND, "Method not found");
    };

    CRPCCommand m_command;
    std::vector<const CRPCCommand*> m_commands;
};

class RpcHandler : public Handler
{
public:
    RpcHandler(ChainImpl::RpcForwarder& forwarder, const CRPCCommand& command)
        : m_forwarder(&forwarder), m_command(command)
    {
        m_forwarder->addCommand(m_command);
    }

    void disconnect() override
    {
        if (m_forwarder) {
            m_forwarder->removeCommand(m_command);
            m_forwarder = nullptr;
        }
    }

    ~RpcHandler() { disconnect(); }

    ChainImpl::RpcForwarder* m_forwarder;
    const CRPCCommand& m_command;
};

std::unique_ptr<Handler> ChainImpl::handleRpc(const CRPCCommand& command)
{
    auto inserted = m_rpc_forwarders.emplace(
        std::piecewise_construct, std::forward_as_tuple(command.name), std::forward_as_tuple(command));
    if (inserted.second && !inserted.first->second.registerForwarder()) {
        m_rpc_forwarders.erase(inserted.first);
        return {};
    }
    return MakeUnique<RpcHandler>(inserted.first->second, command);
}

} // namespace

std::unique_ptr<Chain> MakeChain() { return MakeUnique<ChainImpl>(); }

} // namespace local
} // namespace ipc
