#include <ipc/interfaces.h>

#include <chainparams.h>
#include <consensus/validation.h>
#include <init.h>
#include <ipc/util.h>
#include <net.h>
#include <net_processing.h>
#include <netbase.h>
#include <policy/fees.h>
#include <policy/policy.h>
#include <policy/rbf.h>
#include <rpc/server.h>
#include <scheduler.h>
#include <txmempool.h>
#include <ui_interface.h>
#include <util.h>
#include <validation.h>

#if defined(HAVE_CONFIG_H)
#include "config/bitcoin-config.h"
#endif
#ifdef ENABLE_WALLET
#include "wallet/feebumper.h"
#include "wallet/wallet.h"
#endif

#include <boost/thread.hpp>
#include <univalue.h>

namespace ipc {
namespace local {
namespace {

#ifdef ENABLE_WALLET
#define CHECK_WALLET(x) x
#else
#define CHECK_WALLET(x) throw std::logic_error("Wallet function called in non-wallet build.")
#endif

class HandlerImpl : public Handler
{
public:
    HandlerImpl(boost::signals2::connection connection) : connection(std::move(connection)) {}

    void disconnect() override { connection.disconnect(); }

    boost::signals2::scoped_connection connection;
};

#ifdef ENABLE_WALLET
class PendingWalletTxImpl : public PendingWalletTx
{
public:
    PendingWalletTxImpl(CWallet& wallet) : wallet(wallet), key(&wallet) {}

    const CTransaction& get() override { return *wtx.tx; }

    int64_t getVirtualSize() override { return ::GetVirtualTransactionSize(*wtx.tx); }

    bool commit(WalletValueMap mapValue,
        WalletOrderForm orderForm,
        std::string fromAccount,
        std::string& rejectReason) override
    {
        LOCK2(cs_main, wallet.cs_wallet);
        wtx.mapValue = std::move(mapValue);
        wtx.vOrderForm = std::move(orderForm);
        wtx.strFromAccount = std::move(fromAccount);
        CValidationState state;
        if (!wallet.CommitTransaction(wtx, key, ::g_connman.get(), state)) {
            rejectReason = state.GetRejectReason();
            return false;
        }
        return true;
    }

    CWalletTx wtx;
    CWallet& wallet;
    CReserveKey key;
};

//! Construct wallet tx struct.
WalletTx MakeWalletTx(CWallet& wallet, const CWalletTx& wtx)
{
    WalletTx result;
    result.tx = wtx.tx;
    result.txInIsMine.reserve(wtx.tx->vin.size());
    for (const auto& txin : wtx.tx->vin) {
        result.txInIsMine.emplace_back(wallet.IsMine(txin));
    }
    result.txOutIsMine.reserve(wtx.tx->vout.size());
    result.txOutAddress.reserve(wtx.tx->vout.size());
    result.txOutAddressIsMine.reserve(wtx.tx->vout.size());
    for (const auto& txout : wtx.tx->vout) {
        result.txOutIsMine.emplace_back(wallet.IsMine(txout));
        result.txOutAddress.emplace_back();
        result.txOutAddressIsMine.emplace_back(ExtractDestination(txout.scriptPubKey, result.txOutAddress.back()) ?
                                                   ::IsMine(wallet, result.txOutAddress.back()) :
                                                   ISMINE_NO);
    }
    result.credit = wtx.GetCredit(ISMINE_ALL);
    result.debit = wtx.GetDebit(ISMINE_ALL);
    result.change = wtx.GetChange();
    result.txTime = wtx.GetTxTime();
    result.mapValue = wtx.mapValue;
    result.isCoinBase = wtx.IsCoinBase();
    return result;
}

//! Construct wallet tx status struct.
WalletTxStatus MakeWalletTxStatus(const CWalletTx& wtx)
{
    WalletTxStatus result;
    auto mi = mapBlockIndex.find(wtx.hashBlock);
    CBlockIndex* pindex = mi != mapBlockIndex.end() ? mi->second : nullptr;
    result.blockHeight = (pindex ? pindex->nHeight : std::numeric_limits<int>::max()),
    result.blocksToMaturity = wtx.GetBlocksToMaturity();
    result.depthInMainChain = wtx.GetDepthInMainChain();
    result.requestCount = wtx.GetRequestCount();
    result.timeReceived = wtx.nTimeReceived;
    result.lockTime = wtx.tx->nLockTime;
    result.isFinal = CheckFinalTx(*wtx.tx);
    result.isTrusted = wtx.IsTrusted();
    result.isAbandoned = wtx.isAbandoned();
    result.isCoinBase = wtx.IsCoinBase();
    result.isInMainChain = wtx.IsInMainChain();
    return result;
}

//! Construct wallet TxOut struct.
WalletTxOut MakeWalletTxOut(CWallet& wallet, const CWalletTx& wtx, int n, int depth)
{
    WalletTxOut result;
    result.txOut = wtx.tx->vout[n];
    result.txTime = wtx.GetTxTime();
    result.depthInMainChain = depth;
    result.isSpent = wallet.IsSpent(wtx.GetHash(), n);
    return result;
}

class WalletImpl : public Wallet
{
public:
    WalletImpl(CWallet& wallet) : wallet(wallet) {}

