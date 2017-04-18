#ifndef BITCOIN_IPC_INTERFACES_H
#define BITCOIN_IPC_INTERFACES_H

#include "addrdb.h"                    // For banmap_t
#include "init.h"                      // For HelpMessageMode
#include "net.h"                       // For CConnman::NumConnections
#include "netaddress.h"                // For Network
#include "pubkey.h"                    // For CTxDestination (CKeyID and CScriptID)
#include "script/ismine.h"             // isminefilter, isminetype
#include "script/standard.h"           // For CTxDestination
#include "support/allocators/secure.h" // For SecureString
#include "ui_interface.h"              // For ChangeType

#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <vector>

class proxyType;
class CCoinControl;
class CCoins;
class CKey;
class CNodeStats;
class CValidationState;
class RPCTimerInterface;
class UniValue;
struct CNodeStateStats;
struct CRecipient;

namespace ipc {

class Handler;
class Wallet;
class PendingWalletTx;
struct WalletAddress;
struct WalletBalances;
struct WalletTx;
struct WalletTxOut;
struct WalletTxStatus;
using WalletOrderForm = std::vector<std::pair<std::string, std::string>>;
using WalletValueMap = std::map<std::string, std::string>;

//! Top-level interface for a bitcoin node (bitcoind process).
class Node
{
public:
    virtual ~Node() {}

    //! Set command line arguments.
    virtual void parseParameters(int argc, const char* const argv[]) = 0;

    //! Set a command line argument if it doesn't already have a value
    virtual bool softSetArg(const std::string& arg, const std::string& value) = 0;

    //! Set a command line boolean argument if it doesn't already have a value
    virtual bool softSetBoolArg(const std::string& arg, bool value) = 0;

    //! Load settings from configuration file.
    virtual void readConfigFile(const std::string& confPath) = 0;

    //! Choose network parameters.
    virtual void selectParams(const std::string& network) = 0;

    //! Get network name.
    virtual std::string getNetwork() = 0;

    //! Init logging.
    virtual void initLogging() = 0;

    //! Init parameter interaction.
    virtual void initParameterInteraction() = 0;

    //! Get warnings.
    virtual std::string getWarnings(const std::string& type) = 0;

    // Get log flags.
    virtual uint32_t getLogCategories() = 0;

    //! Start node.
    virtual bool appInit() = 0;

    //! Stop node.
    virtual void appShutdown() = 0;

    //! Start shutdown.
    virtual void startShutdown() = 0;

    //! Return whether shutdown was requested.
    virtual bool shutdownRequested() = 0;

    //! Get help message string.
    virtual std::string helpMessage(HelpMessageMode mode) = 0;

    //! Map port.
    virtual void mapPort(bool useUPnP) = 0;

    //! Get proxy.
    virtual bool getProxy(Network net, proxyType& proxyInfo) = 0;

    //! Get number of connections.
    virtual size_t getNodeCount(CConnman::NumConnections flags) = 0;

    //! Get stats for connected nodes.
    using NodesStats = std::vector<std::tuple<CNodeStats, bool, CNodeStateStats>>;
    virtual bool getNodesStats(NodesStats& stats) = 0;

    //! Get ban map entries.
    virtual bool getBanned(banmap_t& banMap) = 0;

    //! Ban node.
    virtual bool ban(const CNetAddr& netAddr, BanReason reason, int64_t bantimeoffset) = 0;

    //! Unban node.
    virtual bool unban(const CSubNet& ip) = 0;

    //! Disconnect node.
    virtual bool disconnect(NodeId id) = 0;

    //! Get total bytes recv.
    virtual int64_t getTotalBytesRecv() = 0;

    //! Get total bytes sent.
    virtual int64_t getTotalBytesSent() = 0;

    //! Get mempool size.
    virtual size_t getMempoolSize() = 0;

    //! Get mempool dynamic usage.
    virtual size_t getMempoolDynamicUsage() = 0;

    //! Get header tip height and time.
    virtual bool getHeaderTip(int& height, int64_t& blockTime) = 0;

