#include "ipc/capnp/interfaces.h"

#include "bitcoin-config.h"
#include "chainparams.h"
#include "coins.h"
#include "ipc/capnp/messages.capnp.h"
#include "ipc/capnp/serialize.h"
#include "ipc/capnp/util.h"
#include "ipc/interfaces.h"
#include "ipc/util.h"
#include "net_processing.h"
#include "netbase.h"
#include "pubkey.h"

#include <capnp/rpc-twoparty.h>
#include <kj/async-unix.h>
#include <kj/debug.h>
#include <univalue.h>

#include <future>
#include <mutex>
#include <thread>

namespace ipc {
namespace capnp {
namespace {

class HandlerImpl : public Handler
{
public:
    HandlerImpl(EventLoop& loop, messages::Handler::Client client) : loop(loop), client(std::move(client)) {}
    ~HandlerImpl() noexcept override {}

    void disconnect() override
    {
        auto call = MakeCall(loop, [&]() { return client.disconnectRequest(); });
        return call.send();
    }

    EventLoop& loop;
    messages::Handler::Client client;
};

class ShowProgressCallbackServer final : public messages::ShowProgressCallback::Server
{
public:
    ShowProgressCallbackServer(Wallet::ShowProgressFn fn) : fn(std::move(fn)) {}

    kj::Promise<void> call(CallContext context) override
    {
        fn(context.getParams().getTitle(), context.getParams().getProgress());
        return kj::READY_NOW;
    }

    Wallet::ShowProgressFn fn;
};

class StatusChangedCallbackServer final : public messages::StatusChangedCallback::Server
{
public:
    StatusChangedCallbackServer(Wallet::StatusChangedFn fn) : fn(std::move(fn)) {}

    kj::Promise<void> call(CallContext context) override
    {
        fn();
        return kj::READY_NOW;
    }

    Wallet::StatusChangedFn fn;
};

class AddressBookChangedCallbackServer final : public messages::AddressBookChangedCallback::Server
{
public:
    AddressBookChangedCallbackServer(Wallet::AddressBookChangedFn fn) : fn(std::move(fn)) {}

    kj::Promise<void> call(CallContext context) override
    {
        auto params = context.getParams();
        CTxDestination address;
        if (params.getAddress().hasKeyId()) {
            address = Unserialize<CKeyID>(params.getAddress().getKeyId());
        } else if (params.getAddress().hasScriptId()) {
            address = Unserialize<CScriptID>(params.getAddress().getScriptId());
        }
        fn(address, ToString(params.getLabel()), params.getIsMine(), ToString(params.getPurpose()),
            ChangeType(params.getStatus()));
        return kj::READY_NOW;
    }

    Wallet::AddressBookChangedFn fn;
};

class TransactionChangedCallbackServer final : public messages::TransactionChangedCallback::Server
{
public:
    TransactionChangedCallbackServer(Wallet::TransactionChangedFn fn) : fn(std::move(fn)) {}

    kj::Promise<void> call(CallContext context) override
    {
        fn(ToBlob<uint256>(context.getParams().getHashTx()), ChangeType(context.getParams().getStatus()));
        return kj::READY_NOW;
    }

    Wallet::TransactionChangedFn fn;
};

class WatchonlyChangedCallbackServer final : public messages::WatchonlyChangedCallback::Server
{
public:
    WatchonlyChangedCallbackServer(Wallet::WatchonlyChangedFn fn) : fn(std::move(fn)) {}

    kj::Promise<void> call(CallContext context) override
    {
        fn(context.getParams().getHaveWatchOnly());
        return kj::READY_NOW;
    }

    Wallet::WatchonlyChangedFn fn;
};

class PendingWalletTxImpl : public PendingWalletTx
{
public:
    PendingWalletTxImpl(EventLoop& loop, messages::PendingWalletTx::Client client)
        : loop(loop), client(std::move(client))
    {
    }
    ~PendingWalletTxImpl() noexcept override {}

    const CTransaction& get() override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.getRequest();
            return request;
        });
        call.send([&]() {
            auto data = call.response->getResult();
            CDataStream stream(reinterpret_cast<const char*>(data.begin()), reinterpret_cast<const char*>(data.end()),
                SER_NETWORK, CLIENT_VERSION);
            tx = MakeUnique<CTransaction>(deserialize, stream);
        });
        return *tx;
    }
    int64_t getVirtualSize() override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.getVirtualSizeRequest();
            return request;
        });
        return call.send([&]() { return call.response->getResult(); });
    }
    bool commit(WalletValueMap mapValue,
        WalletOrderForm orderForm,
        std::string fromAccount,
        CValidationState& state) override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.commitRequest();
            // TODO request.setMapValue(mapValue);
            // TODO request.setOrderForm(orderForm);
            request.setFromAccount(fromAccount);
            return request;
        });
        return call.send([&]() {
            // TODO return string instead of state. state = call.response->getState();
            return call.response->getResult();
        });
    }

    EventLoop& loop;
    messages::PendingWalletTx::Client client;
    std::unique_ptr<CTransaction> tx;
};


class WalletImpl : public Wallet
{
public:
    WalletImpl(EventLoop& loop, messages::Wallet::Client client) : loop(loop), client(std::move(client)) {}
    ~WalletImpl() noexcept override {}

