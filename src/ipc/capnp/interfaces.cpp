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
#include "wallet/wallet.h"

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

struct Connection
{
    Connection(EventLoop& loop, int fd)
        : m_stream(loop.m_io_context.lowLevelProvider->wrapSocketFd(fd, kj::LowLevelAsyncIoProvider::TAKE_OWNERSHIP)),
          m_network(*m_stream, ::capnp::rpc::twoparty::Side::CLIENT, ::capnp::ReaderOptions()),
          m_rpc_client(::capnp::makeRpcClient(m_network))
    {
    }

    kj::Own<kj::AsyncIoStream> m_stream;
    ::capnp::TwoPartyVatNetwork m_network;
    ::capnp::RpcSystem<::capnp::rpc::twoparty::VatId> m_rpc_client;
};

template <typename Client>
void DestroyClient(EventLoop& loop, Client client, kj::Own<Connection> connection = {})
{
    loop.sync([&]() {
        Client(std::move(client));
        if (connection) {
            connection = nullptr;
        }
    });
}

class HandlerImpl : public Handler
{
public:
    HandlerImpl(EventLoop& loop, messages::Handler::Client client) : m_loop(loop), m_client(std::move(client)) {}
    ~HandlerImpl() noexcept override { DestroyClient(m_loop, std::move(m_client)); }

    void disconnect() override
    {
        auto call = MakeCall(m_loop, [&]() { return m_client.disconnectRequest(); });
        return call.send();
    }

    EventLoop& m_loop;
    messages::Handler::Client m_client;
};

class ShowProgressCallbackServer final : public messages::ShowProgressCallback::Server
{
public:
    ShowProgressCallbackServer(Wallet::ShowProgressFn fn) : m_fn(std::move(fn)) {}

    kj::Promise<void> call(CallContext context) override
    {
        m_fn(context.getParams().getTitle(), context.getParams().getProgress());
        return kj::READY_NOW;
    }

    Wallet::ShowProgressFn m_fn;
};

class StatusChangedCallbackServer final : public messages::StatusChangedCallback::Server
{
public:
    StatusChangedCallbackServer(Wallet::StatusChangedFn fn) : m_fn(std::move(fn)) {}

    kj::Promise<void> call(CallContext context) override
    {
        m_fn();
        return kj::READY_NOW;
    }

    Wallet::StatusChangedFn m_fn;
};

class AddressBookChangedCallbackServer final : public messages::AddressBookChangedCallback::Server
{
public:
    AddressBookChangedCallbackServer(Wallet::AddressBookChangedFn fn) : m_fn(std::move(fn)) {}

    kj::Promise<void> call(CallContext context) override
    {
        auto params = context.getParams();
        CTxDestination address;
        ReadTxDestination(address, params.getAddress());
        m_fn(address, ToString(params.getLabel()), params.getIsMine(), ToString(params.getPurpose()),
            ChangeType(params.getStatus()));
        return kj::READY_NOW;
    }

    Wallet::AddressBookChangedFn m_fn;
};

class TransactionChangedCallbackServer final : public messages::TransactionChangedCallback::Server
{
public:
    TransactionChangedCallbackServer(Wallet::TransactionChangedFn fn) : m_fn(std::move(fn)) {}

    kj::Promise<void> call(CallContext context) override
    {
        m_fn(ToBlob<uint256>(context.getParams().getTxid()), ChangeType(context.getParams().getStatus()));
        return kj::READY_NOW;
    }

    Wallet::TransactionChangedFn m_fn;
};

class WatchonlyChangedCallbackServer final : public messages::WatchonlyChangedCallback::Server
{
public:
    WatchonlyChangedCallbackServer(Wallet::WatchOnlyChangedFn fn) : m_fn(std::move(fn)) {}

    kj::Promise<void> call(CallContext context) override
    {
        m_fn(context.getParams().getHaveWatchOnly());
        return kj::READY_NOW;
    }

    Wallet::WatchOnlyChangedFn m_fn;
};

class PendingWalletTxImpl : public PendingWalletTx
{
public:
    PendingWalletTxImpl(EventLoop& loop, messages::PendingWalletTx::Client client)
        : m_loop(loop), m_client(std::move(client))
    {
    }
    ~PendingWalletTxImpl() noexcept override { DestroyClient(m_loop, std::move(m_client)); }

    const CTransaction& get() override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.getRequest();
            return request;
        });
        call.send([&]() {
            auto data = call.m_response->getResult();
            CDataStream stream(reinterpret_cast<const char*>(data.begin()), reinterpret_cast<const char*>(data.end()),
                SER_NETWORK, CLIENT_VERSION);
            m_tx = MakeUnique<CTransaction>(deserialize, stream);
        });
        return *m_tx;
    }
    int64_t getVirtualSize() override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.getVirtualSizeRequest();
            return request;
        });
        return call.send([&]() { return call.m_response->getResult(); });
    }
    bool commit(WalletValueMap valueMap,
        WalletOrderForm order_form,
        std::string from_account,
        std::string& reject_reason) override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.commitRequest();
            BuildWalletValueMap(request.initValueMap(), valueMap);
            BuildWalletOrderForm(request.initOrderForm(), order_form);
            request.setFromAccount(from_account);
            return request;
        });
        return call.send([&]() {
            reject_reason = ToString(call.m_response->getRejectReason());
            return call.m_response->getResult();
        });
    }

    EventLoop& m_loop;
    messages::PendingWalletTx::Client m_client;
    std::unique_ptr<CTransaction> m_tx;
};


class WalletImpl : public Wallet
{
public:
    WalletImpl(EventLoop& loop, messages::Wallet::Client client) : m_loop(loop), m_client(std::move(client)) {}
    ~WalletImpl() noexcept override { DestroyClient(m_loop, std::move(m_client)); }