    //! Get num blocks.
    virtual int getNumBlocks() = 0;

    //! Get last block time.
    virtual int64_t getLastBlockTime() = 0;

    //! Get verification progress.
    virtual double getVerificationProgress() = 0;

    //! Is initial block download.
    virtual bool isInitialBlockDownload() = 0;

    //! Get reindex.
    virtual bool getReindex() = 0;

    //! Get importing.
    virtual bool getImporting() = 0;

    //! Set network active.
    virtual void setNetworkActive(bool active) = 0;

    //! Get network active.
    virtual bool getNetworkActive() = 0;

    //! Get tx confirm target.
    virtual unsigned int getTxConfirmTarget() = 0;

    //! Get wallet rbf.
    virtual bool getWalletRbf() = 0;

    //! Get required fee.
    virtual CAmount getRequiredFee(unsigned int txBytes) = 0;

    //! Get minimum fee.
    virtual CAmount getMinimumFee(unsigned int txBytes) = 0;

    //! Get max tx fee.
    virtual CAmount getMaxTxFee() = 0;

    //! Estimate smart fee.
    virtual CFeeRate estimateSmartFee(int nBlocks, int* answerFoundAtBlocks = nullptr) = 0;

    //! Get dust relay fee.
    virtual CFeeRate getDustRelayFee() = 0;

    //! Get pay tx fee.
    virtual CFeeRate getPayTxFee() = 0;

    //! Execute rpc command.
    virtual UniValue executeRpc(const std::string& command, const UniValue& params) = 0;

    //! List rpc commands.
    virtual std::vector<std::string> listRpcCommands() = 0;

    //! Set RPC timer interface if unset.
    virtual void rpcSetTimerInterfaceIfUnset(RPCTimerInterface* iface) = 0;

    //! Unset RPC timer interface.
    virtual void rpcUnsetTimerInterface(RPCTimerInterface* iface) = 0;

    //! Get unspent outputs associated with a transaction.
    virtual bool getUnspentOutputs(const uint256& txHash, CCoins& coins) = 0;

    //! Return interface for accessing the wallet.
    virtual std::unique_ptr<Wallet> getWallet() = 0;

    //! Register handler for init messages.
    using InitMessageFn = std::function<void(const std::string& message)>;
    virtual std::unique_ptr<Handler> handleInitMessage(InitMessageFn fn) = 0;

    //! Register handler for message box messages.
    using MessageBoxFn =
        std::function<bool(const std::string& message, const std::string& caption, unsigned int style)>;
    virtual std::unique_ptr<Handler> handleMessageBox(MessageBoxFn fn) = 0;

    //! Register handler for question messages.
    using QuestionFn = std::function<bool(const std::string& message,
        const std::string& nonInteractiveMessage,
        const std::string& caption,
        unsigned int style)>;
    virtual std::unique_ptr<Handler> handleQuestion(QuestionFn fn) = 0;

    //! Register handler for progress messages.
    using ShowProgressFn = std::function<void(const std::string& title, int progress)>;
    virtual std::unique_ptr<Handler> handleShowProgress(ShowProgressFn fn) = 0;

    //! Register handler for load wallet messages.
    using LoadWalletFn = std::function<void(std::unique_ptr<Wallet> wallet)>;
    virtual std::unique_ptr<Handler> handleLoadWallet(LoadWalletFn fn) = 0;

    //! Register handler for number of connections changed messages.
    using NotifyNumConnectionsChangedFn = std::function<void(int newNumConnections)>;
    virtual std::unique_ptr<Handler> handleNotifyNumConnectionsChanged(NotifyNumConnectionsChangedFn fn) = 0;

    //! Register handler for network active messages.
    using NotifyNetworkActiveChangedFn = std::function<void(bool networkActive)>;
    virtual std::unique_ptr<Handler> handleNotifyNetworkActiveChanged(NotifyNetworkActiveChangedFn fn) = 0;

    //! Register handler for notify alert messages.
    using NotifyAlertChangedFn = std::function<void()>;
    virtual std::unique_ptr<Handler> handleNotifyAlertChanged(NotifyAlertChangedFn fn) = 0;