    bool encryptWallet(const SecureString& walletPassphrase) override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.encryptWalletRequest();
            request.setWalletPassphrase(FromSecureString(walletPassphrase));
            return request;
        });
        return call.send([&]() { return call.response->getResult(); });
    }
    bool isCrypted() override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.isCryptedRequest();

            return request;
        });
        return call.send([&]() { return call.response->getResult(); });
    }
    bool lock() override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.lockRequest();

            return request;
        });
        return call.send([&]() { return call.response->getResult(); });
    }
    bool unlock(const SecureString& walletPassphrase) override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.unlockRequest();
            request.setWalletPassphrase(FromSecureString(walletPassphrase));
            return request;
        });
        return call.send([&]() { return call.response->getResult(); });
    }
    bool isLocked() override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.isLockedRequest();

            return request;
        });
        return call.send([&]() { return call.response->getResult(); });
    }
    bool changeWalletPassphrase(const SecureString& oldWalletPassphrase,
        const SecureString& newWalletPassphrase) override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.changeWalletPassphraseRequest();
            request.setOldWalletPassphrase(FromSecureString(oldWalletPassphrase));
            request.setNewWalletPassphrase(FromSecureString(newWalletPassphrase));
            return request;
        });
        return call.send([&]() { return call.response->getResult(); });
    }
    bool backupWallet(const std::string& filename) override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.backupWalletRequest();
            request.setFilename(filename);
            return request;
        });
        return call.send([&]() { return call.response->getResult(); });
    }
    bool getKeyFromPool(CPubKey& pubKey, bool internal) override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.getKeyFromPoolRequest();
            request.setInternal(internal);
            return request;
        });
        return call.send([&]() {
            pubKey = Unserialize<CPubKey>(call.response->getPubKey());
            return call.response->getResult();
        });
    }
    bool getPubKey(const CKeyID& address, CPubKey& pubKey) override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.getPubKeyRequest();
            request.setAddress(ToArray(Serialize(address)));
            return request;
        });
        return call.send([&]() {
            pubKey = Unserialize<CPubKey>(call.response->getPubKey());
            return call.response->getResult();
        });
    }
    bool getKey(const CKeyID& address, CKey& key) override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.getKeyRequest();
            request.setAddress(ToArray(Serialize(address)));
            return request;
        });
        return call.send([&]() {
            // TODO key = Unserialize<CKey>(call.response->getKey());
            return call.response->getResult();
        });
    }
    bool haveKey(const CKeyID& address) override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.haveKeyRequest();
            request.setAddress(ToArray(Serialize((address))));
            return request;
        });
        return call.send([&]() { return call.response->getResult(); });
    }
    bool haveWatchOnly() override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.haveWatchOnlyRequest();

            return request;
        });
        return call.send([&]() { return call.response->getResult(); });
    }
    bool setAddressBook(const CTxDestination& dest, const std::string& name, const std::string& purpose) override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.setAddressBookRequest();
            // TODO request.setDest(dest);
            request.setName(name);
            request.setPurpose(purpose);
            return request;
        });
        return call.send([&]() { return call.response->getResult(); });
    }
    bool delAddressBook(const CTxDestination& dest) override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.delAddressBookRequest();
            // TODO request.setDest(dest);
            return request;
        });
        return call.send([&]() { return call.response->getResult(); });
    }
    bool getAddress(const CTxDestination& dest, std::string* name, isminetype* ismine) override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.getAddressRequest();
            // TODO request.setDest(dest);
            return request;
        });
        return call.send([&]() {
            if (name) {
                *name = ToString(call.response->getName());
            }
            if (ismine) {
                *ismine = isminetype(call.response->getIsmine());
            }
            return call.response->getResult();
        });
    }
    std::vector<WalletAddress> getAddresses() override
    {
        std::vector<WalletAddress> result;
        auto call = MakeCall(loop, [&]() {
            auto request = client.getAddressesRequest();

            return request;
        });
        call.send([&]() {
            // TODO call.response->getResult();
        });
        return result;
    }
    std::set<CTxDestination> getAccountAddresses(const std::string& account) override
    {
        std::set<CTxDestination> result;
        auto call = MakeCall(loop, [&]() {
            auto request = client.getAccountAddressesRequest();
            request.setAccount(account);
            return request;
        });
        call.send([&]() {
            // TODO call.response->getResult();
        });
        return result;
    }
    bool addDestData(const CTxDestination& dest, const std::string& key, const std::string& value) override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.addDestDataRequest();
            // TODO request.setDest(dest);
            request.setKey(key);
            request.setValue(value);
            return request;
        });
        return call.send([&]() { return call.response->getResult(); });
    }
    bool eraseDestData(const CTxDestination& dest, const std::string& key) override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.eraseDestDataRequest();
            // TODO request.setDest(dest);
            request.setKey(key);
            return request;
        });
        return call.send([&]() { return call.response->getResult(); });
    }
    void getDestValues(const std::string& prefix, std::vector<std::string>& values) override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.getDestValuesRequest();
            request.setPrefix(prefix);
            return request;
        });
        return call.send([&]() {
            // TODO values = call.response->getValues();
        });
    }
    void lockCoin(const COutPoint& output) override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.lockCoinRequest();
            request.setOutput(ToArray(Serialize(output)));
            return request;
        });
        return call.send();
    }
    void unlockCoin(const COutPoint& output) override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.unlockCoinRequest();
            request.setOutput(ToArray(Serialize(output)));
            return request;
        });
        return call.send();
    }
    bool isLockedCoin(const COutPoint& output) override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.isLockedCoinRequest();
            request.setOutput(ToArray(Serialize(output)));
            return request;
        });
        return call.send([&]() { return call.response->getResult(); });
    }
    void listLockedCoins(std::vector<COutPoint>& outputs) override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.listLockedCoinsRequest();

            return request;
        });
        return call.send([&]() {
            // TODO outputs = call.response->getOutputs();
        });
    }
    std::unique_ptr<PendingWalletTx> createTransaction(const std::vector<CRecipient>& recipients,
        const CCoinControl* coinControl,
        bool sign,
        int& changePos,
        CAmount& fee,
        std::string& failReason) override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.createTransactionRequest();
            // TODO request.setRecipients(recipients);
            // TODO request.setCoinControl(coinControl);
            request.setSign(sign);
            return request;
        });
        return call.send([&]() {
            changePos = call.response->getChangePos();
            fee = call.response->getFee();
            failReason = call.response->getFailReason();
            return MakeUnique<PendingWalletTxImpl>(loop, call.response->getResult());
        });
    }
    bool transactionCanBeAbandoned(const uint256& txHash) override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.transactionCanBeAbandonedRequest();
            request.setTxHash(ToArray(txHash));
            return request;
        });
        return call.send([&]() { return call.response->getResult(); });
    }
    bool abandonTransaction(const uint256& txHash) override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.abandonTransactionRequest();
            request.setTxHash(ToArray(txHash));
            return request;
        });
        return call.send([&]() { return call.response->getResult(); });
    }
    CTransactionRef getTx(const uint256& txHash) override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.getTxRequest();
            request.setTxHash(ToArray(txHash));
            return request;
        });
        return call.send([&]() {
            // TODO dedup
            auto data = call.response->getResult();
            CDataStream stream(reinterpret_cast<const char*>(data.begin()), reinterpret_cast<const char*>(data.end()),
                SER_NETWORK, CLIENT_VERSION);
            return std::make_shared<CTransaction>(deserialize, stream);
        });
    }
    WalletTx getWalletTx(const uint256& txHash) override
    {
        WalletTx result;
        auto call = MakeCall(loop, [&]() {
            auto request = client.getWalletTxRequest();
            request.setTxHash(ToArray(txHash));
            return request;
        });
        call.send([&]() {
            // TODO call.response->getResult();
        });
        return result;
    }
    std::vector<WalletTx> getWalletTxs() override
    {
        std::vector<WalletTx> result;
        auto call = MakeCall(loop, [&]() {
            auto request = client.getWalletTxsRequest();

            return request;
        });
        call.send([&]() {
            // TODO call.response->getResult();
        });
        return result;
    }
    bool tryGetTxStatus(const uint256& txHash,
        WalletTxStatus& txStatus,
        int& numBlocks,
        int64_t& adjustedTime) override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.tryGetTxStatusRequest();
            request.setTxHash(ToArray(txHash));
            return request;
        });
        return call.send([&]() {
            // txStatus = call.response->getTxStatus();
            numBlocks = call.response->getNumBlocks();
            adjustedTime = call.response->getAdjustedTime();
            return call.response->getResult();
        });
    }
    WalletTx getWalletTxDetails(const uint256& txHash,
        WalletTxStatus& txStatus,
        WalletOrderForm& orderForm,
        bool& inMempool,
        int& numBlocks,
        int64_t& adjustedTime) override
    {
        WalletTx result;
        auto call = MakeCall(loop, [&]() {
            auto request = client.getWalletTxDetailsRequest();
            request.setTxHash(ToArray(txHash));
            return request;
        });
        call.send([&]() {
            // TODO txStatus = call.response->getTxStatus();
            // TODO orderForm = call.response->getOrderForm();
            inMempool = call.response->getInMempool();
            numBlocks = call.response->getNumBlocks();
            adjustedTime = call.response->getAdjustedTime();
            // TODO result = call.response->getResult();
        });
        return result;
    }
    WalletBalances getBalances() override
    {
        WalletBalances result;
        auto call = MakeCall(loop, [&]() {
            auto request = client.getBalancesRequest();

            return request;
        });
        call.send([&]() {
            // TODO return call.response->getResult();
        });
        return result;
    }
    bool tryGetBalances(WalletBalances& balances, int& numBlocks) override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.tryGetBalancesRequest();

            return request;
        });
        return call.send([&]() { // TODO balances = call.response->getBalances();
            numBlocks = call.response->getNumBlocks();
            return call.response->getResult();
        });
    }
    CAmount getBalance() override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.getBalanceRequest();

            return request;
        });
        return call.send([&]() { return call.response->getResult(); });
    }
    CAmount getUnconfirmedBalance() override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.getUnconfirmedBalanceRequest();

            return request;
        });
        return call.send([&]() { return call.response->getResult(); });
    }
    CAmount getImmatureBalance() override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.getImmatureBalanceRequest();

            return request;
        });
        return call.send([&]() { return call.response->getResult(); });
    }
    CAmount getWatchOnlyBalance() override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.getWatchOnlyBalanceRequest();

            return request;
        });
        return call.send([&]() { return call.response->getResult(); });
    }
    CAmount getUnconfirmedWatchOnlyBalance() override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.getUnconfirmedWatchOnlyBalanceRequest();

            return request;
        });
        return call.send([&]() { return call.response->getResult(); });
    }
    CAmount getImmatureWatchOnlyBalance() override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.getImmatureWatchOnlyBalanceRequest();

            return request;
        });
        return call.send([&]() { return call.response->getResult(); });
    }
    CAmount getAvailableBalance(const CCoinControl& coinControl) override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.getAvailableBalanceRequest();
            // TODO request.setCoinControl(coinControl);
            return request;
        });
        return call.send([&]() { return call.response->getResult(); });
    }
    isminetype isMine(const CTxIn& txin) override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.isMineRequest();
            request.setTxin(ToArray(Serialize((txin))));
            return request;
        });
        return call.send([&]() { return isminetype(call.response->getResult()); });
    }
    isminetype isMine(const CTxOut& txout) override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.isMineRequest();
            request.setTxout(ToArray(Serialize(txout)));
            return request;
        });
        return call.send([&]() { return isminetype(call.response->getResult()); });
    }
    CAmount getDebit(const CTxIn& txin, isminefilter filter) override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.getDebitRequest();
            request.setTxin(ToArray(Serialize((txin))));
            request.setFilter(filter);
            return request;
        });
        return call.send([&]() { return call.response->getResult(); });
    }
    CAmount getCredit(const CTxOut& txout, isminefilter filter) override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.getCreditRequest();
            request.setTxout(ToArray(Serialize(txout)));
            request.setFilter(filter);
            return request;
        });
        return call.send([&]() { return call.response->getResult(); });
    }
    CoinsList listCoins() override
    {
        CoinsList result;
        auto call = MakeCall(loop, [&]() {
            auto request = client.listCoinsRequest();

            return request;
        });
        call.send([&]() {
            // TODO result = call.response->getResult();
        });
        return result;
    }
    std::vector<WalletTxOut> getCoins(const std::vector<COutPoint>& outputs) override
    {
        std::vector<WalletTxOut> result;
        auto call = MakeCall(loop, [&]() {
            auto request = client.getCoinsRequest();
            // TODO request.setOutputs(outputs);
            return request;
        });
        call.send([&]() {
            // call.response->getResult();
        });
        return result;
    }
    bool hdEnabled() override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.hdEnabledRequest();

            return request;
        });
        return call.send([&]() { return call.response->getResult(); });
    }
    std::unique_ptr<Handler> handleShowProgress(ShowProgressFn fn) override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.handleShowProgressRequest();
            request.setCallback(kj::heap<ShowProgressCallbackServer>(std::move(fn)));
            return request;
        });
        return call.send([&]() { return MakeUnique<HandlerImpl>(loop, call.response->getHandler()); });
    }
    std::unique_ptr<Handler> handleStatusChanged(StatusChangedFn fn) override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.handleStatusChangedRequest();
            request.setCallback(kj::heap<StatusChangedCallbackServer>(std::move(fn)));
            return request;
        });
        return call.send([&]() { return MakeUnique<HandlerImpl>(loop, call.response->getHandler()); });
    }
    std::unique_ptr<Handler> handleAddressBookChanged(AddressBookChangedFn fn) override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.handleAddressBookChangedRequest();
            request.setCallback(kj::heap<AddressBookChangedCallbackServer>(std::move(fn)));
            return request;
        });
        return call.send([&]() { return MakeUnique<HandlerImpl>(loop, call.response->getHandler()); });
    }
    std::unique_ptr<Handler> handleTransactionChanged(TransactionChangedFn fn) override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.handleTransactionChangedRequest();
            request.setCallback(kj::heap<TransactionChangedCallbackServer>(std::move(fn)));
            return request;
        });
        return call.send([&]() { return MakeUnique<HandlerImpl>(loop, call.response->getHandler()); });
    }
    std::unique_ptr<Handler> handleWatchonlyChanged(WatchonlyChangedFn fn) override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.handleWatchonlyChangedRequest();
            request.setCallback(kj::heap<WatchonlyChangedCallbackServer>(std::move(fn)));
            return request;
        });
        return call.send([&]() { return MakeUnique<HandlerImpl>(loop, call.response->getHandler()); });
    }

    EventLoop& loop;
    messages::Wallet::Client client;
};