    bool encryptWallet(const SecureString& walletPassphrase) override
    {
        return wallet.EncryptWallet(walletPassphrase);
    }
    bool isCrypted() override { return wallet.IsCrypted(); }
    bool lock() override { return wallet.Lock(); }
    bool unlock(const SecureString& walletPassphrase) override { return wallet.Unlock(walletPassphrase); }
    bool isLocked() override { return wallet.IsLocked(); }
    bool changeWalletPassphrase(const SecureString& oldWalletPassphrase,
        const SecureString& newWalletPassphrase) override
    {
        return wallet.ChangeWalletPassphrase(oldWalletPassphrase, newWalletPassphrase);
    }
    bool backupWallet(const std::string& filename) override { return wallet.BackupWallet(filename); }
    bool getKeyFromPool(CPubKey& pubKey, bool internal) override { return wallet.GetKeyFromPool(pubKey, internal); }
    bool getPubKey(const CKeyID& address, CPubKey& pubKey) override { return wallet.GetPubKey(address, pubKey); }
    bool getPrivKey(const CKeyID& address, CKey& key) override { return wallet.GetKey(address, key); }
    bool havePrivKey(const CKeyID& address) override { return wallet.HaveKey(address); }
    bool haveWatchOnly() override { return wallet.HaveWatchOnly(); };
    bool setAddressBook(const CTxDestination& dest, const std::string& name, const std::string& purpose) override
    {
        LOCK(wallet.cs_wallet);
        return wallet.SetAddressBook(dest, name, purpose);
    }
    bool delAddressBook(const CTxDestination& dest) override
    {
        LOCK(wallet.cs_wallet);
        return wallet.DelAddressBook(dest);
    }
    bool getAddress(const CTxDestination& dest, std::string* name, isminetype* ismine) override
    {
        LOCK(wallet.cs_wallet);
        auto it = wallet.mapAddressBook.find(dest);
        if (it == wallet.mapAddressBook.end()) {
            return false;
        }
        if (name) {
            *name = it->second.name;
        }
        if (ismine) {
            *ismine = ::IsMine(wallet, dest);
        }
        return true;
    }
    std::vector<WalletAddress> getAddresses() override
    {
        LOCK(wallet.cs_wallet);
        std::vector<WalletAddress> result;
        for (const auto& item : wallet.mapAddressBook) {
            result.emplace_back();
            result.back().dest = item.first;
            result.back().isMine = ::IsMine(wallet, item.first);
            result.back().name = item.second.name;
            result.back().purpose = item.second.purpose;
        }
        return result;
    }
    std::set<CTxDestination> getAccountAddresses(const std::string& account) override
    {
        return wallet.GetAccountAddresses(account);
    }
    bool addDestData(const CTxDestination& dest, const std::string& key, const std::string& value) override
    {
        LOCK(wallet.cs_wallet);
        return wallet.AddDestData(dest, key, value);
    }
    bool eraseDestData(const CTxDestination& dest, const std::string& key) override
    {
        LOCK(wallet.cs_wallet);
        return wallet.EraseDestData(dest, key);
    }
    std::vector<std::string> getDestValues(const std::string& prefix) override
    {
        return wallet.GetDestValues(prefix);
    }
    void lockCoin(const COutPoint& output) override
    {
        LOCK2(cs_main, wallet.cs_wallet);
        return wallet.LockCoin(output);
    }
    void unlockCoin(const COutPoint& output) override
    {
        LOCK2(cs_main, wallet.cs_wallet);
        return wallet.UnlockCoin(output);
    }
    bool isLockedCoin(const COutPoint& output) override
    {
        LOCK2(cs_main, wallet.cs_wallet);
        return wallet.IsLockedCoin(output.hash, output.n);
    }
    void listLockedCoins(std::vector<COutPoint>& outputs) override
    {
        LOCK2(cs_main, wallet.cs_wallet);
        return wallet.ListLockedCoins(outputs);
    }
    std::unique_ptr<PendingWalletTx> createTransaction(const std::vector<CRecipient>& recipients,
        const CCoinControl* coinControl,
        bool sign,
        int& changePos,
        CAmount& fee,
        std::string& failReason) override
    {
        LOCK2(cs_main, wallet.cs_wallet);
        auto pending = MakeUnique<PendingWalletTxImpl>(wallet);
        if (!wallet.CreateTransaction(
                recipients, pending->wtx, pending->key, fee, changePos, failReason, coinControl, sign)) {
            return {};
        }
        return std::move(pending);
    }
    bool transactionCanBeAbandoned(const uint256& txHash) override { return wallet.TransactionCanBeAbandoned(txHash); }
    bool abandonTransaction(const uint256& txHash) override { return wallet.AbandonTransaction(txHash); }
    bool transactionCanBeBumped(const uint256& txHash)
    {
        LOCK2(cs_main, wallet.cs_wallet);
        const CWalletTx* wtx = wallet.GetWalletTx(txHash);
        return wtx && SignalsOptInRBF(*wtx);
    }
    bool createBumpTransaction(const uint256& txHash,
        int confirmTarget,
        bool ignoreUserSetFee,
        CAmount totalFee,
        bool replaceable,
        std::vector<std::string>& errors,
        CAmount& oldFee,
        CAmount& newFee,
        CMutableTransaction& mtx)
    {
        LOCK2(cs_main, wallet.cs_wallet);
        return FeeBumper::createTransaction(&wallet, txHash, confirmTarget, ignoreUserSetFee, totalFee, replaceable,
                   errors, oldFee, newFee, mtx) == BumpFeeResult::OK;
    }
    bool signBumpTransaction(CMutableTransaction& mtx)
    {
        LOCK2(cs_main, wallet.cs_wallet);
        return FeeBumper::signTransaction(&wallet, mtx);
    }
    bool commitBumpTransaction(const uint256& txHash,
        CMutableTransaction&& mtx,
        std::vector<std::string>& errors,
        uint256& bumpedTxHash)
    {
        LOCK2(cs_main, wallet.cs_wallet);
        return FeeBumper::commitTransaction(&wallet, txHash, std::move(mtx), errors, bumpedTxHash) ==
               BumpFeeResult::OK;
    }
    CTransactionRef getTx(const uint256& txHash) override
    {
        LOCK2(cs_main, wallet.cs_wallet);
        auto mi = wallet.mapWallet.find(txHash);
        if (mi != wallet.mapWallet.end()) {
            return mi->second.tx;
        }
        return {};
    }
    WalletTx getWalletTx(const uint256& txHash) override
    {
        LOCK2(cs_main, wallet.cs_wallet);
        auto mi = wallet.mapWallet.find(txHash);
        if (mi != wallet.mapWallet.end()) {
            return MakeWalletTx(wallet, mi->second);
        }
        return {};
    }
    std::vector<WalletTx> getWalletTxs() override
    {
        LOCK2(cs_main, wallet.cs_wallet);
        std::vector<WalletTx> result;
        result.reserve(wallet.mapWallet.size());
        for (const auto& entry : wallet.mapWallet) {
            result.emplace_back(MakeWalletTx(wallet, entry.second));
        }
        return result;
    }
    bool tryGetTxStatus(const uint256& txHash,
        ipc::WalletTxStatus& txStatus,
        int& numBlocks,
        int64_t& adjustedTime) override
    {
        TRY_LOCK(cs_main, lockMain);
        if (!lockMain) {
            return false;
        }
        TRY_LOCK(wallet.cs_wallet, lockWallet);
        if (!lockWallet) {
            return false;
        }
        auto mi = wallet.mapWallet.find(txHash);
        if (mi == wallet.mapWallet.end()) {
            return false;
        }
        numBlocks = chainActive.Height();
        adjustedTime = ::GetAdjustedTime();
        txStatus = MakeWalletTxStatus(mi->second);
        return true;
    }
    WalletTx getWalletTxDetails(const uint256& txHash,
        WalletTxStatus& txStatus,
        WalletOrderForm& orderForm,
        bool& inMempool,
        int& numBlocks,
        int64_t& adjustedTime) override
    {
        LOCK2(cs_main, wallet.cs_wallet);
        auto mi = wallet.mapWallet.find(txHash);
        if (mi != wallet.mapWallet.end()) {
            numBlocks = chainActive.Height();
            adjustedTime = ::GetAdjustedTime();
            inMempool = mi->second.InMempool();
            orderForm = mi->second.vOrderForm;
            txStatus = MakeWalletTxStatus(mi->second);
            return MakeWalletTx(wallet, mi->second);
        }
        return {};
    }
    WalletBalances getBalances() override
    {
        WalletBalances result;
        result.balance = wallet.GetBalance();
        result.unconfirmedBalance = wallet.GetUnconfirmedBalance();
        result.immatureBalance = wallet.GetImmatureBalance();
        result.haveWatchOnly = wallet.HaveWatchOnly();
        if (result.haveWatchOnly) {
            result.watchOnlyBalance = wallet.GetWatchOnlyBalance();
            result.unconfirmedWatchOnlyBalance = wallet.GetUnconfirmedWatchOnlyBalance();
            result.immatureWatchOnlyBalance = wallet.GetImmatureWatchOnlyBalance();
        }
        return result;
    }
    bool tryGetBalances(WalletBalances& balances, int& numBlocks) override
    {
        TRY_LOCK(cs_main, lockMain);
        if (!lockMain) return false;
        TRY_LOCK(wallet.cs_wallet, lockWallet);
        if (!lockWallet) {
            return false;
        }
        balances = getBalances();
        numBlocks = chainActive.Height();
        return true;
    }
    CAmount getBalance() override { return wallet.GetBalance(); }
    CAmount getAvailableBalance(const CCoinControl& coinControl) override
    {
        return wallet.GetAvailableBalance(&coinControl);
    }
    isminetype isMine(const CTxIn& txin) override
    {
        LOCK2(cs_main, wallet.cs_wallet);
        return wallet.IsMine(txin);
    }
    isminetype isMine(const CTxOut& txout) override
    {
        LOCK2(cs_main, wallet.cs_wallet);
        return wallet.IsMine(txout);
    }
    CAmount getDebit(const CTxIn& txin, isminefilter filter) override
    {
        LOCK2(cs_main, wallet.cs_wallet);
        return wallet.GetDebit(txin, filter);
    }
    CAmount getCredit(const CTxOut& txout, isminefilter filter) override
    {
        LOCK2(cs_main, wallet.cs_wallet);
        return wallet.GetCredit(txout, filter);
    }
    CoinsList listCoins() override
    {
        LOCK2(cs_main, wallet.cs_wallet);
        CoinsList result;
        for (const auto& entry : wallet.ListCoins()) {
            auto& group = result[entry.first];
            for (const auto& coin : entry.second) {
                group.emplace_back(
                    COutPoint(coin.tx->GetHash(), coin.i), MakeWalletTxOut(wallet, *coin.tx, coin.i, coin.nDepth));
            }
        }
        return result;
    }
    std::vector<WalletTxOut> getCoins(const std::vector<COutPoint>& outputs) override
    {
        LOCK2(cs_main, wallet.cs_wallet);
        std::vector<WalletTxOut> result;
        result.reserve(outputs.size());
        for (const auto& output : outputs) {
            result.emplace_back();
            auto it = wallet.mapWallet.find(output.hash);
            if (it != wallet.mapWallet.end()) {
                int depth = it->second.GetDepthInMainChain();
                if (depth >= 0) {
                    result.back() = MakeWalletTxOut(wallet, it->second, output.n, depth);
                }
            }
        }
        return result;
    }
    bool hdEnabled() override { return wallet.IsHDEnabled(); }
    std::unique_ptr<Handler> handleShowProgress(ShowProgressFn fn) override
    {
        return MakeUnique<HandlerImpl>(wallet.ShowProgress.connect(fn));
    }
    std::unique_ptr<Handler> handleStatusChanged(StatusChangedFn fn) override
    {
        return MakeUnique<HandlerImpl>(wallet.NotifyStatusChanged.connect([fn](CCryptoKeyStore*) { fn(); }));
    }
    std::unique_ptr<Handler> handleAddressBookChanged(AddressBookChangedFn fn) override
    {
        return MakeUnique<HandlerImpl>(wallet.NotifyAddressBookChanged.connect(
            [fn](CWallet*, const CTxDestination& address, const std::string& label, bool isMine,
                const std::string& purpose, ChangeType status) { fn(address, label, isMine, purpose, status); }));
    }
    std::unique_ptr<Handler> handleTransactionChanged(TransactionChangedFn fn) override
    {
        return MakeUnique<HandlerImpl>(wallet.NotifyTransactionChanged.connect(
            [fn, this](CWallet*, const uint256& hashTx, ChangeType status) { fn(hashTx, status); }));
    }
    std::unique_ptr<Handler> handleWatchonlyChanged(WatchonlyChangedFn fn) override
    {
        return MakeUnique<HandlerImpl>(wallet.NotifyWatchonlyChanged.connect(fn));
    }