    //! Register handler for ban list messages.
    using BannedListChangedFn = std::function<void()>;
    virtual std::unique_ptr<Handler> handleBannedListChanged(BannedListChangedFn fn) = 0;

    //! Register handler for block tip messages.
    using NotifyBlockTipFn =
        std::function<void(bool initialDownload, int height, int64_t blockTime, double verificationProgress)>;
    virtual std::unique_ptr<Handler> handleNotifyBlockTip(NotifyBlockTipFn fn) = 0;

    //! Register handler for header tip messages.
    using NotifyHeaderTipFn =
        std::function<void(bool initialDownload, int height, int64_t blockTime, double verificationProgress)>;
    virtual std::unique_ptr<Handler> handleNotifyHeaderTip(NotifyHeaderTipFn fn) = 0;
};

//! Interface for accessing a wallet.
class Wallet
{
public:
    virtual ~Wallet() {}

    //! Encrypt wallet.
    virtual bool encryptWallet(const SecureString& walletPassphrase) = 0;

    //! Return whether wallet is encrypted.
    virtual bool isCrypted() = 0;

    //! Lock wallet.
    virtual bool lock() = 0;

    //! Unlock wallet.
    virtual bool unlock(const SecureString& walletPassphrase) = 0;

    //! Return whether wallet is locked.
    virtual bool isLocked() = 0;

    //! Change wallet passphrase.
    virtual bool changeWalletPassphrase(const SecureString& oldWalletPassphrase,
        const SecureString& newWalletPassphrase) = 0;

    //! Back up wallet.
    virtual bool backupWallet(const std::string& filename) = 0;

    // Get key from pool.
    virtual bool getKeyFromPool(CPubKey& pubKey, bool internal = false) = 0;

    //! Get public key.
    virtual bool getPubKey(const CKeyID& address, CPubKey& pubKey) = 0;

    //! Get key.
    virtual bool getKey(const CKeyID& address, CKey& key) = 0;

    //! Return whether wallet has key.
    virtual bool haveKey(const CKeyID& address) = 0;

    //! Return whether wallet has watch only keys.
    virtual bool haveWatchOnly() = 0;

    //! Add or update address.
    virtual bool setAddressBook(const CTxDestination& dest, const std::string& name, const std::string& purpose) = 0;

    // Remove address.
    virtual bool delAddressBook(const CTxDestination& dest) = 0;

    //! Look up address in wallet, return whether exists.
    virtual bool getAddress(const CTxDestination& dest, std::string* name = nullptr, isminetype* ismine = nullptr) = 0;

    //! Get wallet address list.
    virtual std::vector<WalletAddress> getAddresses() = 0;

    // Get account addresses.
    virtual std::set<CTxDestination> getAccountAddresses(const std::string& account) = 0;

    //! Add dest data.
    virtual bool addDestData(const CTxDestination& dest, const std::string& key, const std::string& value) = 0;

    //! Erase dest data.
    virtual bool eraseDestData(const CTxDestination& dest, const std::string& key) = 0;

    //! Get dest values with prefix.
    virtual std::vector<std::string> getDestValues(const std::string& prefix) = 0;

    //! Lock coin.
    virtual void lockCoin(const COutPoint& output) = 0;

    //! Unlock coin.
    virtual void unlockCoin(const COutPoint& output) = 0;

    //! Return whether coin is locked.
    virtual bool isLockedCoin(const COutPoint& output) = 0;

    //! List locked coins.
    virtual void listLockedCoins(std::vector<COutPoint>& outputs) = 0;

    //! Create transaction.
    virtual std::unique_ptr<PendingWalletTx> createTransaction(const std::vector<CRecipient>& recipients,
        const CCoinControl* coinControl,
        bool sign,
        int& changePos,
        CAmount& fee,
        std::string& failReason) = 0;

    //! Return whether transaction can be abandoned.
    virtual bool transactionCanBeAbandoned(const uint256& txHash) = 0;