    bool encryptWallet(const SecureString& wallet_passphrase) override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.encryptWalletRequest();
            request.setWalletPassphrase(ToArray(wallet_passphrase));
            return request;
        });
        return call.send([&]() { return call.m_response->getResult(); });
    }
    bool isCrypted() override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.isCryptedRequest();
            return request;
        });
        return call.send([&]() { return call.m_response->getResult(); });
    }
    bool lock() override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.lockRequest();
            return request;
        });
        return call.send([&]() { return call.m_response->getResult(); });
    }
    bool unlock(const SecureString& wallet_passphrase) override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.unlockRequest();
            request.setWalletPassphrase(ToArray(wallet_passphrase));
            return request;
        });
        return call.send([&]() { return call.m_response->getResult(); });
    }
    bool isLocked() override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.isLockedRequest();
            return request;
        });
        return call.send([&]() { return call.m_response->getResult(); });
    }
    bool changeWalletPassphrase(const SecureString& old_wallet_passphrase,
        const SecureString& new_wallet_passphrase) override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.changeWalletPassphraseRequest();
            request.setOldWalletPassphrase(ToArray(old_wallet_passphrase));
            request.setNewWalletPassphrase(ToArray(new_wallet_passphrase));
            return request;
        });
        return call.send([&]() { return call.m_response->getResult(); });
    }
    bool backupWallet(const std::string& filename) override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.backupWalletRequest();
            request.setFilename(filename);
            return request;
        });
        return call.send([&]() { return call.m_response->getResult(); });
    }
    bool getKeyFromPool(bool internal, CPubKey& pub_key) override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.getKeyFromPoolRequest();
            request.setInternal(internal);
            return request;
        });
        return call.send([&]() {
            pub_key = Unserialize<CPubKey>(call.m_response->getPubKey());
            return call.m_response->getResult();
        });
    }
    bool getPubKey(const CKeyID& address, CPubKey& pub_key) override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.getPubKeyRequest();
            request.setAddress(ToArray(Serialize(address)));
            return request;
        });
        return call.send([&]() {
            pub_key = Unserialize<CPubKey>(call.m_response->getPubKey());
            return call.m_response->getResult();
        });
    }
    bool getPrivKey(const CKeyID& address, CKey& key) override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.getPrivKeyRequest();
            request.setAddress(ToArray(Serialize(address)));
            return request;
        });
        return call.send([&]() {
            ReadKey(key, call.m_response->getKey());
            return call.m_response->getResult();
        });
    }
    bool havePrivKey(const CKeyID& address) override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.havePrivKeyRequest();
            request.setAddress(ToArray(Serialize((address))));
            return request;
        });
        return call.send([&]() { return call.m_response->getResult(); });
    }
    bool haveWatchOnly() override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.haveWatchOnlyRequest();
            return request;
        });
        return call.send([&]() { return call.m_response->getResult(); });
    }
    bool setAddressBook(const CTxDestination& dest, const std::string& name, const std::string& purpose) override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.setAddressBookRequest();
            BuildTxDestination(request.initDest(), dest);
            request.setName(name);
            request.setPurpose(purpose);
            return request;
        });
        return call.send([&]() { return call.m_response->getResult(); });
    }
    bool delAddressBook(const CTxDestination& dest) override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.delAddressBookRequest();
            BuildTxDestination(request.initDest(), dest);
            return request;
        });
        return call.send([&]() { return call.m_response->getResult(); });
    }
    bool getAddress(const CTxDestination& dest, std::string* name, isminetype* ismine) override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.getAddressRequest();
            BuildTxDestination(request.initDest(), dest);
            return request;
        });
        return call.send([&]() {
            if (name) {
                *name = ToString(call.m_response->getName());
            }
            if (ismine) {
                *ismine = isminetype(call.m_response->getIsMine());
            }
            return call.m_response->getResult();
        });
    }
    std::vector<WalletAddress> getAddresses() override
    {
        std::vector<WalletAddress> result;
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.getAddressesRequest();
            return request;
        });
        call.send([&]() {
            result.reserve(call.m_response->getResult().size());
            for (const auto& address : call.m_response->getResult()) {
                result.emplace_back();
                ReadWalletAddress(result.back(), address);
            }
        });
        return result;
    }
    std::set<CTxDestination> getAccountAddresses(const std::string& account) override
    {
        std::set<CTxDestination> result;
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.getAccountAddressesRequest();
            request.setAccount(account);
            return request;
        });
        call.send([&]() {
            for (const auto& dest : call.m_response->getResult()) {
                CTxDestination resultDest;
                ReadTxDestination(resultDest, dest);
                result.emplace(std::move(resultDest));
            }
        });
        return result;
    }
    bool addDestData(const CTxDestination& dest, const std::string& key, const std::string& value) override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.addDestDataRequest();
            BuildTxDestination(request.initDest(), dest);
            request.setKey(key);
            request.setValue(value);
            return request;
        });
        return call.send([&]() { return call.m_response->getResult(); });
    }
    bool eraseDestData(const CTxDestination& dest, const std::string& key) override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.eraseDestDataRequest();
            BuildTxDestination(request.initDest(), dest);
            request.setKey(key);
            return request;
        });
        return call.send([&]() { return call.m_response->getResult(); });
    }
    std::vector<std::string> getDestValues(const std::string& prefix) override
    {
        std::vector<std::string> result;
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.getDestValuesRequest();
            request.setPrefix(prefix);
            return request;
        });
        call.send([&]() {
            result.clear();
            result.reserve(call.m_response->getResult().size());
            for (const auto& value : call.m_response->getResult()) {
                result.emplace_back(ToString(value));
            }
        });
        return result;
    }
    void lockCoin(const COutPoint& output) override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.lockCoinRequest();
            request.setOutput(ToArray(Serialize(output)));
            return request;
        });
        return call.send();
    }
    void unlockCoin(const COutPoint& output) override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.unlockCoinRequest();
            request.setOutput(ToArray(Serialize(output)));
            return request;
        });
        return call.send();
    }
    bool isLockedCoin(const COutPoint& output) override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.isLockedCoinRequest();
            request.setOutput(ToArray(Serialize(output)));
            return request;
        });
        return call.send([&]() { return call.m_response->getResult(); });
    }
    void listLockedCoins(std::vector<COutPoint>& outputs) override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.listLockedCoinsRequest();
            return request;
        });
        return call.send([&]() {
            outputs.clear();
            outputs.reserve(call.m_response->getOutputs().size());
            for (const auto& output : call.m_response->getOutputs()) {
                outputs.emplace_back();
                ipc::capnp::Unserialize(outputs.back(), output);
            }
        });
    }
    std::unique_ptr<PendingWalletTx> createTransaction(const std::vector<CRecipient>& recipients,
        const CCoinControl& coin_control,
        bool sign,
        int& change_pos,
        CAmount& fee,
        std::string& fail_reason) override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.createTransactionRequest();
            auto requestRecipients = request.initRecipients(recipients.size());
            size_t i = 0;
            for (const auto& recipient : recipients) {
                BuildRecipient(requestRecipients[i], recipient);
                ++i;
            }
            BuildCoinControl(request.initCoinControl(), coin_control);
            request.setChangePos(change_pos);
            request.setSign(sign);
            return request;
        });
        return call.send([&]() {
            change_pos = call.m_response->getChangePos();
            fee = call.m_response->getFee();
            fail_reason = call.m_response->getFailReason();
            return call.m_response->hasResult() ?
                       MakeUnique<PendingWalletTxImpl>(m_loop, call.m_response->getResult()) :
                       std::unique_ptr<PendingWalletTx>();
        });
    }
    bool transactionCanBeAbandoned(const uint256& txid) override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.transactionCanBeAbandonedRequest();
            request.setTxid(FromBlob(txid));
            return request;
        });
        return call.send([&]() { return call.m_response->getResult(); });
    }
    bool abandonTransaction(const uint256& txid) override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.abandonTransactionRequest();
            request.setTxid(FromBlob(txid));
            return request;
        });
        return call.send([&]() { return call.m_response->getResult(); });
    }
    bool transactionCanBeBumped(const uint256& txid) override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.transactionCanBeBumpedRequest();
            request.setTxid(FromBlob(txid));
            return request;
        });
        return call.send([&]() { return call.m_response->getResult(); });
    }
    bool createBumpTransaction(const uint256& txid,
        const CCoinControl& coin_control,
        CAmount total_fee,
        std::vector<std::string>& errors,
        CAmount& old_fee,
        CAmount& new_fee,
        CMutableTransaction& mtx) override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.createBumpTransactionRequest();
            request.setTxid(FromBlob(txid));
            BuildCoinControl(request.initCoinControl(), coin_control);
            request.setTotalFee(total_fee);
            return request;
        });
        return call.send([&]() {
            errors.clear();
            for (const auto& error : call.m_response->getErrors()) {
                errors.emplace_back(ToString(error));
            }
            old_fee = call.m_response->getOldFee();
            new_fee = call.m_response->getNewFee();
            mtx = Unserialize<CMutableTransaction>(call.m_response->getMtx());
            return call.m_response->getResult();
        });
    }
    bool signBumpTransaction(CMutableTransaction& mtx) override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.signBumpTransactionRequest();
            request.setMtx(ToArray(Serialize(mtx)));
            return request;
        });
        return call.send([&]() {
            mtx = Unserialize<CMutableTransaction>(call.m_response->getMtx());
            return call.m_response->getResult();
        });
    }
    bool commitBumpTransaction(const uint256& txid,
        CMutableTransaction&& mtx,
        std::vector<std::string>& errors,
        uint256& bumped_txid) override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.commitBumpTransactionRequest();
            request.setTxid(FromBlob(txid));
            request.setMtx(ToArray(Serialize(mtx)));
            return request;
        });
        return call.send([&]() {
            errors.clear();
            for (const auto& error : call.m_response->getErrors()) {
                errors.emplace_back(ToString(error));
            }
            bumped_txid = ToBlob<uint256>(call.m_response->getBumpedTxid());
            return call.m_response->getResult();
        });
    }
    CTransactionRef getTx(const uint256& txid) override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.getTxRequest();
            request.setTxid(FromBlob(txid));
            return request;
        });
        return call.send([&]() {
            // TODO dedup
            auto data = call.m_response->getResult();
            CDataStream stream(reinterpret_cast<const char*>(data.begin()), reinterpret_cast<const char*>(data.end()),
                SER_NETWORK, CLIENT_VERSION);
            return std::make_shared<CTransaction>(deserialize, stream);
        });
    }
    WalletTx getWalletTx(const uint256& txid) override
    {
        WalletTx result;
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.getWalletTxRequest();
            request.setTxid(FromBlob(txid));
            return request;
        });
        call.send([&]() { ReadWalletTx(result, call.m_response->getResult()); });
        return result;
    }
    std::vector<WalletTx> getWalletTxs() override
    {
        std::vector<WalletTx> result;
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.getWalletTxsRequest();
            return request;
        });
        call.send([&]() {
            result.reserve(call.m_response->getResult().size());
            for (const auto& walletTx : call.m_response->getResult()) {
                result.emplace_back();
                ReadWalletTx(result.back(), walletTx);
            }
        });
        return result;
    }
    bool tryGetTxStatus(const uint256& txid,
        WalletTxStatus& tx_status,
        int& num_blocks,
        int64_t& adjusted_time) override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.tryGetTxStatusRequest();
            request.setTxid(FromBlob(txid));
            return request;
        });
        return call.send([&]() {
            ReadWalletTxStatus(tx_status, call.m_response->getTxStatus());
            num_blocks = call.m_response->getNumBlocks();
            adjusted_time = call.m_response->getAdjustedTime();
            return call.m_response->getResult();
        });
    }
    WalletTx getWalletTxDetails(const uint256& txid,
        WalletTxStatus& tx_status,
        WalletOrderForm& order_form,
        bool& in_mempool,
        int& num_blocks,
        int64_t& adjusted_time) override
    {
        WalletTx result;
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.getWalletTxDetailsRequest();
            request.setTxid(FromBlob(txid));
            return request;
        });
        call.send([&]() {
            ReadWalletTxStatus(tx_status, call.m_response->getTxStatus());
            ReadWalletOrderForm(order_form, call.m_response->getOrderForm());
            in_mempool = call.m_response->getInMempool();
            num_blocks = call.m_response->getNumBlocks();
            adjusted_time = call.m_response->getAdjustedTime();
            ReadWalletTx(result, call.m_response->getResult());
        });
        return result;
    }
    WalletBalances getBalances() override
    {
        WalletBalances result;
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.getBalancesRequest();
            return request;
        });
        call.send([&]() { ReadWalletBalances(result, call.m_response->getResult()); });
        return result;
    }
    bool tryGetBalances(WalletBalances& balances, int& num_blocks) override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.tryGetBalancesRequest();
            return request;
        });
        return call.send([&]() {
            ReadWalletBalances(balances, call.m_response->getBalances());
            num_blocks = call.m_response->getNumBlocks();
            return call.m_response->getResult();
        });
    }
    CAmount getBalance() override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.getBalanceRequest();
            return request;
        });
        return call.send([&]() { return call.m_response->getResult(); });
    }
    CAmount getAvailableBalance(const CCoinControl& coin_control) override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.getAvailableBalanceRequest();
            BuildCoinControl(request.initCoinControl(), coin_control);
            return request;
        });
        return call.send([&]() { return call.m_response->getResult(); });
    }
    isminetype txinIsMine(const CTxIn& txin) override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.txinIsMineRequest();
            request.setTxin(ToArray(Serialize((txin))));
            return request;
        });
        return call.send([&]() { return isminetype(call.m_response->getResult()); });
    }
    isminetype txoutIsMine(const CTxOut& txout) override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.txoutIsMineRequest();
            request.setTxout(ToArray(Serialize(txout)));
            return request;
        });
        return call.send([&]() { return isminetype(call.m_response->getResult()); });
    }
    CAmount getDebit(const CTxIn& txin, isminefilter filter) override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.getDebitRequest();
            request.setTxin(ToArray(Serialize((txin))));
            request.setFilter(filter);
            return request;
        });
        return call.send([&]() { return call.m_response->getResult(); });
    }
    CAmount getCredit(const CTxOut& txout, isminefilter filter) override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.getCreditRequest();
            request.setTxout(ToArray(Serialize(txout)));
            request.setFilter(filter);
            return request;
        });
        return call.send([&]() { return call.m_response->getResult(); });
    }
    CoinsList listCoins() override
    {
        CoinsList result;
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.listCoinsRequest();
            return request;
        });
        call.send([&]() { ReadCoinsList(result, call.m_response->getResult()); });
        return result;
    }
    std::vector<WalletTxOut> getCoins(const std::vector<COutPoint>& outputs) override
    {
        std::vector<WalletTxOut> result;
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.getCoinsRequest();
            auto requestOutputs = request.initOutputs(outputs.size());
            size_t i = 0;
            for (const auto& output : outputs) {
                requestOutputs.set(i, ToArray(Serialize(output)));
                ++i;
            }
            return request;
        });
        call.send([&]() {
            result.reserve(call.m_response->getResult().size());
            for (const auto& coin : call.m_response->getResult()) {
                result.emplace_back();
                ReadWalletTxOut(result.back(), coin);
            }
        });
        return result;
    }
    bool hdEnabled() override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.hdEnabledRequest();
            return request;
        });
        return call.send([&]() { return call.m_response->getResult(); });
    }
    std::unique_ptr<Handler> handleShowProgress(ShowProgressFn fn) override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.handleShowProgressRequest();
            request.setCallback(kj::heap<ShowProgressCallbackServer>(std::move(fn)));
            return request;
        });
        return call.send([&]() { return MakeUnique<HandlerImpl>(m_loop, call.m_response->getHandler()); });
    }
    std::unique_ptr<Handler> handleStatusChanged(StatusChangedFn fn) override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.handleStatusChangedRequest();
            request.setCallback(kj::heap<StatusChangedCallbackServer>(std::move(fn)));
            return request;
        });
        return call.send([&]() { return MakeUnique<HandlerImpl>(m_loop, call.m_response->getHandler()); });
    }
    std::unique_ptr<Handler> handleAddressBookChanged(AddressBookChangedFn fn) override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.handleAddressBookChangedRequest();
            request.setCallback(kj::heap<AddressBookChangedCallbackServer>(std::move(fn)));
            return request;
        });
        return call.send([&]() { return MakeUnique<HandlerImpl>(m_loop, call.m_response->getHandler()); });
    }
    std::unique_ptr<Handler> handleTransactionChanged(TransactionChangedFn fn) override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.handleTransactionChangedRequest();
            request.setCallback(kj::heap<TransactionChangedCallbackServer>(std::move(fn)));
            return request;
        });
        return call.send([&]() { return MakeUnique<HandlerImpl>(m_loop, call.m_response->getHandler()); });
    }
    std::unique_ptr<Handler> handleWatchOnlyChanged(WatchOnlyChangedFn fn) override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.handleWatchOnlyChangedRequest();
            request.setCallback(kj::heap<WatchonlyChangedCallbackServer>(std::move(fn)));
            return request;
        });
        return call.send([&]() { return MakeUnique<HandlerImpl>(m_loop, call.m_response->getHandler()); });
    }

    EventLoop& m_loop;
    messages::Wallet::Client m_client;
};