class InitMessageCallbackServer final : public messages::InitMessageCallback::Server
{
public:
    InitMessageCallbackServer(Node::InitMessageFn fn) : fn(std::move(fn)) {}

    kj::Promise<void> call(CallContext context) override
    {
        fn(context.getParams().getMessage());
        return kj::READY_NOW;
    }

    Node::InitMessageFn fn;
};

class MessageBoxCallbackServer final : public messages::MessageBoxCallback::Server
{
public:
    MessageBoxCallbackServer(Node::MessageBoxFn fn) : fn(std::move(fn)) {}

    kj::Promise<void> call(CallContext context) override
    {
        fn(context.getParams().getMessage(), context.getParams().getCaption(), context.getParams().getStyle());
        return kj::READY_NOW;
    }

    Node::MessageBoxFn fn;
};

class QuestionCallbackServer final : public messages::QuestionCallback::Server
{
public:
    QuestionCallbackServer(Node::QuestionFn fn) : fn(std::move(fn)) {}

    kj::Promise<void> call(CallContext context) override
    {
        fn(context.getParams().getMessage(), context.getParams().getNonInteractiveMessage(),
            context.getParams().getCaption(), context.getParams().getStyle());
        return kj::READY_NOW;
    }

    Node::QuestionFn fn;
};

