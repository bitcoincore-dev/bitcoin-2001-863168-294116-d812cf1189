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
#include <warnings.h>

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif
#ifdef ENABLE_WALLET
#include <wallet/feebumper.h>
#include <wallet/fees.h>
#include <wallet/wallet.h>
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
    HandlerImpl(boost::signals2::connection connection) : m_connection(std::move(connection)) {}

    void disconnect() override { m_connection.disconnect(); }

    boost::signals2::scoped_connection m_connection;
};

#ifdef ENABLE_WALLET
class PendingWalletTxImpl : public PendingWalletTx
{
public:
    PendingWalletTxImpl(CWallet& wallet) : m_wallet(wallet), m_key(&wallet) {}

    const CTransaction& get() override { return *m_wtx.tx; }

    int64_t getVirtualSize() override { return ::GetVirtualTransactionSize(*m_wtx.tx); }

    bool commit(WalletValueMap value_map,
        WalletOrderForm order_form,
        std::string from_account,
        std::string& reject_reason) override
    {
        LOCK2(::cs_main, m_wallet.cs_wallet);
        m_wtx.mapValue = std::move(value_map);
        m_wtx.vOrderForm = std::move(order_form);
        m_wtx.strFromAccount = std::move(from_account);
        CValidationState state;
        if (!m_wallet.CommitTransaction(m_wtx, m_key, ::g_connman.get(), state)) {
            reject_reason = state.GetRejectReason();
            return false;
        }
        return true;
    }

    CWalletTx m_wtx;
    CWallet& m_wallet;
    CReserveKey m_key;
};

//! Construct wallet tx struct.
WalletTx MakeWalletTx(CWallet& wallet, const CWalletTx& wtx)
{
    WalletTx result;
    result.tx = wtx.tx;
    result.txin_is_mine.reserve(wtx.tx->vin.size());
    for (const auto& txin : wtx.tx->vin) {
        result.txin_is_mine.emplace_back(wallet.IsMine(txin));
    }
    result.txout_is_mine.reserve(wtx.tx->vout.size());
    result.txout_address.reserve(wtx.tx->vout.size());
    result.txout_address_is_mine.reserve(wtx.tx->vout.size());
    for (const auto& txout : wtx.tx->vout) {
        result.txout_is_mine.emplace_back(wallet.IsMine(txout));
        result.txout_address.emplace_back();
        result.txout_address_is_mine.emplace_back(
            ::ExtractDestination(txout.scriptPubKey, result.txout_address.back()) ?
                ::IsMine(wallet, result.txout_address.back()) :
                ISMINE_NO);
    }
    result.credit = wtx.GetCredit(ISMINE_ALL);
    result.debit = wtx.GetDebit(ISMINE_ALL);
    result.change = wtx.GetChange();
    result.time = wtx.GetTxTime();
    result.value_map = wtx.mapValue;
    result.is_coinbase = wtx.IsCoinBase();
    return result;
}

//! Construct wallet tx status struct.
WalletTxStatus MakeWalletTxStatus(const CWalletTx& wtx)
{
    WalletTxStatus result;
    auto mi = ::mapBlockIndex.find(wtx.hashBlock);
    CBlockIndex* block = mi != ::mapBlockIndex.end() ? mi->second : nullptr;
    result.block_height = (block ? block->nHeight : std::numeric_limits<int>::max()),
    result.blocks_to_maturity = wtx.GetBlocksToMaturity();
    result.depth_in_main_chain = wtx.GetDepthInMainChain();
    result.request_count = wtx.GetRequestCount();
    result.time_received = wtx.nTimeReceived;
    result.lock_time = wtx.tx->nLockTime;
    result.is_final = ::CheckFinalTx(*wtx.tx);
    result.is_trusted = wtx.IsTrusted();
    result.is_abandoned = wtx.isAbandoned();
    result.is_coinbase = wtx.IsCoinBase();
    result.is_in_main_chain = wtx.IsInMainChain();
    return result;
}