class InitMessageCallbackServer final : public messages::InitMessageCallback::Server
{
public:
    InitMessageCallbackServer(Node::InitMessageFn fn) : m_fn(std::move(fn)) {}

    kj::Promise<void> call(CallContext context) override
    {
        m_fn(context.getParams().getMessage());
        return kj::READY_NOW;
    }

    Node::InitMessageFn m_fn;
};

class MessageBoxCallbackServer final : public messages::MessageBoxCallback::Server
{
public:
    MessageBoxCallbackServer(EventLoop& loop, Node::MessageBoxFn fn) : m_loop(loop), m_fn(std::move(fn)) {}

    kj::Promise<void> call(CallContext context) override
    {
        return m_loop.async(kj::mvCapture(context, [this](CallContext context) {
            m_fn(context.getParams().getMessage(), context.getParams().getCaption(), context.getParams().getStyle());
        }));
    }

    EventLoop& m_loop;
    Node::MessageBoxFn m_fn;
};

class QuestionCallbackServer final : public messages::QuestionCallback::Server
{
public:
    QuestionCallbackServer(Node::QuestionFn fn) : m_fn(std::move(fn)) {}

    kj::Promise<void> call(CallContext context) override
    {
        m_fn(context.getParams().getMessage(), context.getParams().getNonInteractiveMessage(),
            context.getParams().getCaption(), context.getParams().getStyle());
        return kj::READY_NOW;
    }