    CWallet& wallet;
};
#endif

class NodeImpl : public Node
{
public:
    void parseParameters(int argc, const char* const argv[]) override { ::ParseParameters(argc, argv); }
    bool softSetArg(const std::string& arg, const std::string& value) override { return ::SoftSetArg(arg, value); }
    bool softSetBoolArg(const std::string& arg, bool value) override { return ::SoftSetBoolArg(arg, value); }
    void readConfigFile(const std::string& confPath) override { ::ReadConfigFile(confPath); }
    void selectParams(const std::string& network) override { ::SelectParams(network); }
    std::string getNetwork() override { return ::Params().NetworkIDString(); }
    void initLogging() override { ::InitLogging(); }
    void initParameterInteraction() override { ::InitParameterInteraction(); }
    std::string getWarnings(const std::string& type) override { return ::GetWarnings(type); }
    uint32_t getLogCategories() override { return logCategories; }
    bool appInit() override
    {
        return ::AppInitBasicSetup() && ::AppInitParameterInteraction() && ::AppInitSanityChecks() &&
               ::AppInitMain(threadGroup, scheduler);
    }
    void appShutdown() override
    {
        ::Interrupt(threadGroup);
        threadGroup.join_all();
        ::Shutdown();
    }
    void startShutdown() override { ::StartShutdown(); }
    bool shutdownRequested() override { return ::ShutdownRequested(); }
    std::string helpMessage(HelpMessageMode mode) override { return ::HelpMessage(mode); }
    void mapPort(bool useUPnP) override { ::MapPort(useUPnP); }
    bool getProxy(Network net, proxyType& proxyInfo) override { return ::GetProxy(net, proxyInfo); }
    size_t getNodeCount(CConnman::NumConnections flags) override
    {
        return g_connman ? g_connman->GetNodeCount(flags) : 0;
    }
    bool getNodesStats(NodesStats& stats) override
    {
        stats.clear();

        if (g_connman) {
            std::vector<CNodeStats> statsTemp;
            g_connman->GetNodeStats(statsTemp);

            stats.reserve(statsTemp.size());
            for (auto& nodeStatsTemp : statsTemp) {
                stats.emplace_back(std::move(nodeStatsTemp), false, CNodeStateStats());
            }

            // Try to retrieve the CNodeStateStats for each node.
            TRY_LOCK(cs_main, lockMain);
            if (lockMain) {
                for (auto& nodeStats : stats) {
                    std::get<1>(nodeStats) = GetNodeStateStats(std::get<0>(nodeStats).nodeid, std::get<2>(nodeStats));
                }
            }
            return true;
        }
        return false;
    }
    bool getBanned(banmap_t& banMap)
    {
        if (g_connman) {
            g_connman->GetBanned(banMap);
            return true;
        }
        return false;
    }
    bool ban(const CNetAddr& netAddr, BanReason reason, int64_t bantimeoffset) override
    {
        if (g_connman) {
            g_connman->Ban(netAddr, reason, bantimeoffset);
            return true;
        }
        return false;
    }
    bool unban(const CSubNet& ip) override
    {
        if (g_connman) {
            g_connman->Unban(ip);
            return true;
        }
        return false;
    }
    bool disconnect(NodeId id) override
    {
        if (g_connman) {
            return g_connman->DisconnectNode(id);
        }
        return false;
    }
    int64_t getTotalBytesRecv() override { return g_connman ? g_connman->GetTotalBytesRecv() : 0; }
    int64_t getTotalBytesSent() override { return g_connman ? g_connman->GetTotalBytesSent() : 0; }
    size_t getMempoolSize() override { return mempool.size(); }
    size_t getMempoolDynamicUsage() override { return mempool.DynamicMemoryUsage(); }
    bool getHeaderTip(int& height, int64_t& blockTime) override
    {
        LOCK(cs_main);
        if (pindexBestHeader) {
            height = pindexBestHeader->nHeight;
            blockTime = pindexBestHeader->GetBlockTime();
            return true;
        }
        return false;
    }
    int getNumBlocks() override
    {
        LOCK(cs_main);
        return chainActive.Height();
    }
    int64_t getLastBlockTime() override
    {
        LOCK(cs_main);
        if (chainActive.Tip()) {
            return chainActive.Tip()->GetBlockTime();
        }
        return Params().GenesisBlock().GetBlockTime(); // Genesis block's time of current network
    }
    double getVerificationProgress() override
    {
        const CBlockIndex* tip;
        {
            LOCK(cs_main);
            tip = chainActive.Tip();
        }
        return ::GuessVerificationProgress(Params().TxData(), tip);
    }
    bool isInitialBlockDownload() override { return ::IsInitialBlockDownload(); }
    bool getReindex() override { return ::fReindex; }
    bool getImporting() override { return ::fImporting; }
    void setNetworkActive(bool active) override
    {
        if (g_connman) {
            g_connman->SetNetworkActive(active);
        }
    }
    bool getNetworkActive() override { return g_connman && g_connman->GetNetworkActive(); }
    unsigned int getTxConfirmTarget() override { CHECK_WALLET(return ::nTxConfirmTarget); }
    bool getWalletRbf() override { CHECK_WALLET(return ::fWalletRbf); }
    CAmount getRequiredFee(unsigned int txBytes) override { CHECK_WALLET(return CWallet::GetRequiredFee(txBytes)); }
    CAmount getMinimumFee(unsigned int txBytes) override
    {
        CHECK_WALLET(return CWallet::GetMinimumFee(txBytes, ::nTxConfirmTarget, ::mempool, ::feeEstimator));
    }
    CAmount getMaxTxFee() override { return ::maxTxFee; }
    CFeeRate estimateSmartFee(int nBlocks, int* answerFoundAtBlocks = nullptr) override
    {
        return ::feeEstimator.estimateSmartFee(nBlocks, answerFoundAtBlocks, ::mempool);
    }
    CFeeRate getDustRelayFee() override { return ::dustRelayFee; }
    CFeeRate getFallbackFee() override { CHECK_WALLET(return CWallet::fallbackFee); }
    CFeeRate getPayTxFee() override { CHECK_WALLET(return ::payTxFee); }
    void setPayTxFee(CFeeRate rate) override { CHECK_WALLET(::payTxFee = rate); }
    UniValue executeRpc(const std::string& command, const UniValue& params) override
    {
        JSONRPCRequest req;
        req.params = params;
        req.strMethod = command;
        return tableRPC.execute(req);
    }
    std::vector<std::string> listRpcCommands() override { return tableRPC.listCommands(); }
    void rpcSetTimerInterfaceIfUnset(RPCTimerInterface* iface) override { ::RPCSetTimerInterfaceIfUnset(iface); }
    void rpcUnsetTimerInterface(RPCTimerInterface* iface) override { ::RPCUnsetTimerInterface(iface); }
    bool getUnspentOutputs(const uint256& txid, CCoins& coins) override
    {
        LOCK(cs_main);
        return ::pcoinsTip->GetCoins(txid, coins);
    }
    std::unique_ptr<Wallet> getWallet() override { CHECK_WALLET(return MakeUnique<WalletImpl>(*pwalletMain)); }
    std::unique_ptr<Handler> handleInitMessage(InitMessageFn fn) override
    {
        return MakeUnique<HandlerImpl>(uiInterface.InitMessage.connect(fn));
    }
    std::unique_ptr<Handler> handleMessageBox(MessageBoxFn fn) override
    {
        return MakeUnique<HandlerImpl>(uiInterface.ThreadSafeMessageBox.connect(fn));
    }
    std::unique_ptr<Handler> handleQuestion(QuestionFn fn) override
    {
        return MakeUnique<HandlerImpl>(uiInterface.ThreadSafeQuestion.connect(fn));
    }
    std::unique_ptr<Handler> handleShowProgress(ShowProgressFn fn) override
    {
        return MakeUnique<HandlerImpl>(uiInterface.ShowProgress.connect(fn));
    }
    std::unique_ptr<Handler> handleLoadWallet(LoadWalletFn fn) override
    {
        CHECK_WALLET(return MakeUnique<HandlerImpl>(
            uiInterface.LoadWallet.connect([fn](CWallet* wallet) { fn(MakeUnique<WalletImpl>(*wallet)); })));
    }
    std::unique_ptr<Handler> handleNotifyNumConnectionsChanged(NotifyNumConnectionsChangedFn fn) override
    {
        return MakeUnique<HandlerImpl>(uiInterface.NotifyNumConnectionsChanged.connect(fn));
    }
    std::unique_ptr<Handler> handleNotifyNetworkActiveChanged(NotifyNetworkActiveChangedFn fn) override
    {
        return MakeUnique<HandlerImpl>(uiInterface.NotifyNetworkActiveChanged.connect(fn));
    }
    std::unique_ptr<Handler> handleNotifyAlertChanged(NotifyAlertChangedFn fn) override
    {
        return MakeUnique<HandlerImpl>(uiInterface.NotifyAlertChanged.connect(fn));
    }
    std::unique_ptr<Handler> handleBannedListChanged(BannedListChangedFn fn) override
    {
        return MakeUnique<HandlerImpl>(uiInterface.BannedListChanged.connect(fn));
    }
    std::unique_ptr<Handler> handleNotifyBlockTip(NotifyBlockTipFn fn) override
    {
        return MakeUnique<HandlerImpl>(
            uiInterface.NotifyBlockTip.connect([fn](bool initialDownload, const CBlockIndex* block) {
                fn(initialDownload, block->nHeight, block->GetBlockTime(),
                    GuessVerificationProgress(Params().TxData(), block));
            }));
    }
    std::unique_ptr<Handler> handleNotifyHeaderTip(NotifyHeaderTipFn fn) override
    {
        return MakeUnique<HandlerImpl>(
            uiInterface.NotifyHeaderTip.connect([fn](bool initialDownload, const CBlockIndex* block) {
                fn(initialDownload, block->nHeight, block->GetBlockTime(),
                    GuessVerificationProgress(Params().TxData(), block));
            }));
    }

    boost::thread_group threadGroup;
    ::CScheduler scheduler;
};

} // namespace

std::unique_ptr<Node> MakeNode() { return MakeUnique<NodeImpl>(); }

} // namespace local
} // namespace ipc