//! Construct wallet TxOut struct.
WalletTxOut MakeWalletTxOut(CWallet& wallet, const CWalletTx& wtx, int n, int depth)
{
    WalletTxOut result;
    result.txout = wtx.tx->vout[n];
    result.time = wtx.GetTxTime();
    result.depth_in_main_chain = depth;
    result.is_spent = wallet.IsSpent(wtx.GetHash(), n);
    return result;
}

class WalletImpl : public Wallet
{
public:
    WalletImpl(CWallet& wallet) : m_wallet(wallet) {}

    bool encryptWallet(const SecureString& wallet_passphrase) override
    {
        return m_wallet.EncryptWallet(wallet_passphrase);
    }
    bool isCrypted() override { return m_wallet.IsCrypted(); }
    bool lock() override { return m_wallet.Lock(); }
    bool unlock(const SecureString& wallet_passphrase) override { return m_wallet.Unlock(wallet_passphrase); }
    bool isLocked() override { return m_wallet.IsLocked(); }
    bool changeWalletPassphrase(const SecureString& old_wallet_passphrase,
        const SecureString& new_wallet_passphrase) override
    {
        return m_wallet.ChangeWalletPassphrase(old_wallet_passphrase, new_wallet_passphrase);
    }
    bool backupWallet(const std::string& filename) override { return m_wallet.BackupWallet(filename); }
    bool getKeyFromPool(bool internal, CPubKey& pub_key) override
    {
        return m_wallet.GetKeyFromPool(pub_key, internal);
    }
    bool getPubKey(const CKeyID& address, CPubKey& pub_key) override { return m_wallet.GetPubKey(address, pub_key); }
    bool getPrivKey(const CKeyID& address, CKey& key) override { return m_wallet.GetKey(address, key); }
    bool isSpendable(const CTxDestination& dest) override { return IsMine(m_wallet, dest) & ISMINE_SPENDABLE; }
    bool haveWatchOnly() override { return m_wallet.HaveWatchOnly(); };
    bool setAddressBook(const CTxDestination& dest, const std::string& name, const std::string& purpose) override
    {
        LOCK(m_wallet.cs_wallet);
        return m_wallet.SetAddressBook(dest, name, purpose);
    }
    bool delAddressBook(const CTxDestination& dest) override
    {
        LOCK(m_wallet.cs_wallet);
        return m_wallet.DelAddressBook(dest);
    }
    bool getAddress(const CTxDestination& dest, std::string* name, isminetype* is_mine) override
    {
        LOCK(m_wallet.cs_wallet);
        auto it = m_wallet.mapAddressBook.find(dest);
        if (it == m_wallet.mapAddressBook.end()) {
            return false;
        }
        if (name) {
            *name = it->second.name;
        }
        if (is_mine) {
            *is_mine = ::IsMine(m_wallet, dest);
        }
        return true;
    }
    std::vector<WalletAddress> getAddresses() override
    {
        LOCK(m_wallet.cs_wallet);
        std::vector<WalletAddress> result;
        for (const auto& item : m_wallet.mapAddressBook) {
            result.emplace_back();
            result.back().dest = item.first;
            result.back().is_mine = ::IsMine(m_wallet, item.first);
            result.back().name = item.second.name;
            result.back().purpose = item.second.purpose;
        }
        return result;
    }
    std::set<CTxDestination> getAccountAddresses(const std::string& account) override
    {
        return m_wallet.GetAccountAddresses(account);
    }
    bool addDestData(const CTxDestination& dest, const std::string& key, const std::string& value) override
    {
        LOCK(m_wallet.cs_wallet);
        return m_wallet.AddDestData(dest, key, value);
    }
    bool eraseDestData(const CTxDestination& dest, const std::string& key) override
    {
        LOCK(m_wallet.cs_wallet);
        return m_wallet.EraseDestData(dest, key);
    }
    std::vector<std::string> getDestValues(const std::string& prefix) override
    {
        return m_wallet.GetDestValues(prefix);
    }
    void lockCoin(const COutPoint& output) override
    {
        LOCK2(::cs_main, m_wallet.cs_wallet);
        return m_wallet.LockCoin(output);
    }
    void unlockCoin(const COutPoint& output) override
    {
        LOCK2(::cs_main, m_wallet.cs_wallet);
        return m_wallet.UnlockCoin(output);
    }
    bool isLockedCoin(const COutPoint& output) override
    {
        LOCK2(::cs_main, m_wallet.cs_wallet);
        return m_wallet.IsLockedCoin(output.hash, output.n);
    }
    void listLockedCoins(std::vector<COutPoint>& outputs) override
    {
        LOCK2(::cs_main, m_wallet.cs_wallet);
        return m_wallet.ListLockedCoins(outputs);
    }
    std::unique_ptr<PendingWalletTx> createTransaction(const std::vector<CRecipient>& recipients,
        const CCoinControl& coin_control,
        bool sign,
        int& change_pos,
        CAmount& fee,
        std::string& fail_reason) override
    {
        LOCK2(::cs_main, m_wallet.cs_wallet);
        auto pending = MakeUnique<PendingWalletTxImpl>(m_wallet);
        if (!m_wallet.CreateTransaction(
                recipients, pending->m_wtx, pending->m_key, fee, change_pos, fail_reason, coin_control, sign)) {
            return {};
        }
        return std::move(pending);
    }
    bool transactionCanBeAbandoned(const uint256& txid) override { return m_wallet.TransactionCanBeAbandoned(txid); }
    bool abandonTransaction(const uint256& txid) override { return m_wallet.AbandonTransaction(txid); }
    bool transactionCanBeBumped(const uint256& txid) override
    {
        return feebumper::TransactionCanBeBumped(&m_wallet, txid);
    }
    bool createBumpTransaction(const uint256& txid,
        const CCoinControl& coin_control,
        CAmount total_fee,
        std::vector<std::string>& errors,
        CAmount& old_fee,
        CAmount& new_fee,
        CMutableTransaction& mtx) override
    {
        return feebumper::CreateTransaction(&m_wallet, txid, coin_control, total_fee, errors, old_fee, new_fee, mtx) ==
               feebumper::Result::OK;
    }
    bool signBumpTransaction(CMutableTransaction& mtx) override { return feebumper::SignTransaction(&m_wallet, mtx); }
    bool commitBumpTransaction(const uint256& txid,
        CMutableTransaction&& mtx,
        std::vector<std::string>& errors,
        uint256& bumped_txid) override
    {
        return feebumper::CommitTransaction(&m_wallet, txid, std::move(mtx), errors, bumped_txid) ==
               feebumper::Result::OK;
    }
    CTransactionRef getTx(const uint256& txid) override
    {
        LOCK2(::cs_main, m_wallet.cs_wallet);
        auto mi = m_wallet.mapWallet.find(txid);
        if (mi != m_wallet.mapWallet.end()) {
            return mi->second.tx;
        }
        return {};
    }
    WalletTx getWalletTx(const uint256& txid) override
    {
        LOCK2(::cs_main, m_wallet.cs_wallet);
        auto mi = m_wallet.mapWallet.find(txid);
        if (mi != m_wallet.mapWallet.end()) {
            return MakeWalletTx(m_wallet, mi->second);
        }
        return {};
    }
    std::vector<WalletTx> getWalletTxs() override
    {
        LOCK2(::cs_main, m_wallet.cs_wallet);
        std::vector<WalletTx> result;
        result.reserve(m_wallet.mapWallet.size());
        for (const auto& entry : m_wallet.mapWallet) {
            result.emplace_back(MakeWalletTx(m_wallet, entry.second));
        }
        return result;
    }
    bool tryGetTxStatus(const uint256& txid,
        ipc::WalletTxStatus& tx_status,
        int& num_blocks,
        int64_t& adjusted_time) override
    {
        TRY_LOCK(::cs_main, lockMain);
        if (!lockMain) {
            return false;
        }
        TRY_LOCK(m_wallet.cs_wallet, lockWallet);
        if (!lockWallet) {
            return false;
        }
        auto mi = m_wallet.mapWallet.find(txid);
        if (mi == m_wallet.mapWallet.end()) {
            return false;
        }
        num_blocks = ::chainActive.Height();
        adjusted_time = ::GetAdjustedTime();
        tx_status = MakeWalletTxStatus(mi->second);
        return true;
    }
    WalletTx getWalletTxDetails(const uint256& txid,
        WalletTxStatus& tx_status,
        WalletOrderForm& order_form,
        bool& in_mempool,
        int& num_blocks,
        int64_t& adjusted_time) override
    {
        LOCK2(::cs_main, m_wallet.cs_wallet);
        auto mi = m_wallet.mapWallet.find(txid);
        if (mi != m_wallet.mapWallet.end()) {
            num_blocks = ::chainActive.Height();
            adjusted_time = ::GetAdjustedTime();
            in_mempool = mi->second.InMempool();
            order_form = mi->second.vOrderForm;
            tx_status = MakeWalletTxStatus(mi->second);
            return MakeWalletTx(m_wallet, mi->second);
        }
        return {};
    }
    WalletBalances getBalances() override
    {
        WalletBalances result;
        result.balance = m_wallet.GetBalance();
        result.unconfirmed_balance = m_wallet.GetUnconfirmedBalance();
        result.immature_balance = m_wallet.GetImmatureBalance();
        result.have_watch_only = m_wallet.HaveWatchOnly();
        if (result.have_watch_only) {
            result.watch_only_balance = m_wallet.GetWatchOnlyBalance();
            result.unconfirmed_watch_only_balance = m_wallet.GetUnconfirmedWatchOnlyBalance();
            result.immature_watch_only_balance = m_wallet.GetImmatureWatchOnlyBalance();
        }
        return result;
    }
    bool tryGetBalances(WalletBalances& balances, int& num_blocks) override
    {
        TRY_LOCK(::cs_main, lockMain);
        if (!lockMain) return false;
        TRY_LOCK(m_wallet.cs_wallet, lockWallet);
        if (!lockWallet) {
            return false;
        }
        balances = getBalances();
        num_blocks = ::chainActive.Height();
        return true;
    }
    CAmount getBalance() override { return m_wallet.GetBalance(); }
    CAmount getAvailableBalance(const CCoinControl& coin_control) override
    {
        return m_wallet.GetAvailableBalance(&coin_control);
    }
    isminetype txinIsMine(const CTxIn& txin) override
    {
        LOCK2(::cs_main, m_wallet.cs_wallet);
        return m_wallet.IsMine(txin);
    }
    isminetype txoutIsMine(const CTxOut& txout) override
    {
        LOCK2(::cs_main, m_wallet.cs_wallet);
        return m_wallet.IsMine(txout);
    }
    CAmount getDebit(const CTxIn& txin, isminefilter filter) override
    {
        LOCK2(::cs_main, m_wallet.cs_wallet);
        return m_wallet.GetDebit(txin, filter);
    }
    CAmount getCredit(const CTxOut& txout, isminefilter filter) override
    {
        LOCK2(::cs_main, m_wallet.cs_wallet);
        return m_wallet.GetCredit(txout, filter);
    }
    CoinsList listCoins() override
    {
        LOCK2(::cs_main, m_wallet.cs_wallet);
        CoinsList result;
        for (const auto& entry : m_wallet.ListCoins()) {
            auto& group = result[entry.first];
            for (const auto& coin : entry.second) {
                group.emplace_back(
                    COutPoint(coin.tx->GetHash(), coin.i), MakeWalletTxOut(m_wallet, *coin.tx, coin.i, coin.nDepth));
            }
        }
        return result;
    }
    std::vector<WalletTxOut> getCoins(const std::vector<COutPoint>& outputs) override
    {
        LOCK2(::cs_main, m_wallet.cs_wallet);
        std::vector<WalletTxOut> result;
        result.reserve(outputs.size());
        for (const auto& output : outputs) {
            result.emplace_back();
            auto it = m_wallet.mapWallet.find(output.hash);
            if (it != m_wallet.mapWallet.end()) {
                int depth = it->second.GetDepthInMainChain();
                if (depth >= 0) {
                    result.back() = MakeWalletTxOut(m_wallet, it->second, output.n, depth);
                }
            }
        }
        return result;
    }
    bool hdEnabled() override { return m_wallet.IsHDEnabled(); }
    std::unique_ptr<Handler> handleShowProgress(ShowProgressFn fn) override
    {
        return MakeUnique<HandlerImpl>(m_wallet.ShowProgress.connect(fn));
    }
    std::unique_ptr<Handler> handleStatusChanged(StatusChangedFn fn) override
    {
        return MakeUnique<HandlerImpl>(m_wallet.NotifyStatusChanged.connect([fn](CCryptoKeyStore*) { fn(); }));
    }
    std::unique_ptr<Handler> handleAddressBookChanged(AddressBookChangedFn fn) override
    {
        return MakeUnique<HandlerImpl>(m_wallet.NotifyAddressBookChanged.connect(
            [fn](CWallet*, const CTxDestination& address, const std::string& label, bool is_mine,
                const std::string& purpose, ChangeType status) { fn(address, label, is_mine, purpose, status); }));
    }
    std::unique_ptr<Handler> handleTransactionChanged(TransactionChangedFn fn) override
    {
        return MakeUnique<HandlerImpl>(m_wallet.NotifyTransactionChanged.connect(
            [fn, this](CWallet*, const uint256& txid, ChangeType status) { fn(txid, status); }));
    }
    std::unique_ptr<Handler> handleWatchOnlyChanged(WatchOnlyChangedFn fn) override
    {
        return MakeUnique<HandlerImpl>(m_wallet.NotifyWatchonlyChanged.connect(fn));
    }