    Node::QuestionFn m_fn;
};

class LoadWalletCallbackServer final : public messages::LoadWalletCallback::Server
{
public:
    LoadWalletCallbackServer(EventLoop& loop, Node::LoadWalletFn fn) : m_loop(loop), m_fn(std::move(fn)) {}

    kj::Promise<void> call(CallContext context) override
    {
        auto wallet = MakeUnique<WalletImpl>(m_loop, context.getParams().getWallet());
        context.releaseParams();
        return m_loop.async(
            kj::mvCapture(context, kj::mvCapture(wallet, [this](std::unique_ptr<WalletImpl> wallet,
                                                             CallContext context) { m_fn(std::move(wallet)); })));
    }

    EventLoop& m_loop;
    Node::LoadWalletFn m_fn;
};

class NotifyNumConnectionsChangedCallbackServer final : public messages::NotifyNumConnectionsChangedCallback::Server
{
public:
    NotifyNumConnectionsChangedCallbackServer(Node::NotifyNumConnectionsChangedFn fn) : m_fn(std::move(fn)) {}

    kj::Promise<void> call(CallContext context) override
    {
        m_fn(context.getParams().getNewNumConnections());
        return kj::READY_NOW;
    }

    Node::NotifyNumConnectionsChangedFn m_fn;
};