    //! Abandon transaction.
    virtual bool abandonTransaction(const uint256& txHash) = 0;

    //! Return whether transaction can be bumped.
    virtual bool transactionCanBeBumped(const uint256& txHash) = 0;

    //! Create bump transaction.
    virtual bool createBumpTransaction(const uint256& txHash,
        int confirmTarget,
        bool ignoreUserSetFee,
        CAmount totalFee,
        bool replaceable,
        std::vector<std::string>& errors,
        CAmount& oldFee,
        CAmount& newFee,
        CMutableTransaction& mtx) = 0;

    //! Sign bump transaction.
    virtual bool signBumpTransaction(CMutableTransaction& mtx) = 0;

    //! Commit bump transaction.
    virtual bool commitBumpTransaction(const uint256& txHash,
        CMutableTransaction&& mtx,
        std::vector<std::string>& errors,
        uint256& bumpedTxHash) = 0;

    //! Get a transaction.
    virtual CTransactionRef getTx(const uint256& txHash) = 0;

    //! Get transaction information.
    virtual WalletTx getWalletTx(const uint256& txHash) = 0;

    //! Get list of all wallet transactions.
    virtual std::vector<WalletTx> getWalletTxs() = 0;

    //! Try to get updated status for a particular transaction, if possible without blocking.
    virtual bool tryGetTxStatus(const uint256& txHash,
        WalletTxStatus& txStatus,
        int& numBlocks,
        int64_t& adjustedTime) = 0;

    //! Get transaction details.
    virtual WalletTx getWalletTxDetails(const uint256& txHash,
        WalletTxStatus& txStatus,
        WalletOrderForm& orderForm,
        bool& inMempool,
        int& numBlocks,
        int64_t& adjustedTime) = 0;

    //! Get balances.
    virtual WalletBalances getBalances() = 0;

    //! Get balances if possible without blocking.
    virtual bool tryGetBalances(WalletBalances& balances, int& numBlocks) = 0;

    //! Get balance.
    virtual CAmount getBalance() = 0;

    //! Get unconfirmed balance.
    virtual CAmount getUnconfirmedBalance() = 0;

    //! Get immature balance.
    virtual CAmount getImmatureBalance() = 0;

    //! Get watch only balance.
    virtual CAmount getWatchOnlyBalance() = 0;

    //! Get unconfirmed watch only balance.
    virtual CAmount getUnconfirmedWatchOnlyBalance() = 0;

    //! Get immature watch only balance.
    virtual CAmount getImmatureWatchOnlyBalance() = 0;

    //! Get available balance.
    virtual CAmount getAvailableBalance(const CCoinControl& coinControl) = 0;

    //! Return whether transaction input belongs to wallet.
    virtual isminetype isMine(const CTxIn& txin) = 0;

    //! Return whether transaction output belongs to wallet.
    virtual isminetype isMine(const CTxOut& txout) = 0;

    //! Return debit amount if transaction input belongs to wallet.
    virtual CAmount getDebit(const CTxIn& txin, isminefilter filter) = 0;

    //! Return credit amount if transaction input belongs to wallet.
    virtual CAmount getCredit(const CTxOut& txout, isminefilter filter) = 0;

    //! Return AvailableCoins + LockedCoins grouped by wallet address.
    //! (put change in one group with wallet address)
    using CoinsList = std::map<CTxDestination, std::vector<std::tuple<COutPoint, WalletTxOut>>>;
    virtual CoinsList listCoins() = 0;

    //! Return wallet transaction output information.
    virtual std::vector<WalletTxOut> getCoins(const std::vector<COutPoint>& outputs) = 0;

    // Return whether HD enabled.
    virtual bool hdEnabled() = 0;

    //! Register handler for show progress messages.
    using ShowProgressFn = std::function<void(const std::string& title, int progress)>;
    virtual std::unique_ptr<Handler> handleShowProgress(ShowProgressFn fn) = 0;

    //! Register handler for status changed messages.
    using StatusChangedFn = std::function<void()>;
    virtual std::unique_ptr<Handler> handleStatusChanged(StatusChangedFn fn) = 0;