class LoadWalletCallbackServer final : public messages::LoadWalletCallback::Server
{
public:
    LoadWalletCallbackServer(EventLoop& loop, Node::LoadWalletFn fn) : loop(loop), fn(std::move(fn)) {}

    kj::Promise<void> call(CallContext context) override
    {
        fn(MakeUnique<WalletImpl>(loop, context.getParams().getWallet()));
        return kj::READY_NOW;
    }

    EventLoop& loop;
    Node::LoadWalletFn fn;
};

class NotifyNumConnectionsChangedCallbackServer final : public messages::NotifyNumConnectionsChangedCallback::Server
{
public:
    NotifyNumConnectionsChangedCallbackServer(Node::NotifyNumConnectionsChangedFn fn) : fn(std::move(fn)) {}

    kj::Promise<void> call(CallContext context) override
    {
        fn(context.getParams().getNewNumConnections());
        return kj::READY_NOW;
    }

    Node::NotifyNumConnectionsChangedFn fn;
};

class NotifyNetworkActiveChangedCallbackServer final : public messages::NotifyNetworkActiveChangedCallback::Server
{
public:
    NotifyNetworkActiveChangedCallbackServer(Node::NotifyNetworkActiveChangedFn fn) : fn(std::move(fn)) {}

    kj::Promise<void> call(CallContext context) override
    {
        fn(context.getParams().getNetworkActive());
        return kj::READY_NOW;
    }