class NotifyNetworkActiveChangedCallbackServer final : public messages::NotifyNetworkActiveChangedCallback::Server
{
public:
    NotifyNetworkActiveChangedCallbackServer(Node::NotifyNetworkActiveChangedFn fn) : m_fn(std::move(fn)) {}

    kj::Promise<void> call(CallContext context) override
    {
        m_fn(context.getParams().getNetworkActive());
        return kj::READY_NOW;
    }

    Node::NotifyNetworkActiveChangedFn m_fn;
};

class NotifyAlertChangedCallbackServer final : public messages::NotifyAlertChangedCallback::Server
{
public:
    NotifyAlertChangedCallbackServer(Node::NotifyAlertChangedFn fn) : m_fn(std::move(fn)) {}

    kj::Promise<void> call(CallContext context) override
    {
        m_fn();
        return kj::READY_NOW;
    }

    Node::NotifyAlertChangedFn m_fn;
};

class BannedListChangedCallbackServer final : public messages::BannedListChangedCallback::Server
{
public:
    BannedListChangedCallbackServer(Node::BannedListChangedFn fn) : m_fn(std::move(fn)) {}

    kj::Promise<void> call(CallContext context) override
    {
        m_fn();
        return kj::READY_NOW;
    }

    Node::BannedListChangedFn m_fn;
};

class NotifyBlockTipCallbackServer final : public messages::NotifyBlockTipCallback::Server
{
public:
    NotifyBlockTipCallbackServer(Node::NotifyBlockTipFn fn) : m_fn(std::move(fn)) {}

    kj::Promise<void> call(CallContext context) override
    {
        m_fn(context.getParams().getInitialDownload(), context.getParams().getHeight(),
            context.getParams().getBlockTime(), context.getParams().getVerificationProgress());
        return kj::READY_NOW;
    }

    Node::NotifyBlockTipFn m_fn;
};

class NotifyHeaderTipCallbackServer final : public messages::NotifyHeaderTipCallback::Server
{
public:
    NotifyHeaderTipCallbackServer(Node::NotifyHeaderTipFn fn) : m_fn(std::move(fn)) {}

    kj::Promise<void> call(CallContext context) override
    {
        m_fn(context.getParams().getInitialDownload(), context.getParams().getHeight(),
            context.getParams().getBlockTime(), context.getParams().getVerificationProgress());
        return kj::READY_NOW;
    }

    Node::NotifyHeaderTipFn m_fn;
};

class NodeImpl : public Node
{
public:
    NodeImpl(EventLoop& loop, kj::Own<Connection> connection, messages::Node::Client client)
        : m_loop(loop), m_connection(kj::mv(connection)), m_client(std::move(client))
    {
    }
    ~NodeImpl() noexcept override
    {
        DestroyClient(m_loop, std::move(m_client), kj::mv(m_connection));
        m_loop.shutdown();
    }