    //! Register handler for address book changed messages.
    using AddressBookChangedFn = std::function<void(const CTxDestination& address,
        const std::string& label,
        bool isMine,
        const std::string& purpose,
        ChangeType status)>;
    virtual std::unique_ptr<Handler> handleAddressBookChanged(AddressBookChangedFn fn) = 0;

    //! Register handler for transaction changed messages.
    using TransactionChangedFn = std::function<void(const uint256& hashTx, ChangeType status)>;
    virtual std::unique_ptr<Handler> handleTransactionChanged(TransactionChangedFn fn) = 0;

    //! Register handler for watchonly changed messages.
    using WatchonlyChangedFn = std::function<void(bool haveWatchOnly)>;
    virtual std::unique_ptr<Handler> handleWatchonlyChanged(WatchonlyChangedFn fn) = 0;
};

//! Interface for managing a registered handler.
class Handler
{
public:
    virtual ~Handler() {}
    //! Disconnect the handler.
    virtual void disconnect() = 0;
};

//! Tracking object returned by CreateTransaction and passed to CommitTransaction.
class PendingWalletTx
{
public:
    virtual ~PendingWalletTx() {}

    //! Get transaction data.
    virtual const CTransaction& get() = 0;

    //! Get virtual transaction size.
    virtual int64_t getVirtualSize() = 0;

    //! Send pending transaction and commit to wallet.
    virtual bool commit(WalletValueMap mapValue,
        WalletOrderForm orderForm,
        std::string fromAccount,
        std::string& rejectReason) = 0;
};

//! Information about one wallet address.
struct WalletAddress
{
    CTxDestination dest;
    isminetype isMine;
    std::string name;
    std::string purpose;
};

//! Collection of wallet balances.
struct WalletBalances
{
    CAmount balance = 0;
    CAmount unconfirmedBalance = 0;
    CAmount immatureBalance = 0;
    bool haveWatchOnly = false;
    CAmount watchOnlyBalance = 0;
    CAmount unconfirmedWatchOnlyBalance = 0;
    CAmount immatureWatchOnlyBalance = 0;

    bool balanceChanged(const WalletBalances& prev) const
    {
        return balance != prev.balance || unconfirmedBalance != prev.unconfirmedBalance ||
               immatureBalance != prev.immatureBalance || watchOnlyBalance != prev.watchOnlyBalance ||
               unconfirmedWatchOnlyBalance != prev.unconfirmedWatchOnlyBalance ||
               immatureWatchOnlyBalance != prev.immatureWatchOnlyBalance;
    }
};

// Wallet transaction information.
struct WalletTx
{
    CTransactionRef tx;
    std::vector<isminetype> txInIsMine;
    std::vector<isminetype> txOutIsMine;
    std::vector<CTxDestination> txOutAddress;
    std::vector<isminetype> txOutAddressIsMine;
    CAmount credit;
    CAmount debit;
    CAmount change;
    int64_t txTime;
    std::map<std::string, std::string> mapValue;
    bool isCoinBase;
};

//! Updated transaction status.
struct WalletTxStatus
{
    int blockHeight;
    int blocksToMaturity;
    int depthInMainChain;
    int requestCount;
    unsigned int timeReceived;
    uint32_t lockTime;
    bool isFinal;
    bool isTrusted;
    bool isAbandoned;
    bool isCoinBase;
    bool isInMainChain;
};

//! Wallet transaction output.
struct WalletTxOut
{
    CTxOut txOut;
    int64_t txTime;
    int depthInMainChain = -1;
    bool isSpent = false;
};

//! Protocol IPC interface should use to communicate with implementation.
enum Protocol {
    LOCAL, //!< Call functions linked into current executable.
};

//! Create IPC node interface, communicating with requested protocol. Returns
//! null if protocol isn't implemented or is not available in the current build
//! configuation.
std::unique_ptr<Node> MakeNode(Protocol protocol);

} // namespace ipc

#endif // BITCOIN_IPC_INTERFACES_H