    Node::NotifyNetworkActiveChangedFn fn;
};

class NotifyAlertChangedCallbackServer final : public messages::NotifyAlertChangedCallback::Server
{
public:
    NotifyAlertChangedCallbackServer(Node::NotifyAlertChangedFn fn) : fn(std::move(fn)) {}

    kj::Promise<void> call(CallContext context) override
    {
        fn();
        return kj::READY_NOW;
    }

    Node::NotifyAlertChangedFn fn;
};

class BannedListChangedCallbackServer final : public messages::BannedListChangedCallback::Server
{
public:
    BannedListChangedCallbackServer(Node::BannedListChangedFn fn) : fn(std::move(fn)) {}

    kj::Promise<void> call(CallContext context) override
    {
        fn();
        return kj::READY_NOW;
    }

    Node::BannedListChangedFn fn;
};

class NotifyBlockTipCallbackServer final : public messages::NotifyBlockTipCallback::Server
{
public:
    NotifyBlockTipCallbackServer(Node::NotifyBlockTipFn fn) : fn(std::move(fn)) {}

    kj::Promise<void> call(CallContext context) override
    {
        fn(context.getParams().getInitialDownload(), context.getParams().getHeight(),
            context.getParams().getBlockTime(), context.getParams().getVerificationProgress());
        return kj::READY_NOW;
    }

    Node::NotifyBlockTipFn fn;
};

class NotifyHeaderTipCallbackServer final : public messages::NotifyHeaderTipCallback::Server
{
public:
    NotifyHeaderTipCallbackServer(Node::NotifyHeaderTipFn fn) : fn(std::move(fn)) {}

    kj::Promise<void> call(CallContext context) override
    {
        fn(context.getParams().getInitialDownload(), context.getParams().getHeight(),
            context.getParams().getBlockTime(), context.getParams().getVerificationProgress());
        return kj::READY_NOW;
    }

    Node::NotifyHeaderTipFn fn;
};

//! VatId for server side of IPC connection.
struct ServerVatId
{
    ::capnp::word scratch[4]{};
    ::capnp::MallocMessageBuilder message{scratch};
    ::capnp::rpc::twoparty::VatId::Builder vatId{message.getRoot<::capnp::rpc::twoparty::VatId>()};
    ServerVatId() { vatId.setSide(::capnp::rpc::twoparty::Side::SERVER); }
};

class NodeImpl : public Node
{
public:
    NodeImpl(EventLoop& loop, int fd)
        : loop(loop),
          stream(loop.ioContext.lowLevelProvider->wrapSocketFd(fd, kj::LowLevelAsyncIoProvider::TAKE_OWNERSHIP))
    {
    }
    ~NodeImpl() noexcept override {}

    void parseParameters(int argc, const char* const argv[]) override
    {
        ::ParseParameters(argc, argv);
        auto call = MakeCall(loop, [&]() {
            auto request = client.parseParametersRequest();
            auto args = request.initArgv(argc);
            for (int i = 0; i < argc; ++i) {
                args.set(i, argv[i]);
            }
            return request;
        });
        return call.send();
    }
    bool softSetArg(const std::string& arg, const std::string& value) override
    {
        ::SoftSetArg(arg, value);
        auto call = MakeCall(loop, [&]() {
            auto request = client.softSetArgRequest();
            request.setArg(arg);
            request.setValue(value);
            return request;
        });
        return call.send([&]() { return call.response->getResult(); });
    }
    bool softSetBoolArg(const std::string& arg, bool value) override
    {
        ::SoftSetBoolArg(arg, value);
        auto call = MakeCall(loop, [&]() {
            auto request = client.softSetBoolArgRequest();
            request.setArg(arg);
            request.setValue(value);
            return request;
        });
        return call.send([&]() { return call.response->getResult(); });
    }
    void readConfigFile(const std::string& confPath) override
    {
        ::ReadConfigFile(confPath);
        auto call = MakeCall(loop, [&]() {
            auto request = client.readConfigFileRequest();
            request.setConfPath(confPath);
            return request;
        });
        return call.send();
    }
    void selectParams(const std::string& network) override
    {
        ::SelectParams(network);
        auto call = MakeCall(loop, [&]() {
            auto request = client.selectParamsRequest();
            request.setNetwork(network);
            return request;
        });
        return call.send();
    }
    std::string getNetwork() override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.getNetworkRequest();