    void parseParameters(int argc, const char* const argv[]) override
    {
        gArgs.ParseParameters(argc, argv);
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.parseParametersRequest();
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
        gArgs.SoftSetArg(arg, value);
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.softSetArgRequest();
            request.setArg(arg);
            request.setValue(value);
            return request;
        });
        return call.send([&]() { return call.m_response->getResult(); });
    }
    bool softSetBoolArg(const std::string& arg, bool value) override
    {
        gArgs.SoftSetBoolArg(arg, value);
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.softSetBoolArgRequest();
            request.setArg(arg);
            request.setValue(value);
            return request;
        });
        return call.send([&]() { return call.m_response->getResult(); });
    }
    void readConfigFile(const std::string& conf_path) override
    {
        gArgs.ReadConfigFile(conf_path);
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.readConfigFileRequest();
            request.setConfPath(conf_path);
            return request;
        });
        call.send([&]() {
            if (call.m_response->hasError()) {
                throw std::runtime_error(ToString(call.m_response->getError()));
            }
        });
    }
    void selectParams(const std::string& network) override
    {
        ::SelectParams(network);
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.selectParamsRequest();
            request.setNetwork(network);
            return request;
        });
        call.send([&]() {
            if (call.m_response->hasError()) {
                throw std::runtime_error(ToString(call.m_response->getError()));
            }
        });
        return call.send();
    }
    std::string getNetwork() override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.getNetworkRequest();
            return request;
        });
        return call.send([&]() { return std::string(call.m_response->getResult()); });
    }
    void initLogging() override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.initLoggingRequest();
            return request;
        });
        return call.send();
    }
    void initParameterInteraction() override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.initParameterInteractionRequest();
            return request;
        });
        return call.send();
    }
    std::string getWarnings(const std::string& type) override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.getWarningsRequest();
            request.setType(type);
            return request;
        });
        return call.send([&]() { return std::string(call.m_response->getResult()); });
    }
    uint32_t getLogCategories() override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.getLogCategoriesRequest();
            return request;
        });
        return call.send([&]() { return call.m_response->getResult(); });
    }
    bool baseInitialize() override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.baseInitializeRequest();
            return request;
        });
        return call.send([&]() {
            if (call.m_response->hasError()) {
                throw std::runtime_error(ToString(call.m_response->getError()));
            }
            return call.m_response->getResult();
        });
    }
    bool appInitMain() override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.appInitMainRequest();
            return request;
        });
        return call.send([&]() {
            if (call.m_response->hasError()) {
                throw std::runtime_error(ToString(call.m_response->getError()));
            }
            return call.m_response->getResult();
        });
    }
    void appShutdown() override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.appShutdownRequest();
            return request;
        });
        return call.send();
    }
    void startShutdown() override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.startShutdownRequest();
            return request;
        });
        return call.send();
    }
    bool shutdownRequested() override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.shutdownRequestedRequest();
            return request;
        });
        return call.send([&]() { return call.m_response->getResult(); });
    }
    bool interruptInit() override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.interruptInitRequest();
            return request;
        });
        return call.send([&]() { return call.m_response->getResult(); });
    }
    std::string helpMessage(HelpMessageMode mode) override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.helpMessageRequest();
            request.setMode(mode);
            return request;
        });
        return call.send([&]() { return std::string(call.m_response->getResult()); });
    }
    void mapPort(bool use_upnp) override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.mapPortRequest();
            request.setUseUPnP(use_upnp);
            return request;
        });
        return call.send();
    }
    bool getProxy(Network net, proxyType& proxy_info) override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.getProxyRequest();
            request.setNet(net);
            return request;
        });
        return call.send([&]() {
            ReadProxy(proxy_info, call.m_response->getProxyInfo());
            return call.m_response->getResult();
        });
    }
    size_t getNodeCount(CConnman::NumConnections flags) override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.getNodeCountRequest();
            request.setFlags(flags);
            return request;
        });
        return call.send([&]() { return call.m_response->getResult(); });
    }
    bool getNodesStats(NodesStats& stats) override
    {
        auto call = MakeCall(m_loop, [&]() { return m_client.getNodesStatsRequest(); });
        return call.send([&]() {
            stats.clear();
            auto resultStats = call.m_response->getStats();
            stats.reserve(resultStats.size());
            for (const auto& resultNodeStats : resultStats) {
                stats.emplace_back(CNodeStats(), false, CNodeStateStats());
                ReadNodeStats(std::get<0>(stats.back()), resultNodeStats);
                if (resultNodeStats.hasStateStats()) {
                    std::get<1>(stats.back()) = true;
                    ReadNodeStateStats(std::get<2>(stats.back()), resultNodeStats.getStateStats());
                }
            }
            return call.m_response->getResult();
        });
    }
    bool getBanned(banmap_t& banmap) override
    {
        auto call = MakeCall(m_loop, [&]() { return m_client.getBannedRequest(); });
        return call.send([&]() {
            ReadBanmap(banmap, call.m_response->getBanmap());
            return call.m_response->getResult();
        });
    }
    bool ban(const CNetAddr& net_addr, BanReason reason, int64_t ban_time_offset) override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.banRequest();
            request.setNetAddr(ToArray(Serialize(net_addr)));
            request.setReason(reason);
            request.setBanTimeOffset(ban_time_offset);
            return request;
        });
        return call.send([&]() { return call.m_response->getResult(); });
    }
    bool unban(const CSubNet& ip) override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.unbanRequest();
            request.setIp(ToArray(Serialize((ip))));
            return request;
        });
        return call.send([&]() { return call.m_response->getResult(); });
    }
    bool disconnect(NodeId id) override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.disconnectRequest();
            request.setId(id);
            return request;
        });
        return call.send([&]() { return call.m_response->getResult(); });
    }
    int64_t getTotalBytesRecv() override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.getTotalBytesRecvRequest();
            return request;
        });
        return call.send([&]() { return call.m_response->getResult(); });
    }
    int64_t getTotalBytesSent() override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.getTotalBytesSentRequest();
            return request;
        });
        return call.send([&]() { return call.m_response->getResult(); });
    }
    size_t getMempoolSize() override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.getMempoolSizeRequest();
            return request;
        });
        return call.send([&]() { return call.m_response->getResult(); });
    }
    size_t getMempoolDynamicUsage() override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.getMempoolDynamicUsageRequest();
            return request;
        });
        return call.send([&]() { return call.m_response->getResult(); });
    }
    bool getHeaderTip(int& height, int64_t& block_time) override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.getHeaderTipRequest();
            return request;
        });
        return call.send([&]() {
            height = call.m_response->getHeight();
            block_time = call.m_response->getBlockTime();
            return call.m_response->getResult();
        });
    }
    int getNumBlocks() override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.getNumBlocksRequest();
            return request;
        });
        return call.send([&]() { return call.m_response->getResult(); });
    }
    int64_t getLastBlockTime() override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.getLastBlockTimeRequest();
            return request;
        });
        return call.send([&]() { return call.m_response->getResult(); });
    }
    double getVerificationProgress() override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.getVerificationProgressRequest();
            return request;
        });
        return call.send([&]() { return call.m_response->getResult(); });
    }
    bool isInitialBlockDownload() override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.isInitialBlockDownloadRequest();
            return request;
        });
        return call.send([&]() { return call.m_response->getResult(); });
    }
    bool getReindex() override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.getReindexRequest();
            return request;
        });
        return call.send([&]() { return call.m_response->getResult(); });
    }
    bool getImporting() override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.getImportingRequest();
            return request;
        });
        return call.send([&]() { return call.m_response->getResult(); });
    }
    void setNetworkActive(bool active) override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.setNetworkActiveRequest();
            request.setActive(active);
            return request;
        });
        return call.send();
    }
    bool getNetworkActive() override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.getNetworkActiveRequest();
            return request;
        });
        return call.send([&]() { return call.m_response->getResult(); });
    }
    unsigned int getTxConfirmTarget() override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.getTxConfirmTargetRequest();
            return request;
        });
        return call.send([&]() { return call.m_response->getResult(); });
    }
    bool getWalletRbf() override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.getWalletRbfRequest();
            return request;
        });
        return call.send([&]() { return call.m_response->getResult(); });
    }
    CAmount getRequiredFee(unsigned int tx_bytes) override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.getRequiredFeeRequest();
            request.setTxBytes(tx_bytes);
            return request;
        });
        return call.send([&]() { return call.m_response->getResult(); });
    }
    CAmount getMinimumFee(unsigned int tx_bytes,
        const CCoinControl& coin_control,
        int* returned_target,
        FeeReason* reason) override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.getMinimumFeeRequest();
            request.setTxBytes(tx_bytes);
            BuildCoinControl(request.initCoinControl(), coin_control);
            return request;
        });
        return call.send([&]() {
            if (returned_target) {
                *returned_target = call.m_response->getReturnedTarget();
            }
            if (reason) {
                *reason = FeeReason(call.m_response->getReason());
            }
            return call.m_response->getResult();
        });
    }
    CAmount getMaxTxFee() override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.getMaxTxFeeRequest();
            return request;
        });
        return call.send([&]() { return call.m_response->getResult(); });
    }
    CFeeRate estimateSmartFee(int num_blocks, bool conservative, int* returned_target) override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.estimateSmartFeeRequest();
            request.setNumBlocks(num_blocks);
            request.setConservative(conservative);
            return request;
        });
        return call.send([&]() {
            if (returned_target) {
                *returned_target = call.m_response->getReturnedTarget();
            }
            return Unserialize<CFeeRate>(call.m_response->getResult());
        });
    }
    CFeeRate getDustRelayFee() override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.getDustRelayFeeRequest();
            return request;
        });
        return call.send([&]() { return Unserialize<CFeeRate>(call.m_response->getResult()); });
    }
    CFeeRate getFallbackFee() override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.getFallbackFeeRequest();
            return request;
        });
        return call.send([&]() { return Unserialize<CFeeRate>(call.m_response->getResult()); });
    }
    CFeeRate getPayTxFee() override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.getPayTxFeeRequest();
            return request;
        });
        return call.send([&]() { return Unserialize<CFeeRate>(call.m_response->getResult()); });
    }
    void setPayTxFee(CFeeRate rate) override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.setPayTxFeeRequest();
            request.setRate(ToArray(Serialize(rate)));
            return request;
        });
        return call.send();
    }
    UniValue executeRpc(const std::string& command, const UniValue& params, const std::string& uri) override
    {
        UniValue result;
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.executeRpcRequest();
            request.setCommand(command);
            request.setUri(uri);
            BuildUniValue(request.initParams(), params);
            return request;
        });
        call.send([&]() {
            if (call.m_response->hasRpcError()) {
                ReadUniValue(result, call.m_response->getRpcError());
                throw result;
            }
            if (call.m_response->hasError()) {
                throw std::runtime_error(ToString(call.m_response->getError()));
            }
            ReadUniValue(result, call.m_response->getResult());
        });
        return result;
    }
    std::vector<std::string> listRpcCommands() override
    {
        std::vector<std::string> result;
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.listRpcCommandsRequest();
            return request;
        });
        call.send([&]() {
            result.clear();
            result.reserve(call.m_response->getResult().size());
            for (const auto& command : call.m_response->getResult()) {
                result.emplace_back(ToString(command));
            }
        });
        return result;
    }
    void rpcSetTimerInterfaceIfUnset(RPCTimerInterface* iface) override
    {
        // Ignore interface argument when using Cap'n Proto. It's simpler and
        // more efficient to just implemented the interface on the server side.
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.rpcSetTimerInterfaceIfUnsetRequest();
            return request;
        });
        return call.send();
    }
    void rpcUnsetTimerInterface(RPCTimerInterface* iface) override
    {
        // Ignore interface argument when using Cap'n Proto. It's simpler and
        // more efficient to just implemented the interface on the server side.
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.rpcUnsetTimerInterfaceRequest();
            return request;
        });
        return call.send();
    }
    bool getUnspentOutput(const COutPoint& output, Coin& coin) override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.getUnspentOutputRequest();
            request.setOutput(ToArray(Serialize(output)));
            return request;
        });
        return call.send([&]() {
            coin = Unserialize<Coin>(call.m_response->getCoin());
            return call.m_response->getResult();
        });
    }
    std::unique_ptr<Wallet> getWallet(size_t index) override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.getWalletRequest();
            request.setIndex(index);
            return request;
        });
        return call.send([&]() { return MakeUnique<WalletImpl>(m_loop, call.m_response->getResult()); });
    }
    std::unique_ptr<Handler> handleInitMessage(InitMessageFn fn) override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.handleInitMessageRequest();
            request.setCallback(kj::heap<InitMessageCallbackServer>(std::move(fn)));
            return request;
        });
        return call.send([&]() { return MakeUnique<HandlerImpl>(m_loop, call.m_response->getHandler()); });
    }
    std::unique_ptr<Handler> handleMessageBox(MessageBoxFn fn) override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.handleMessageBoxRequest();
            request.setCallback(kj::heap<MessageBoxCallbackServer>(m_loop, std::move(fn)));
            return request;
        });
        return call.send([&]() { return MakeUnique<HandlerImpl>(m_loop, call.m_response->getHandler()); });
    }
    std::unique_ptr<Handler> handleQuestion(QuestionFn fn) override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.handleQuestionRequest();
            request.setCallback(kj::heap<QuestionCallbackServer>(std::move(fn)));
            return request;
        });
        return call.send([&]() { return MakeUnique<HandlerImpl>(m_loop, call.m_response->getHandler()); });
    }
    std::unique_ptr<Handler> handleShowProgress(ShowProgressFn fn) override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.handleShowProgressRequest();
            request.setCallback(kj::heap<ShowProgressCallbackServer>(std::move(fn)));
            return request;
        });
        return call.send([&]() { return MakeUnique<HandlerImpl>(m_loop, call.m_response->getHandler()); });
    }
    std::unique_ptr<Handler> handleLoadWallet(LoadWalletFn fn) override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.handleLoadWalletRequest();
            request.setCallback(kj::heap<LoadWalletCallbackServer>(m_loop, std::move(fn)));
            return request;
        });
        return call.send([&]() { return MakeUnique<HandlerImpl>(m_loop, call.m_response->getHandler()); });
    }
    std::unique_ptr<Handler> handleNotifyNumConnectionsChanged(NotifyNumConnectionsChangedFn fn) override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.handleNotifyNumConnectionsChangedRequest();
            request.setCallback(kj::heap<NotifyNumConnectionsChangedCallbackServer>(std::move(fn)));
            return request;
        });
        return call.send([&]() { return MakeUnique<HandlerImpl>(m_loop, call.m_response->getHandler()); });
    }
    std::unique_ptr<Handler> handleNotifyNetworkActiveChanged(NotifyNetworkActiveChangedFn fn) override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.handleNotifyNetworkActiveChangedRequest();
            request.setCallback(kj::heap<NotifyNetworkActiveChangedCallbackServer>(std::move(fn)));
            return request;
        });
        return call.send([&]() { return MakeUnique<HandlerImpl>(m_loop, call.m_response->getHandler()); });
    }
    std::unique_ptr<Handler> handleNotifyAlertChanged(NotifyAlertChangedFn fn) override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.handleNotifyAlertChangedRequest();
            request.setCallback(kj::heap<NotifyAlertChangedCallbackServer>(std::move(fn)));
            return request;
        });
        return call.send([&]() { return MakeUnique<HandlerImpl>(m_loop, call.m_response->getHandler()); });
    }
    std::unique_ptr<Handler> handleBannedListChanged(BannedListChangedFn fn) override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.handleBannedListChangedRequest();
            request.setCallback(kj::heap<BannedListChangedCallbackServer>(std::move(fn)));
            return request;
        });
        return call.send([&]() { return MakeUnique<HandlerImpl>(m_loop, call.m_response->getHandler()); });
    }
    std::unique_ptr<Handler> handleNotifyBlockTip(NotifyBlockTipFn fn) override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.handleNotifyBlockTipRequest();
            request.setCallback(kj::heap<NotifyBlockTipCallbackServer>(std::move(fn)));
            return request;
        });
        return call.send([&]() { return MakeUnique<HandlerImpl>(m_loop, call.m_response->getHandler()); });
    }
    std::unique_ptr<Handler> handleNotifyHeaderTip(NotifyHeaderTipFn fn) override
    {
        auto call = MakeCall(m_loop, [&]() {
            auto request = m_client.handleNotifyHeaderTipRequest();
            request.setCallback(kj::heap<NotifyHeaderTipCallbackServer>(std::move(fn)));
            return request;
        });
        return call.send([&]() { return MakeUnique<HandlerImpl>(m_loop, call.m_response->getHandler()); });
    }

    EventLoop& m_loop;
    kj::Own<Connection> m_connection;
    messages::Node::Client m_client;
};

//! VatId for server side of IPC connection.
struct ServerVatId
{
    ::capnp::word scratch[4]{};
    ::capnp::MallocMessageBuilder message{scratch};
    ::capnp::rpc::twoparty::VatId::Builder vat_id{message.getRoot<::capnp::rpc::twoparty::VatId>()};
    ServerVatId() { vat_id.setSide(::capnp::rpc::twoparty::Side::SERVER); }
};

} // namespace

std::unique_ptr<Node> MakeNode(int fd)
{
    std::promise<std::unique_ptr<Node>> node_promise;
    std::thread thread([&]() {
        EventLoop loop(std::move(thread));
        auto connection = kj::heap<Connection>(loop, fd);
        node_promise.set_value(MakeUnique<NodeImpl>(loop, kj::mv(connection),
            connection->m_rpc_client.bootstrap(ServerVatId().vat_id).castAs<messages::Node>()));
        loop.loop();
    });
    return node_promise.get_future().get();
}

} // namespace capnp
} // namespace ipc