    CWallet& m_wallet;
};
#endif

class NodeImpl : public Node
{
public:
    void parseParameters(int argc, const char* const argv[]) override { gArgs.ParseParameters(argc, argv); }
    void readConfigFile(const std::string& conf_path) override { gArgs.ReadConfigFile(conf_path); }
    bool softSetArg(const std::string& arg, const std::string& value) override { return gArgs.SoftSetArg(arg, value); }
    bool softSetBoolArg(const std::string& arg, bool value) override { return gArgs.SoftSetBoolArg(arg, value); }
    void selectParams(const std::string& network) override { ::SelectParams(network); }
    std::string getNetwork() override { return ::Params().NetworkIDString(); }
    void initLogging() override { ::InitLogging(); }
    void initParameterInteraction() override { ::InitParameterInteraction(); }
    std::string getWarnings(const std::string& type) override { return ::GetWarnings(type); }
    uint32_t getLogCategories() override { return ::logCategories; }
    bool baseInitialize() override
    {
        return ::AppInitBasicSetup() && ::AppInitParameterInteraction() && ::AppInitSanityChecks() &&
               ::AppInitLockDataDirectory();
    }
    bool appInitMain() override { return ::AppInitMain(m_thread_group, m_scheduler); }
    void appShutdown() override
    {
        ::Interrupt(m_thread_group);
        m_thread_group.join_all();
        ::Shutdown();
    }
    void startShutdown() override { ::StartShutdown(); }
    bool shutdownRequested() override { return ::ShutdownRequested(); }
    std::string helpMessage(HelpMessageMode mode) override { return ::HelpMessage(mode); }
    void mapPort(bool use_upnp) override { ::MapPort(use_upnp); }
    bool getProxy(Network net, proxyType& proxy_info) override { return ::GetProxy(net, proxy_info); }
    size_t getNodeCount(CConnman::NumConnections flags) override
    {
        return ::g_connman ? ::g_connman->GetNodeCount(flags) : 0;
    }
    bool getNodesStats(NodesStats& stats) override
    {
        stats.clear();

        if (::g_connman) {
            std::vector<CNodeStats> stats_temp;
            ::g_connman->GetNodeStats(stats_temp);

            stats.reserve(stats_temp.size());
            for (auto& node_stats_temp : stats_temp) {
                stats.emplace_back(std::move(node_stats_temp), false, CNodeStateStats());
            }

            // Try to retrieve the CNodeStateStats for each node.
            TRY_LOCK(::cs_main, lockMain);
            if (lockMain) {
                for (auto& node_stats : stats) {
                    std::get<1>(node_stats) =
                        GetNodeStateStats(std::get<0>(node_stats).nodeid, std::get<2>(node_stats));
                }
            }
            return true;
        }
        return false;
    }
    bool getBanned(banmap_t& banmap) override
    {
        if (::g_connman) {
            ::g_connman->GetBanned(banmap);
            return true;
        }
        return false;
    }
    bool ban(const CNetAddr& net_addr, BanReason reason, int64_t ban_time_offset) override
    {
        if (::g_connman) {
            ::g_connman->Ban(net_addr, reason, ban_time_offset);
            return true;
        }
        return false;
    }
    bool unban(const CSubNet& ip) override
    {
        if (::g_connman) {
            ::g_connman->Unban(ip);
            return true;
        }
        return false;
    }
    bool disconnect(NodeId id) override
    {
        if (::g_connman) {
            return ::g_connman->DisconnectNode(id);
        }
        return false;
    }
    int64_t getTotalBytesRecv() override { return ::g_connman ? ::g_connman->GetTotalBytesRecv() : 0; }
    int64_t getTotalBytesSent() override { return ::g_connman ? ::g_connman->GetTotalBytesSent() : 0; }
    size_t getMempoolSize() override { return ::mempool.size(); }
    size_t getMempoolDynamicUsage() override { return ::mempool.DynamicMemoryUsage(); }
    bool getHeaderTip(int& height, int64_t& block_time) override
    {
        LOCK(::cs_main);
        if (::pindexBestHeader) {
            height = ::pindexBestHeader->nHeight;
            block_time = ::pindexBestHeader->GetBlockTime();
            return true;
        }
        return false;
    }
    int getNumBlocks() override
    {
        LOCK(::cs_main);
        return ::chainActive.Height();
    }
    int64_t getLastBlockTime() override
    {
        LOCK(::cs_main);
        if (::chainActive.Tip()) {
            return ::chainActive.Tip()->GetBlockTime();
        }
        return ::Params().GenesisBlock().GetBlockTime(); // Genesis block's time of current network
    }
    double getVerificationProgress() override
    {
        const CBlockIndex* tip;
        {
            LOCK(::cs_main);
            tip = ::chainActive.Tip();
        }
        return ::GuessVerificationProgress(::Params().TxData(), tip);
    }
    bool isInitialBlockDownload() override { return ::IsInitialBlockDownload(); }
    bool getReindex() override { return ::fReindex; }
    bool getImporting() override { return ::fImporting; }
    void setNetworkActive(bool active) override
    {
        if (::g_connman) {
            ::g_connman->SetNetworkActive(active);
        }
    }
    bool getNetworkActive() override { return ::g_connman && ::g_connman->GetNetworkActive(); }
    unsigned int getTxConfirmTarget() override { CHECK_WALLET(return ::nTxConfirmTarget); }
    bool getWalletRbf() override { CHECK_WALLET(return ::fWalletRbf); }
    CAmount getRequiredFee(unsigned int tx_bytes) override { CHECK_WALLET(return GetRequiredFee(tx_bytes)); }
    CAmount getMinimumFee(unsigned int tx_bytes,
        const CCoinControl& coin_control,
        int* returned_target,
        FeeReason* reason) override
    {
        FeeCalculation fee_calc;
        CHECK_WALLET(return GetMinimumFee(tx_bytes, coin_control, ::mempool, ::feeEstimator, &fee_calc));
        if (returned_target) *returned_target = fee_calc.returnedTarget;
        if (reason) *reason = fee_calc.reason;
    }
    CAmount getMaxTxFee() override { return ::maxTxFee; }
    CFeeRate estimateSmartFee(int num_blocks, bool conservative, int* returned_target = nullptr) override
    {
        FeeCalculation fee_calc;
        CFeeRate result = ::feeEstimator.estimateSmartFee(num_blocks, &fee_calc, conservative);
        if (returned_target) {
            *returned_target = fee_calc.returnedTarget;
        }
        return result;
    }
    CFeeRate getDustRelayFee() override { return ::dustRelayFee; }
    CFeeRate getFallbackFee() override { CHECK_WALLET(return CWallet::fallbackFee); }
    CFeeRate getPayTxFee() override { CHECK_WALLET(return ::payTxFee); }
    void setPayTxFee(CFeeRate rate) override { CHECK_WALLET(::payTxFee = rate); }
    UniValue executeRpc(const std::string& command, const UniValue& params, const std::string& uri) override
    {
        JSONRPCRequest req;
        req.params = params;
        req.strMethod = command;
        req.URI = uri;
        return ::tableRPC.execute(req);
    }
    std::vector<std::string> listRpcCommands() override { return ::tableRPC.listCommands(); }
    void rpcSetTimerInterfaceIfUnset(RPCTimerInterface* iface) override { ::RPCSetTimerInterfaceIfUnset(iface); }
    void rpcUnsetTimerInterface(RPCTimerInterface* iface) override { ::RPCUnsetTimerInterface(iface); }
    bool getUnspentOutput(const COutPoint& output, Coin& coin) override
    {
        LOCK(::cs_main);
        return ::pcoinsTip->GetCoin(output, coin);
    }
    std::unique_ptr<Wallet> getWallet(size_t index) override
    {
        CHECK_WALLET(return index < ::vpwallets.size() ? MakeUnique<WalletImpl>(*::vpwallets[index]) :
                                                         std::unique_ptr<WalletImpl>());
    }
    std::unique_ptr<Handler> handleInitMessage(InitMessageFn fn) override
    {
        return MakeUnique<HandlerImpl>(::uiInterface.InitMessage.connect(fn));
    }
    std::unique_ptr<Handler> handleMessageBox(MessageBoxFn fn) override
    {
        return MakeUnique<HandlerImpl>(::uiInterface.ThreadSafeMessageBox.connect(fn));
    }
    std::unique_ptr<Handler> handleQuestion(QuestionFn fn) override
    {
        return MakeUnique<HandlerImpl>(::uiInterface.ThreadSafeQuestion.connect(fn));
    }
    std::unique_ptr<Handler> handleShowProgress(ShowProgressFn fn) override
    {
        return MakeUnique<HandlerImpl>(::uiInterface.ShowProgress.connect(fn));
    }
    std::unique_ptr<Handler> handleLoadWallet(LoadWalletFn fn) override
    {
        CHECK_WALLET(return MakeUnique<HandlerImpl>(
            ::uiInterface.LoadWallet.connect([fn](CWallet* wallet) { fn(MakeUnique<WalletImpl>(*wallet)); })));
    }
    std::unique_ptr<Handler> handleNotifyNumConnectionsChanged(NotifyNumConnectionsChangedFn fn) override
    {
        return MakeUnique<HandlerImpl>(::uiInterface.NotifyNumConnectionsChanged.connect(fn));
    }
    std::unique_ptr<Handler> handleNotifyNetworkActiveChanged(NotifyNetworkActiveChangedFn fn) override
    {
        return MakeUnique<HandlerImpl>(::uiInterface.NotifyNetworkActiveChanged.connect(fn));
    }
    std::unique_ptr<Handler> handleNotifyAlertChanged(NotifyAlertChangedFn fn) override
    {
        return MakeUnique<HandlerImpl>(::uiInterface.NotifyAlertChanged.connect(fn));
    }
    std::unique_ptr<Handler> handleBannedListChanged(BannedListChangedFn fn) override
    {
        return MakeUnique<HandlerImpl>(::uiInterface.BannedListChanged.connect(fn));
    }
    std::unique_ptr<Handler> handleNotifyBlockTip(NotifyBlockTipFn fn) override
    {
        return MakeUnique<HandlerImpl>(
            ::uiInterface.NotifyBlockTip.connect([fn](bool initial_download, const CBlockIndex* block) {
                fn(initial_download, block->nHeight, block->GetBlockTime(),
                    ::GuessVerificationProgress(::Params().TxData(), block));
            }));
    }
    std::unique_ptr<Handler> handleNotifyHeaderTip(NotifyHeaderTipFn fn) override
    {
        return MakeUnique<HandlerImpl>(
            ::uiInterface.NotifyHeaderTip.connect([fn](bool initial_download, const CBlockIndex* block) {
                fn(initial_download, block->nHeight, block->GetBlockTime(),
                    ::GuessVerificationProgress(::Params().TxData(), block));
            }));
    }

    boost::thread_group m_thread_group;
    ::CScheduler m_scheduler;
};

} // namespace

std::unique_ptr<Node> MakeNode() { return MakeUnique<NodeImpl>(); }

} // namespace local
} // namespace ipc