            return request;
        });
        return call.send([&]() { return call.response->getResult(); });
    }
    void initLogging() override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.initLoggingRequest();

            return request;
        });
        return call.send();
    }
    void initParameterInteraction() override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.initParameterInteractionRequest();

            return request;
        });
        return call.send();
    }
    std::string getWarnings(const std::string& type) override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.getWarningsRequest();
            request.setType(type);
            return request;
        });
        return call.send([&]() { return call.response->getResult(); });
    }
    uint32_t getLogCategories() override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.getLogCategoriesRequest();

            return request;
        });
        return call.send([&]() { return call.response->getResult(); });
    }
    bool appInit() override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.appInitRequest();

            return request;
        });
        return call.send([&]() { return call.response->getResult(); });
    }
    void appShutdown() override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.appShutdownRequest();

            return request;
        });
        return call.send();
    }
    void startShutdown() override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.startShutdownRequest();

            return request;
        });
        return call.send();
    }
    bool shutdownRequested() override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.shutdownRequestedRequest();

            return request;
        });
        return call.send([&]() { return call.response->getResult(); });
    }
    std::string helpMessage(HelpMessageMode mode) override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.helpMessageRequest();
            request.setMode(mode);
            return request;
        });
        return call.send([&]() { return call.response->getResult(); });
    }
    void mapPort(bool useUPnP) override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.mapPortRequest();
            request.setUseUPnP(useUPnP);
            return request;
        });
        return call.send();
    }
    bool getProxy(Network net, proxyType& proxyInfo) override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.getProxyRequest();
            request.setNet(net);
            return request;
        });
        return call.send([&]() {
            // TODO proxyInfo = call.response->getProxyInfo();
            return call.response->getResult();
        });
    }
    size_t getNodeCount(CConnman::NumConnections flags) override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.getNodeCountRequest();
            request.setFlags(flags);
            return request;
        });
        return call.send([&]() { return call.response->getResult(); });
    }
    bool getNodesStats(NodesStats& stats) override
    {
        auto call = MakeCall(loop, [&]() { return client.getNodesStatsRequest(); });
        return call.send([&]() {
            stats.clear();
            if (call.response->hasStats()) {
                auto resultStats = call.response->getStats();
                stats.reserve(resultStats.size());
                for (const auto& resultNodeStats : resultStats) {
                    stats.emplace_back(CNodeStats(), false, CNodeStateStats());
                    ReadNodeStats(std::get<0>(stats.back()), resultNodeStats);
                    if (resultNodeStats.hasStateStats()) {
                        std::get<1>(stats.back()) = true;
                        ReadNodeStateStats(std::get<2>(stats.back()), resultNodeStats.getStateStats());
                    }
                }
                return true;
            }
            return false;
        });
    }
    bool getBanned(banmap_t& banMap) override
    {
        auto call = MakeCall(loop, [&]() { return client.getBannedRequest(); });
        return call.send([&]() {
            if (call.response->hasBanMap()) {
                for (const auto& entry : call.response->getBanMap().getEntries()) {
                    banMap.emplace(
                        Unserialize<CSubNet>(entry.getSubnet()), Unserialize<CBanEntry>(entry.getBanEntry()));
                }
                return true;
            }
            return false;
        });
    }
    bool ban(const CNetAddr& netAddr, BanReason reason, int64_t bantimeoffset) override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.banRequest();
            request.setNetAddr(ToArray(Serialize(netAddr)));
            request.setReason(reason);
            request.setBantimeoffset(bantimeoffset);
            return request;
        });
        return call.send([&]() { return call.response->getResult(); });
    }
    bool unban(const CSubNet& ip) override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.unbanRequest();
            request.setIp(ToArray(Serialize((ip))));
            return request;
        });
        return call.send([&]() { return call.response->getResult(); });
    }
    bool disconnect(NodeId id) override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.disconnectRequest();
            request.setId(id);
            return request;
        });
        return call.send([&]() { return call.response->getResult(); });
    }
    int64_t getTotalBytesRecv() override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.getTotalBytesRecvRequest();

            return request;
        });
        return call.send([&]() { return call.response->getResult(); });
    }
    int64_t getTotalBytesSent() override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.getTotalBytesSentRequest();

            return request;
        });
        return call.send([&]() { return call.response->getResult(); });
    }
    size_t getMempoolSize() override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.getMempoolSizeRequest();

            return request;
        });
        return call.send([&]() { return call.response->getResult(); });
    }
    size_t getMempoolDynamicUsage() override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.getMempoolDynamicUsageRequest();

            return request;
        });
        return call.send([&]() { return call.response->getResult(); });
    }
    bool getHeaderTip(int& height, int64_t& blockTime) override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.getHeaderTipRequest();

            return request;
        });
        return call.send([&]() {
            height = call.response->getHeight();
            blockTime = call.response->getBlockTime();
            return call.response->getResult();
        });
    }
    int getNumBlocks() override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.getNumBlocksRequest();

            return request;
        });
        return call.send([&]() { return call.response->getResult(); });
    }
    int64_t getLastBlockTime() override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.getLastBlockTimeRequest();

            return request;
        });
        return call.send([&]() { return call.response->getResult(); });
    }
    double getVerificationProgress() override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.getVerificationProgressRequest();

            return request;
        });
        return call.send([&]() { return call.response->getResult(); });
    }
    bool isInitialBlockDownload() override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.isInitialBlockDownloadRequest();

            return request;
        });
        return call.send([&]() { return call.response->getResult(); });
    }
    bool getReindex() override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.getReindexRequest();

            return request;
        });
        return call.send([&]() { return call.response->getResult(); });
    }
    bool getImporting() override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.getImportingRequest();

            return request;
        });
        return call.send([&]() { return call.response->getResult(); });
    }
    void setNetworkActive(bool active) override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.setNetworkActiveRequest();
            request.setActive(active);
            return request;
        });
        return call.send();
    }
    bool getNetworkActive() override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.getNetworkActiveRequest();

            return request;
        });
        return call.send([&]() { return call.response->getResult(); });
    }
    unsigned int getTxConfirmTarget() override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.getTxConfirmTargetRequest();

            return request;
        });
        return call.send([&]() { return call.response->getResult(); });
    }
    bool getWalletRbf() override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.getWalletRbfRequest();

            return request;
        });
        return call.send([&]() { return call.response->getResult(); });
    }
    CAmount getRequiredFee(unsigned int txBytes) override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.getRequiredFeeRequest();
            request.setTxBytes(txBytes);
            return request;
        });
        return call.send([&]() { return call.response->getResult(); });
    }
    CAmount getMinimumFee(unsigned int txBytes) override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.getMinimumFeeRequest();
            request.setTxBytes(txBytes);
            return request;
        });
        return call.send([&]() { return call.response->getResult(); });
    }
    CAmount getMaxTxFee() override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.getMaxTxFeeRequest();

            return request;
        });
        return call.send([&]() { return call.response->getResult(); });
    }
    CFeeRate estimateSmartFee(int nBlocks, int* answerFoundAtBlocks) override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.estimateSmartFeeRequest();
            request.setNBlocks(nBlocks);
            return request;
        });
        return call.send([&]() {
            if (answerFoundAtBlocks) {
                *answerFoundAtBlocks = call.response->getAnswerFoundAtBlocks();
            }
            return Unserialize<CFeeRate>(call.response->getResult());
        });
    }
    CFeeRate getDustRelayFee() override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.getDustRelayFeeRequest();

            return request;
        });
        return call.send([&]() { return Unserialize<CFeeRate>(call.response->getResult()); });
    }
    CFeeRate getFallbackFee() override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.getFallbackFeeRequest();

            return request;
        });
        return call.send([&]() { return Unserialize<CFeeRate>(call.response->getResult()); });
    }
    CFeeRate getPayTxFee() override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.getPayTxFeeRequest();

            return request;
        });
        return call.send([&]() { return Unserialize<CFeeRate>(call.response->getResult()); });
    }
    void setPayTxFee(CFeeRate rate) override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.setPayTxFeeRequest();
            request.setRate(ToArray(Serialize(rate)));
            return request;
        });
        return call.send();
    }
    UniValue executeRpc(const std::string& command, const UniValue& params) override
    {
        UniValue result;
        auto call = MakeCall(loop, [&]() {
            auto request = client.executeRpcRequest();
            request.setCommand(command);
            request.setParams(params.write());
            return request;
        });
        call.send([&]() { result.read(ToString(call.response->getResult())); });
        return result;
    }
    std::vector<std::string> listRpcCommands() override
    {
        std::vector<std::string> result;
        auto call = MakeCall(loop, [&]() {
            auto request = client.listRpcCommandsRequest();
            return request;
        });
        call.send([&]() { // TODO call.response->getResult();
        });
        return result;
    }
    void rpcSetTimerInterfaceIfUnset(RPCTimerInterface* iface) override
    {
        // Ignore interface argument when using Cap'n Proto. It's simpler and
        // more efficient to just implemented the interface on the server side.
        auto call = MakeCall(loop, [&]() {
            auto request = client.rpcSetTimerInterfaceIfUnsetRequest();
            return request;
        });
        return call.send();
    }
    void rpcUnsetTimerInterface(RPCTimerInterface* iface) override
    {
        // Ignore interface argument when using Cap'n Proto. It's simpler and
        // more efficient to just implemented the interface on the server side.
        auto call = MakeCall(loop, [&]() {
            auto request = client.rpcUnsetTimerInterfaceRequest();
            return request;
        });
        return call.send();
    }
    bool getUnspentOutputs(const uint256& txHash, CCoins& coins) override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.getUnspentOutputsRequest();
            request.setTxHash(ToArray(txHash));
            return request;
        });
        return call.send([&]() {
            coins = Unserialize<CCoins>(call.response->getCoins());
            return call.response->getResult();
        });
    }
    std::unique_ptr<Wallet> getWallet() override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.getWalletRequest();

            return request;
        });
        return call.send([&]() { return MakeUnique<WalletImpl>(loop, call.response->getResult()); });
    }
    std::unique_ptr<Handler> handleInitMessage(InitMessageFn fn) override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.handleInitMessageRequest();
            request.setCallback(kj::heap<InitMessageCallbackServer>(std::move(fn)));
            return request;
        });
        return call.send([&]() { return MakeUnique<HandlerImpl>(loop, call.response->getHandler()); });
    }
    std::unique_ptr<Handler> handleMessageBox(MessageBoxFn fn) override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.handleMessageBoxRequest();
            request.setCallback(kj::heap<MessageBoxCallbackServer>(std::move(fn)));
            return request;
        });
        return call.send([&]() { return MakeUnique<HandlerImpl>(loop, call.response->getHandler()); });
    }
    std::unique_ptr<Handler> handleQuestion(QuestionFn fn) override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.handleQuestionRequest();
            request.setCallback(kj::heap<QuestionCallbackServer>(std::move(fn)));
            return request;
        });
        return call.send([&]() { return MakeUnique<HandlerImpl>(loop, call.response->getHandler()); });
    }
    std::unique_ptr<Handler> handleShowProgress(ShowProgressFn fn) override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.handleShowProgressRequest();
            request.setCallback(kj::heap<ShowProgressCallbackServer>(std::move(fn)));
            return request;
        });
        return call.send([&]() { return MakeUnique<HandlerImpl>(loop, call.response->getHandler()); });
    }
    std::unique_ptr<Handler> handleLoadWallet(LoadWalletFn fn) override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.handleLoadWalletRequest();
            request.setCallback(kj::heap<LoadWalletCallbackServer>(loop, std::move(fn)));
            return request;
        });
        return call.send([&]() { return MakeUnique<HandlerImpl>(loop, call.response->getHandler()); });
    }
    std::unique_ptr<Handler> handleNotifyNumConnectionsChanged(NotifyNumConnectionsChangedFn fn) override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.handleNotifyNumConnectionsChangedRequest();
            request.setCallback(kj::heap<NotifyNumConnectionsChangedCallbackServer>(std::move(fn)));
            return request;
        });
        return call.send([&]() { return MakeUnique<HandlerImpl>(loop, call.response->getHandler()); });
    }
    std::unique_ptr<Handler> handleNotifyNetworkActiveChanged(NotifyNetworkActiveChangedFn fn) override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.handleNotifyNetworkActiveChangedRequest();
            request.setCallback(kj::heap<NotifyNetworkActiveChangedCallbackServer>(std::move(fn)));
            return request;
        });
        return call.send([&]() { return MakeUnique<HandlerImpl>(loop, call.response->getHandler()); });
    }
    std::unique_ptr<Handler> handleNotifyAlertChanged(NotifyAlertChangedFn fn) override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.handleNotifyAlertChangedRequest();
            request.setCallback(kj::heap<NotifyAlertChangedCallbackServer>(std::move(fn)));
            return request;
        });
        return call.send([&]() { return MakeUnique<HandlerImpl>(loop, call.response->getHandler()); });
    }
    std::unique_ptr<Handler> handleBannedListChanged(BannedListChangedFn fn) override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.handleBannedListChangedRequest();
            request.setCallback(kj::heap<BannedListChangedCallbackServer>(std::move(fn)));
            return request;
        });
        return call.send([&]() { return MakeUnique<HandlerImpl>(loop, call.response->getHandler()); });
    }
    std::unique_ptr<Handler> handleNotifyBlockTip(NotifyBlockTipFn fn) override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.handleNotifyBlockTipRequest();
            request.setCallback(kj::heap<NotifyBlockTipCallbackServer>(std::move(fn)));
            return request;
        });
        return call.send([&]() { return MakeUnique<HandlerImpl>(loop, call.response->getHandler()); });
    }
    std::unique_ptr<Handler> handleNotifyHeaderTip(NotifyHeaderTipFn fn) override
    {
        auto call = MakeCall(loop, [&]() {
            auto request = client.handleNotifyHeaderTipRequest();
            request.setCallback(kj::heap<NotifyHeaderTipCallbackServer>(std::move(fn)));
            return request;
        });
        return call.send([&]() { return MakeUnique<HandlerImpl>(loop, call.response->getHandler()); });
    }

    EventLoop& loop;
    kj::Own<kj::AsyncIoStream> stream;
    ::capnp::TwoPartyVatNetwork network{*stream, ::capnp::rpc::twoparty::Side::CLIENT, ::capnp::ReaderOptions()};
    ::capnp::RpcSystem<::capnp::rpc::twoparty::VatId> rpcClient{::capnp::makeRpcClient(network)};
    messages::Node::Client client{rpcClient.bootstrap(ServerVatId().vatId).castAs<messages::Node>()};
};

} // namespace

std::unique_ptr<Node> MakeNode(int fd)
{
    std::promise<EventLoop*> loopPromise;
    std::thread thread([&loopPromise, &thread]() {
        EventLoop loop(std::move(thread));
        loopPromise.set_value(&loop);
        loop.loop();
    });
    EventLoop* loop = loopPromise.get_future().get();

    std::promise<std::unique_ptr<Node>> nodePromise;
    loop->sync([&] { nodePromise.set_value(MakeUnique<NodeImpl>(*loop, fd)); });
    return nodePromise.get_future().get();
}

} // namespace capnp
} // namespace ipc
