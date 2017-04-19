#include "ipc/capnp/server.h"

#include "coins.h"
#include "ipc/capnp/serialize.h"
#include "ipc/capnp/util.h"
#include "ipc/interfaces.h"
#include "ipc/util.h"
#include "key.h"
#include "net_processing.h"
#include "netbase.h"
#include "pubkey.h"
#include "rpc/server.h"

#if defined(HAVE_CONFIG_H)
#include "config/bitcoin-config.h"
#endif
#ifdef ENABLE_WALLET
#include "wallet/coincontrol.h"
#endif

#include <capnp/rpc-twoparty.h>

namespace ipc {
namespace capnp {
namespace {

#ifdef ENABLE_WALLET
#define CHECK_WALLET(x) x
#else
#define CHECK_WALLET(x) throw std::logic_error("Wallet function called in non-wallet build.")
#endif

template <typename Client>
class HandlerServer final : public messages::Handler::Server
{
public:
    template <typename MakeImpl>
    HandlerServer(Client&& client, MakeImpl&& make_impl) : m_client(std::move(client)), m_impl(make_impl(m_client))
    {
    }

    kj::Promise<void> disconnect(DisconnectContext context) override
    {
        m_impl->disconnect();
        return kj::READY_NOW;
    }

    Client m_client;
    std::unique_ptr<Handler> m_impl;
};

template <typename Context, typename MakeConnection>
void SetHandler(Context& context, MakeConnection&& make_connection)
{
    auto client = context.getParams().getCallback();
    using Server = HandlerServer<decltype(client)>;
    context.getResults().setHandler(
        messages::Handler::Client(kj::heap<Server>(std::move(client), std::forward<MakeConnection>(make_connection))));
}

#ifdef ENABLE_WALLET
class PendingWalletTxServer final : public messages::PendingWalletTx::Server
{
public:
    PendingWalletTxServer(EventLoop& loop, std::unique_ptr<PendingWalletTx> impl)
        : m_loop(loop), m_impl(std::move(impl))
    {
    }

    kj::Promise<void> get(GetContext context) override
    {
        context.getResults().setResult(ToArray(Serialize(m_impl->get())));
        return kj::READY_NOW;
    }
    kj::Promise<void> getVirtualSize(GetVirtualSizeContext context) override
    {
        context.getResults().setResult(m_impl->getVirtualSize());
        return kj::READY_NOW;
    }
    kj::Promise<void> commit(CommitContext context) override
    {
        return m_loop.async(kj::mvCapture(context, [this](CommitContext context) {
            WalletValueMap value_map;
            ReadWalletValueMap(value_map, context.getParams().getValueMap());
            WalletOrderForm order_form;
            ReadWalletOrderForm(order_form, context.getParams().getOrderForm());
            std::string from_account = context.getParams().getFromAccount();
            std::string reject_reason;
            context.releaseParams();
            context.getResults().setResult(
                m_impl->commit(std::move(value_map), std::move(order_form), std::move(from_account), reject_reason));
            context.getResults().setRejectReason(reject_reason);
        }));
    }
    EventLoop& m_loop;
    std::unique_ptr<PendingWalletTx> m_impl;
};
class WalletServer final : public messages::Wallet::Server
{
public:
    WalletServer(EventLoop& loop, std::unique_ptr<Wallet> impl) : m_loop(loop), m_impl(std::move(impl)) {}
    kj::Promise<void> encryptWallet(EncryptWalletContext context) override
    {
        context.getResults().setResult(
            m_impl->encryptWallet(ToSecureString(context.getParams().getWalletPassphrase())));
        return kj::READY_NOW;
    }
    kj::Promise<void> isCrypted(IsCryptedContext context) override
    {
        context.getResults().setResult(m_impl->isCrypted());
        return kj::READY_NOW;
    }
    kj::Promise<void> lock(LockContext context) override
    {
        context.getResults().setResult(m_impl->lock());
        return kj::READY_NOW;
    }
    kj::Promise<void> unlock(UnlockContext context) override
    {
        context.getResults().setResult(m_impl->unlock(ToSecureString(context.getParams().getWalletPassphrase())));
        return kj::READY_NOW;
    }
    kj::Promise<void> isLocked(IsLockedContext context) override
    {
        context.getResults().setResult(m_impl->isLocked());
        return kj::READY_NOW;
    }
    kj::Promise<void> changeWalletPassphrase(ChangeWalletPassphraseContext context) override
    {
        context.getResults().setResult(
            m_impl->changeWalletPassphrase(ToSecureString(context.getParams().getOldWalletPassphrase()),
                ToSecureString(context.getParams().getNewWalletPassphrase())));
        return kj::READY_NOW;
    }
    kj::Promise<void> backupWallet(BackupWalletContext context) override
    {
        context.getResults().setResult(m_impl->backupWallet(context.getParams().getFilename()));
        return kj::READY_NOW;
    }
    kj::Promise<void> getKeyFromPool(GetKeyFromPoolContext context) override
    {
        CPubKey pub_key;
        context.getResults().setResult(m_impl->getKeyFromPool(pub_key, context.getParams().getInternal()));
        context.getResults().setPubKey(ToArray(Serialize(pub_key)));
        return kj::READY_NOW;
    }
    kj::Promise<void> getPubKey(GetPubKeyContext context) override
    {
        CPubKey pub_key;
        context.getResults().setResult(
            m_impl->getPubKey(Unserialize<CKeyID>(context.getParams().getAddress()), pub_key));
        context.getResults().setPubKey(ToArray(Serialize(pub_key)));
        return kj::READY_NOW;
    }
    kj::Promise<void> getPrivKey(GetPrivKeyContext context) override
    {
        CKey key;
        context.getResults().setResult(m_impl->getPrivKey(Unserialize<CKeyID>(context.getParams().getAddress()), key));
        BuildKey(context.getResults().initKey(), key);
        return kj::READY_NOW;
    }
    kj::Promise<void> havePrivKey(HavePrivKeyContext context) override
    {
        context.getResults().setResult(m_impl->havePrivKey(Unserialize<CKeyID>(context.getParams().getAddress())));
        return kj::READY_NOW;
    }
    kj::Promise<void> haveWatchOnly(HaveWatchOnlyContext context) override
    {
        context.getResults().setResult(m_impl->haveWatchOnly());
        return kj::READY_NOW;
    }
    kj::Promise<void> setAddressBook(SetAddressBookContext context) override
    {
        CTxDestination dest;
        ReadTxDestination(dest, context.getParams().getDest());
        context.getResults().setResult(
            m_impl->setAddressBook(dest, context.getParams().getName(), context.getParams().getPurpose()));
        return kj::READY_NOW;
    }
    kj::Promise<void> delAddressBook(DelAddressBookContext context) override
    {
        CTxDestination dest;
        ReadTxDestination(dest, context.getParams().getDest());
        context.getResults().setResult(m_impl->delAddressBook(dest));
        return kj::READY_NOW;
    }
    kj::Promise<void> getAddress(GetAddressContext context) override
    {
        CTxDestination dest;
        ReadTxDestination(dest, context.getParams().getDest());
        std::string name;
        isminetype ismine;
        context.getResults().setResult(m_impl->getAddress(dest, &name, &ismine));
        context.getResults().setName(name);
        context.getResults().setIsMine(ismine);
        return kj::READY_NOW;
    }
    kj::Promise<void> getAddresses(GetAddressesContext context) override
    {
        auto addresses = m_impl->getAddresses();
        auto result_addresses = context.getResults().initResult(addresses.size());
        size_t i = 0;
        for (const auto& address : addresses) {
            BuildWalletAddress(result_addresses[i], address);
            ++i;
        }
        return kj::READY_NOW;
    }
    kj::Promise<void> getAccountAddresses(GetAccountAddressesContext context) override
    {
        auto addresses = m_impl->getAccountAddresses(context.getParams().getAccount());
        auto result_addresses = context.getResults().initResult(addresses.size());
        size_t i = 0;
        for (const auto& address : addresses) {
            BuildTxDestination(result_addresses[i], address);
            ++i;
        }
        return kj::READY_NOW;
    }
    kj::Promise<void> addDestData(AddDestDataContext context) override
    {
        CTxDestination dest;
        ReadTxDestination(dest, context.getParams().getDest());
        context.getResults().setResult(
            m_impl->addDestData(dest, context.getParams().getKey(), context.getParams().getValue()));
        return kj::READY_NOW;
    }
    kj::Promise<void> eraseDestData(EraseDestDataContext context) override
    {
        CTxDestination dest;
        ReadTxDestination(dest, context.getParams().getDest());
        context.getResults().setResult(m_impl->eraseDestData(dest, context.getParams().getKey()));
        return kj::READY_NOW;
    }
    kj::Promise<void> getDestValues(GetDestValuesContext context) override
    {
        auto values = m_impl->getDestValues(context.getParams().getPrefix());
        auto result_values = context.getResults().initResult(values.size());
        size_t i = 0;
        for (const auto& value : values) {
            result_values.set(i, ToArray(value));
            ++i;
        }
        return kj::READY_NOW;
    }
    kj::Promise<void> lockCoin(LockCoinContext context) override
    {
        m_impl->lockCoin(Unserialize<COutPoint>(context.getParams().getOutput()));
        return kj::READY_NOW;
    }
    kj::Promise<void> unlockCoin(UnlockCoinContext context) override
    {
        m_impl->unlockCoin(Unserialize<COutPoint>(context.getParams().getOutput()));
        return kj::READY_NOW;
    }
    kj::Promise<void> isLockedCoin(IsLockedCoinContext context) override
    {
        context.getResults().setResult(m_impl->isLockedCoin(Unserialize<COutPoint>(context.getParams().getOutput())));
        return kj::READY_NOW;
    }
    kj::Promise<void> listLockedCoins(ListLockedCoinsContext context) override
    {
        std::vector<COutPoint> outputs;
        m_impl->listLockedCoins(outputs);
        auto result_outputs = context.getResults().initOutputs(outputs.size());
        size_t i = 0;
        for (const auto& output : outputs) {
            result_outputs.set(i, ToArray(Serialize(output)));
            ++i;
        }
        return kj::READY_NOW;
    }
    kj::Promise<void> createTransaction(CreateTransactionContext context) override
    {
        std::vector<CRecipient> recipients;
        recipients.reserve(context.getParams().getRecipients().size());
        for (const auto& recipient : context.getParams().getRecipients()) {
            recipients.emplace_back();
            ReadRecipient(recipients.back(), recipient);
        }
        CCoinControl coin_control;
        const CCoinControl* coinControlPtr = nullptr;
        if (context.getParams().hasCoinControl()) {
            ReadCoinControl(coin_control, context.getParams().getCoinControl());
            coinControlPtr = &coin_control;
        }
        int change_pos = context.getParams().getChangePos();
        CAmount fee;
        std::string fail_reason;
        if (auto tx = m_impl->createTransaction(
                recipients, coinControlPtr, context.getParams().getSign(), change_pos, fee, fail_reason)) {
            context.getResults().setResult(
                messages::PendingWalletTx::Client(kj::heap<PendingWalletTxServer>(m_loop, std::move(tx))));
        }
        context.getResults().setChangePos(change_pos);
        context.getResults().setFee(fee);
        context.getResults().setFailReason(fail_reason);
        return kj::READY_NOW;
    }
    kj::Promise<void> transactionCanBeAbandoned(TransactionCanBeAbandonedContext context) override
    {
        context.getResults().setResult(
            m_impl->transactionCanBeAbandoned(ToBlob<uint256>(context.getParams().getTxid())));
        return kj::READY_NOW;
    }
    kj::Promise<void> abandonTransaction(AbandonTransactionContext context) override
    {
        context.getResults().setResult(m_impl->abandonTransaction(ToBlob<uint256>(context.getParams().getTxid())));
        return kj::READY_NOW;
    }
    kj::Promise<void> transactionCanBeBumped(TransactionCanBeBumpedContext context) override
    {
        context.getResults().setValue(m_impl->transactionCanBeBumped(ToBlob<uint256>(context.getParams().getTxid())));
        return kj::READY_NOW;
    }
    kj::Promise<void> createBumpTransaction(CreateBumpTransactionContext context) override
    {
        std::vector<std::string> errors;
        CAmount old_fee;
        CAmount new_fee;
        CMutableTransaction mtx;
        context.getResults().setValue(m_impl->createBumpTransaction(ToBlob<uint256>(context.getParams().getTxid()),
            context.getParams().getConfirmTarget(), context.getParams().getIgnoreGlobalPayTxFee(),
            context.getParams().getTotalFee(), context.getParams().getReplaceable(), errors, old_fee, new_fee, mtx));
        auto result_errors = context.getResults().initErrors(errors.size());
        size_t i = 0;
        for (const auto& error : errors) {
            result_errors.set(i, error);
            ++i;
        }
        context.getResults().setOldFee(old_fee);
        context.getResults().setNewFee(new_fee);
        context.getResults().setMtx(ToArray(Serialize(mtx)));
        return kj::READY_NOW;
    }
    kj::Promise<void> signBumpTransaction(SignBumpTransactionContext context) override
    {
        CMutableTransaction mtx;
        Unserialize(mtx, context.getParams().getMtx());
        context.getResults().setValue(m_impl->signBumpTransaction(mtx));
        context.getResults().setMtx(ToArray(Serialize(mtx)));
        return kj::READY_NOW;
    }
    kj::Promise<void> commitBumpTransaction(CommitBumpTransactionContext context) override
    {
        std::vector<std::string> errors;
        uint256 bumped_txid;
        context.getResults().setValue(m_impl->commitBumpTransaction(ToBlob<uint256>(context.getParams().getTxid()),
            Unserialize<CMutableTransaction>(context.getParams().getMtx()), errors, bumped_txid));
        auto result_errors = context.getResults().initErrors(errors.size());
        size_t i = 0;
        for (const auto& error : errors) {
            result_errors.set(i, error);
            ++i;
        }
        context.getResults().setBumpedTxid(FromBlob(bumped_txid));
        return kj::READY_NOW;
    }
    kj::Promise<void> getTx(GetTxContext context) override
    {
        auto tx = m_impl->getTx(ToBlob<uint256>(context.getParams().getTxid()));
        if (tx) {
            context.getResults().setResult(ToArray(Serialize(*tx)));
        }
        return kj::READY_NOW;
    }
    kj::Promise<void> getWalletTx(GetWalletTxContext context) override
    {
        BuildWalletTx(
            context.getResults().initResult(), m_impl->getWalletTx(ToBlob<uint256>(context.getParams().getTxid())));
        return kj::READY_NOW;
    }
    kj::Promise<void> getWalletTxs(GetWalletTxsContext context) override
    {
        auto wallet_txs = m_impl->getWalletTxs();
        auto result_wallet_txs = context.getResults().initResult(wallet_txs.size());
        size_t i = 0;
        for (const auto& wallet_tx : wallet_txs) {
            BuildWalletTx(result_wallet_txs[i], wallet_tx);
            ++i;
        }
        return kj::READY_NOW;
    }
    kj::Promise<void> tryGetTxStatus(TryGetTxStatusContext context) override
    {
        WalletTxStatus tx_status;
        int num_blocks;
        int64_t adjusted_time;
        context.getResults().setResult(m_impl->tryGetTxStatus(
            ToBlob<uint256>(context.getParams().getTxid()), tx_status, num_blocks, adjusted_time));
        BuildWalletTxStatus(context.getResults().initTxStatus(), tx_status);
        context.getResults().setNumBlocks(num_blocks);
        context.getResults().setAdjustedTime(adjusted_time);
        return kj::READY_NOW;
    }
    kj::Promise<void> getWalletTxDetails(GetWalletTxDetailsContext context) override
    {
        WalletTxStatus tx_status;
        WalletOrderForm order_form;
        bool in_mempool;
        int num_blocks;
        int64_t adjusted_time;
        auto wallet_tx = m_impl->getWalletTxDetails(ToBlob<uint256>(context.getParams().getTxid()), tx_status,
            order_form, in_mempool, num_blocks, adjusted_time);
        BuildWalletTx(context.getResults().initResult(), wallet_tx);
        BuildWalletTxStatus(context.getResults().initTxStatus(), tx_status);
        BuildWalletOrderForm(context.getResults().initOrderForm(), order_form);
        context.getResults().setInMempool(in_mempool);
        context.getResults().setNumBlocks(num_blocks);
        context.getResults().setAdjustedTime(adjusted_time);
        return kj::READY_NOW;
    }
    kj::Promise<void> getBalances(GetBalancesContext context) override
    {
        BuildWalletBalances(context.getResults().initResult(), m_impl->getBalances());
        return kj::READY_NOW;
    }
    kj::Promise<void> tryGetBalances(TryGetBalancesContext context) override
    {
        WalletBalances balances;
        int num_blocks;
        context.getResults().setResult(m_impl->tryGetBalances(balances, num_blocks));
        BuildWalletBalances(context.getResults().initBalances(), balances);
        context.getResults().setNumBlocks(num_blocks);
        return kj::READY_NOW;
    }
    kj::Promise<void> getBalance(GetBalanceContext context) override
    {
        context.getResults().setResult(m_impl->getBalance());
        return kj::READY_NOW;
    }
    kj::Promise<void> getAvailableBalance(GetAvailableBalanceContext context) override
    {
        CCoinControl coin_control;
        ReadCoinControl(coin_control, context.getParams().getCoinControl());
        context.getResults().setResult(m_impl->getAvailableBalance(coin_control));
        return kj::READY_NOW;
    }
    kj::Promise<void> isMine(IsMineContext context) override
    {
        if (context.getParams().hasTxin()) {
            context.getResults().setResult(m_impl->isMine(Unserialize<CTxIn>(context.getParams().getTxin())));
        }
        if (context.getParams().hasTxout()) {
            context.getResults().setResult(m_impl->isMine(Unserialize<CTxOut>(context.getParams().getTxout())));
        }
        return kj::READY_NOW;
    }
    kj::Promise<void> getDebit(GetDebitContext context) override
    {
        context.getResults().setResult(m_impl->getDebit(
            Unserialize<CTxIn>(context.getParams().getTxin()), isminefilter(context.getParams().getFilter())));
        return kj::READY_NOW;
    }
    kj::Promise<void> getCredit(GetCreditContext context) override
    {
        context.getResults().setResult(m_impl->getCredit(
            Unserialize<CTxOut>(context.getParams().getTxout()), isminefilter(context.getParams().getFilter())));
        return kj::READY_NOW;
    }
    kj::Promise<void> listCoins(ListCoinsContext context) override
    {
        BuildCoinsList(context.getResults().initResult(), m_impl->listCoins());
        return kj::READY_NOW;
    }
    kj::Promise<void> getCoins(GetCoinsContext context) override
    {
        std::vector<COutPoint> outputs;
        outputs.reserve(context.getParams().getOutputs().size());
        for (const auto& output : context.getParams().getOutputs()) {
            outputs.emplace_back(Unserialize<COutPoint>(output));
        }
        auto coins = m_impl->getCoins(outputs);
        auto result = context.getResults().initResult(coins.size());
        size_t i = 0;
        for (const auto& coin : coins) {
            BuildWalletTxOut(result[i], coin);
            ++i;
        }
        return kj::READY_NOW;
    }
    kj::Promise<void> hdEnabled(HdEnabledContext context) override
    {
        context.getResults().setResult(m_impl->hdEnabled());
        return kj::READY_NOW;
    }
    kj::Promise<void> handleShowProgress(HandleShowProgressContext context) override
    {
        SetHandler(context, [this](messages::ShowProgressCallback::Client& client) {
            return m_impl->handleShowProgress([this, &client](const std::string& title, int progress) {
                auto call = MakeCall(m_loop, [&]() {
                    auto request = client.callRequest();
                    request.setTitle(title);
                    request.setProgress(progress);
                    return request;
                });
                return call.send();
            });
        });
        return kj::READY_NOW;
    }
    kj::Promise<void> handleStatusChanged(HandleStatusChangedContext context) override
    {
        SetHandler(context, [this](messages::StatusChangedCallback::Client& client) {
            return m_impl->handleStatusChanged([this, &client]() {
                auto call = MakeCall(m_loop, [&]() {
                    auto request = client.callRequest();
                    return request;
                });
                return call.send();
            });
        });
        return kj::READY_NOW;
    }
    kj::Promise<void> handleAddressBookChanged(HandleAddressBookChangedContext context) override
    {
        SetHandler(context, [this](messages::AddressBookChangedCallback::Client& client) {
            return m_impl->handleAddressBookChanged([this, &client](const CTxDestination& address,
                const std::string& label, bool isMine, const std::string& purpose, ChangeType status) {
                auto call = MakeCall(m_loop, [&]() {
                    auto request = client.callRequest();
                    BuildTxDestination(request.initAddress(), address);
                    request.setLabel(label);
                    request.setIsMine(isMine);
                    request.setPurpose(purpose);
                    request.setStatus(status);
                    return request;
                });
                return call.send();
            });
        });
        return kj::READY_NOW;
    }
    kj::Promise<void> handleTransactionChanged(HandleTransactionChangedContext context) override
    {
        SetHandler(context, [this](messages::TransactionChangedCallback::Client& client) {
            return m_impl->handleTransactionChanged([this, &client](const uint256& txid, ChangeType status) {
                auto call = MakeCall(m_loop, [&]() {
                    auto request = client.callRequest();
                    request.setTxid(FromBlob(txid));
                    request.setStatus(status);
                    return request;
                });
                return call.send();
            });
        });
        return kj::READY_NOW;
    }
    kj::Promise<void> handleWatchOnlyChanged(HandleWatchOnlyChangedContext context) override
    {
        SetHandler(context, [this](messages::WatchonlyChangedCallback::Client& client) {
            return m_impl->handleWatchOnlyChanged([this, &client](bool haveWatchOnly) {
                auto call = MakeCall(m_loop, [&]() {
                    auto request = client.callRequest();
                    request.setHaveWatchOnly(haveWatchOnly);
                    return request;
                });
                return call.send();
            });
        });
        return kj::READY_NOW;
    }

    EventLoop& m_loop;
    std::unique_ptr<Wallet> m_impl;
};
#endif // ENABLE_WALLET

class RpcTimer : public ::RPCTimerBase
{
public:
    RpcTimer(EventLoop& loop, std::function<void(void)>& fn, int64_t millis)
        : m_fn(fn), m_promise(loop.m_io_context.provider->getTimer()
                                  .afterDelay(millis * kj::MILLISECONDS)
                                  .then([this]() { m_fn(); })
                                  .eagerlyEvaluate(nullptr))
    {
    }
    ~RpcTimer() noexcept override {}

    std::function<void(void)> m_fn;
    kj::Promise<void> m_promise;
};

class RpcTimerInterface : public ::RPCTimerInterface
{
public:
    RpcTimerInterface(EventLoop& loop) : m_loop(loop) {}
    const char* Name() override { return "Cap'n Proto"; }
    RPCTimerBase* NewTimer(std::function<void(void)>& fn, int64_t millis) override
    {
        return new RpcTimer(m_loop, fn, millis);
    }
    EventLoop& m_loop;
};

class NodeServer final : public messages::Node::Server
{
public:
    NodeServer(EventLoop& loop, Node* impl) : m_loop(loop), m_impl(impl) {}

    kj::Promise<void> parseParameters(ParseParametersContext context) override
    {
        std::vector<const char*> argv(context.getParams().getArgv().size());
        size_t i = 0;
        for (const std::string& arg : context.getParams().getArgv()) {
            argv[i] = arg.c_str();
            ++i;
        }
        m_impl->parseParameters(argv.size(), argv.data());
        return kj::READY_NOW;
    }
    kj::Promise<void> softSetArg(SoftSetArgContext context) override
    {
        context.getResults().setResult(
            m_impl->softSetArg(context.getParams().getArg(), context.getParams().getValue()));
        return kj::READY_NOW;
    }
    kj::Promise<void> softSetBoolArg(SoftSetBoolArgContext context) override
    {
        context.getResults().setResult(
            m_impl->softSetBoolArg(context.getParams().getArg(), context.getParams().getValue()));
        return kj::READY_NOW;
    }
    kj::Promise<void> readConfigFile(ReadConfigFileContext context) override
    {
        try {
            m_impl->readConfigFile(context.getParams().getConfPath());
        } catch (const std::exception& e) {
            context.getResults().setError(e.what());
        }
        return kj::READY_NOW;
    }
    kj::Promise<void> selectParams(SelectParamsContext context) override
    {
        try {
            m_impl->selectParams(context.getParams().getNetwork());
        } catch (const std::exception& e) {
            context.getResults().setError(e.what());
        }
        return kj::READY_NOW;
    }
    kj::Promise<void> getNetwork(GetNetworkContext context) override
    {
        context.getResults().setResult(m_impl->getNetwork());
        return kj::READY_NOW;
    }
    kj::Promise<void> initLogging(InitLoggingContext context) override
    {
        m_impl->initLogging();
        return kj::READY_NOW;
    }
    kj::Promise<void> initParameterInteraction(InitParameterInteractionContext context) override
    {
        m_impl->initParameterInteraction();
        return kj::READY_NOW;
    }
    kj::Promise<void> getWarnings(GetWarningsContext context) override
    {
        context.getResults().setResult(m_impl->getWarnings(context.getParams().getType()));
        return kj::READY_NOW;
    }
    kj::Promise<void> getLogCategories(GetLogCategoriesContext context) override
    {
        context.getResults().setResult(m_impl->getLogCategories());
        return kj::READY_NOW;
    }
    kj::Promise<void> appInit(AppInitContext context) override
    {
        context.releaseParams();
        return m_loop.async(kj::mvCapture(context, [this](AppInitContext context) {
            try {
                context.getResults().setResult(m_impl->appInit());
            } catch (const std::exception& e) {
                context.getResults().setError(e.what());
            }
        }));
    }
    kj::Promise<void> appShutdown(AppShutdownContext context) override
    {
        m_impl->appShutdown();
        return kj::READY_NOW;
    }
    kj::Promise<void> startShutdown(StartShutdownContext context) override
    {
        m_impl->startShutdown();
        return kj::READY_NOW;
    }
    kj::Promise<void> shutdownRequested(ShutdownRequestedContext context) override
    {
        context.getResults().setResult(m_impl->shutdownRequested());
        return kj::READY_NOW;
    }
    kj::Promise<void> helpMessage(HelpMessageContext context) override
    {
        context.getResults().setResult(m_impl->helpMessage(HelpMessageMode(context.getParams().getMode())));
        return kj::READY_NOW;
    }
    kj::Promise<void> mapPort(MapPortContext context) override
    {
        m_impl->mapPort(context.getParams().getUseUPnP());
        return kj::READY_NOW;
    }
    kj::Promise<void> getProxy(GetProxyContext context) override
    {
        proxyType proxy_info;
        context.getResults().setResult(m_impl->getProxy(Network(context.getParams().getNet()), proxy_info));
        BuildProxy(context.getResults().initProxyInfo(), proxy_info);
        return kj::READY_NOW;
    }
    kj::Promise<void> getNodeCount(GetNodeCountContext context) override
    {
        context.getResults().setResult(m_impl->getNodeCount(CConnman::NumConnections(context.getParams().getFlags())));
        return kj::READY_NOW;
    }
    kj::Promise<void> getNodesStats(GetNodesStatsContext context) override
    {
        Node::NodesStats stats;
        if (m_impl->getNodesStats(stats)) {
            auto result_stats = context.getResults().initStats(stats.size());
            size_t i = 0;
            for (const auto& node_stats : stats) {
                BuildNodeStats(result_stats[i], std::get<0>(node_stats));
                if (std::get<1>(node_stats)) {
                    BuildNodeStateStats(result_stats[i].initStateStats(), std::get<2>(node_stats));
                }
                ++i;
            }
        }
        return kj::READY_NOW;
    }
    kj::Promise<void> getBanned(GetBannedContext context) override
    {
        banmap_t banmap;
        if (m_impl->getBanned(banmap)) {
            BuildBanmap(context.getResults().initBanmap(), banmap);
        }
        return kj::READY_NOW;
    }
    kj::Promise<void> ban(BanContext context) override
    {
        context.getResults().setResult(m_impl->ban(Unserialize<CNetAddr>(context.getParams().getNetAddr()),
            BanReason(context.getParams().getReason()), context.getParams().getBanTimeOffset()));
        return kj::READY_NOW;
    }
    kj::Promise<void> unban(UnbanContext context) override
    {
        context.getResults().setResult(m_impl->unban(Unserialize<CSubNet>(context.getParams().getIp())));
        return kj::READY_NOW;
    }
    kj::Promise<void> disconnect(DisconnectContext context) override
    {
        context.getResults().setResult(m_impl->disconnect(context.getParams().getId()));
        return kj::READY_NOW;
    }
    kj::Promise<void> getTotalBytesRecv(GetTotalBytesRecvContext context) override
    {
        context.getResults().setResult(m_impl->getTotalBytesRecv());
        return kj::READY_NOW;
    }
    kj::Promise<void> getTotalBytesSent(GetTotalBytesSentContext context) override
    {
        context.getResults().setResult(m_impl->getTotalBytesSent());
        return kj::READY_NOW;
    }
    kj::Promise<void> getMempoolSize(GetMempoolSizeContext context) override
    {
        context.getResults().setResult(m_impl->getMempoolSize());
        return kj::READY_NOW;
    }
    kj::Promise<void> getMempoolDynamicUsage(GetMempoolDynamicUsageContext context) override
    {
        context.getResults().setResult(m_impl->getMempoolDynamicUsage());
        return kj::READY_NOW;
    }
    kj::Promise<void> getHeaderTip(GetHeaderTipContext context) override
    {
        int height;
        int64_t block_time;
        context.getResults().setResult(m_impl->getHeaderTip(height, block_time));
        context.getResults().setHeight(height);
        context.getResults().setBlockTime(block_time);
        return kj::READY_NOW;
    }
    kj::Promise<void> getNumBlocks(GetNumBlocksContext context) override
    {
        context.getResults().setResult(m_impl->getNumBlocks());
        return kj::READY_NOW;
    }
    kj::Promise<void> getLastBlockTime(GetLastBlockTimeContext context) override
    {
        context.getResults().setResult(m_impl->getLastBlockTime());
        return kj::READY_NOW;
    }
    kj::Promise<void> getVerificationProgress(GetVerificationProgressContext context) override
    {
        context.getResults().setResult(m_impl->getVerificationProgress());
        return kj::READY_NOW;
    }
    kj::Promise<void> isInitialBlockDownload(IsInitialBlockDownloadContext context) override
    {
        context.getResults().setResult(m_impl->isInitialBlockDownload());
        return kj::READY_NOW;
    }
    kj::Promise<void> getReindex(GetReindexContext context) override
    {
        context.getResults().setResult(m_impl->getReindex());
        return kj::READY_NOW;
    }
    kj::Promise<void> getImporting(GetImportingContext context) override
    {
        context.getResults().setResult(m_impl->getImporting());
        return kj::READY_NOW;
    }
    kj::Promise<void> setNetworkActive(SetNetworkActiveContext context) override
    {
        m_impl->setNetworkActive(context.getParams().getActive());
        return kj::READY_NOW;
    }
    kj::Promise<void> getNetworkActive(GetNetworkActiveContext context) override
    {
        context.getResults().setResult(m_impl->getNetworkActive());
        return kj::READY_NOW;
    }
    kj::Promise<void> getTxConfirmTarget(GetTxConfirmTargetContext context) override
    {
        context.getResults().setResult(m_impl->getTxConfirmTarget());
        return kj::READY_NOW;
    }
    kj::Promise<void> getWalletRbf(GetWalletRbfContext context) override
    {
        context.getResults().setResult(m_impl->getWalletRbf());
        return kj::READY_NOW;
    }
    kj::Promise<void> getRequiredFee(GetRequiredFeeContext context) override
    {
        context.getResults().setResult(m_impl->getRequiredFee(context.getParams().getTxBytes()));
        return kj::READY_NOW;
    }
    kj::Promise<void> getMinimumFee(GetMinimumFeeContext context) override
    {
        context.getResults().setResult(
            m_impl->getMinimumFee(context.getParams().getTxBytes(), context.getParams().getConfirmTarget()));
        return kj::READY_NOW;
    }
    kj::Promise<void> getMaxTxFee(GetMaxTxFeeContext context) override
    {
        context.getResults().setResult(m_impl->getMaxTxFee());
        return kj::READY_NOW;
    }
    kj::Promise<void> estimateSmartFee(EstimateSmartFeeContext context) override
    {
        int answer_found_at_blocks;
        context.getResults().setResult(
            ToArray(Serialize(m_impl->estimateSmartFee(context.getParams().getNumBlocks(), &answer_found_at_blocks))));
        context.getResults().setAnswerFoundAtBlocks(answer_found_at_blocks);
        return kj::READY_NOW;
    }
    kj::Promise<void> getDustRelayFee(GetDustRelayFeeContext context) override
    {
        context.getResults().setResult(ToArray(Serialize(m_impl->getDustRelayFee())));
        return kj::READY_NOW;
    }
    kj::Promise<void> getFallbackFee(GetFallbackFeeContext context) override
    {
        context.getResults().setResult(ToArray(Serialize(m_impl->getFallbackFee())));
        return kj::READY_NOW;
    }
    kj::Promise<void> getPayTxFee(GetPayTxFeeContext context) override
    {
        context.getResults().setResult(ToArray(Serialize(m_impl->getPayTxFee())));
        return kj::READY_NOW;
    }
    kj::Promise<void> setPayTxFee(SetPayTxFeeContext context) override
    {
        m_impl->setPayTxFee(Unserialize<CFeeRate>(context.getParams().getRate()));
        return kj::READY_NOW;
    }
    kj::Promise<void> executeRpc(ExecuteRpcContext context) override
    {
        UniValue params;
        ReadUniValue(params, context.getParams().getParams());
        try {
            BuildUniValue(context.getResults().initResult(),
                m_impl->executeRpc(ToString(context.getParams().getCommand()), params));
        } catch (const UniValue& e) {
            BuildUniValue(context.getResults().initRpcError(), e);
        } catch (const std::exception& e) {
            context.getResults().setError(e.what());
        }
        return kj::READY_NOW;
    }
    kj::Promise<void> listRpcCommands(ListRpcCommandsContext context) override
    {
        auto commands = m_impl->listRpcCommands();
        auto result_commands = context.getResults().initResult(commands.size());
        size_t i = 0;
        for (const auto& command : commands) {
            result_commands.set(i, command);
            ++i;
        }
        return kj::READY_NOW;
    }
    kj::Promise<void> rpcSetTimerInterfaceIfUnset(RpcSetTimerInterfaceIfUnsetContext context) override
    {
        if (!m_timer_interface) {
            m_timer_interface = MakeUnique<RpcTimerInterface>(m_loop);
        }
        m_impl->rpcSetTimerInterfaceIfUnset(m_timer_interface.get());
        return kj::READY_NOW;
    }
    kj::Promise<void> rpcUnsetTimerInterface(RpcUnsetTimerInterfaceContext context) override
    {
        m_impl->rpcUnsetTimerInterface(m_timer_interface.get());
        m_timer_interface.reset();
        return kj::READY_NOW;
    }
    kj::Promise<void> getUnspentOutput(GetUnspentOutputContext context) override
    {
        Coin coin;
        context.getResults().setResult(
            m_impl->getUnspentOutput(Unserialize<COutPoint>(context.getParams().getOutput()), coin));
        context.getResults().setCoin(ToArray(Serialize(coin)));
        return kj::READY_NOW;
    }
    kj::Promise<void> getWallet(GetWalletContext context) override
    {
        CHECK_WALLET(context.getResults().setResult(messages::Wallet::Client(
            kj::heap<WalletServer>(m_loop, m_impl->getWallet(context.getParams().getIndex())))));
        return kj::READY_NOW;
    }
    kj::Promise<void> handleInitMessage(HandleInitMessageContext context) override
    {
        SetHandler(context, [this](messages::InitMessageCallback::Client& client) {
            return m_impl->handleInitMessage([this, &client](const std::string& message) {
                auto call = MakeCall(m_loop, [&]() {
                    auto request = client.callRequest();
                    request.setMessage(message);
                    return request;
                });
                return call.send();
            });
        });
        return kj::READY_NOW;
    }
    kj::Promise<void> handleMessageBox(HandleMessageBoxContext context) override
    {
        SetHandler(context, [this](messages::MessageBoxCallback::Client& client) {
            return m_impl->handleMessageBox(
                [this, &client](const std::string& message, const std::string& caption, unsigned int style) {
                    auto call = MakeCall(m_loop, [&]() {
                        auto request = client.callRequest();
                        request.setMessage(message);
                        request.setCaption(caption);
                        request.setStyle(style);
                        return request;
                    });
                    return call.send([&]() { return call.m_response->getResult(); });
                });
        });
        return kj::READY_NOW;
    }
    kj::Promise<void> handleQuestion(HandleQuestionContext context) override
    {
        SetHandler(context, [this](messages::QuestionCallback::Client& client) {
            return m_impl->handleQuestion([this, &client](const std::string& message,
                const std::string& non_interactive_message, const std::string& caption, unsigned int style) {
                auto call = MakeCall(m_loop, [&]() {
                    auto request = client.callRequest();
                    request.setMessage(message);
                    request.setNonInteractiveMessage(non_interactive_message);
                    request.setCaption(caption);
                    request.setStyle(style);
                    return request;
                });
                return call.send([&]() { return call.m_response->getResult(); });
            });
        });
        return kj::READY_NOW;
    }
    kj::Promise<void> handleShowProgress(HandleShowProgressContext context) override
    {
        SetHandler(context, [this](messages::ShowProgressCallback::Client& client) {
            return m_impl->handleShowProgress([this, &client](const std::string& title, int progress) {
                auto call = MakeCall(m_loop, [&]() {
                    auto request = client.callRequest();
                    request.setTitle(title);
                    request.setProgress(progress);
                    return request;
                });
                return call.send();
            });
        });
        return kj::READY_NOW;
    }
    kj::Promise<void> handleLoadWallet(HandleLoadWalletContext context) override
    {
        CHECK_WALLET(SetHandler(context, [this](messages::LoadWalletCallback::Client& client) {
            return m_impl->handleLoadWallet([this, &client](std::unique_ptr<Wallet> wallet) {
                auto call = MakeCall(m_loop, [&]() {
                    auto request = client.callRequest();
                    request.setWallet(messages::Wallet::Client(kj::heap<WalletServer>(m_loop, std::move(wallet))));
                    return request;
                });
                return call.send();
            });
        }));
        return kj::READY_NOW;
    }
    kj::Promise<void> handleNotifyNumConnectionsChanged(HandleNotifyNumConnectionsChangedContext context) override
    {
        SetHandler(context, [this](messages::NotifyNumConnectionsChangedCallback::Client& client) {
            return m_impl->handleNotifyNumConnectionsChanged([this, &client](int new_num_connections) {
                auto call = MakeCall(m_loop, [&]() {
                    auto request = client.callRequest();
                    request.setNewNumConnections(new_num_connections);
                    return request;
                });
                return call.send();
            });
        });
        return kj::READY_NOW;
    }
    kj::Promise<void> handleNotifyNetworkActiveChanged(HandleNotifyNetworkActiveChangedContext context) override
    {
        SetHandler(context, [this](messages::NotifyNetworkActiveChangedCallback::Client& client) {
            return m_impl->handleNotifyNetworkActiveChanged([this, &client](bool network_active) {
                auto call = MakeCall(m_loop, [&]() {
                    auto request = client.callRequest();
                    request.setNetworkActive(network_active);
                    return request;
                });
                return call.send();
            });
        });
        return kj::READY_NOW;
    }
    kj::Promise<void> handleNotifyAlertChanged(HandleNotifyAlertChangedContext context) override
    {
        SetHandler(context, [this](messages::NotifyAlertChangedCallback::Client& client) {
            return m_impl->handleNotifyAlertChanged([this, &client]() {
                auto call = MakeCall(m_loop, [&]() {
                    auto request = client.callRequest();
                    return request;
                });
                return call.send();
            });
        });
        return kj::READY_NOW;
    }
    kj::Promise<void> handleBannedListChanged(HandleBannedListChangedContext context) override
    {
        SetHandler(context, [this](messages::BannedListChangedCallback::Client& client) {
            return m_impl->handleBannedListChanged([this, &client]() {
                auto call = MakeCall(m_loop, [&]() {
                    auto request = client.callRequest();
                    return request;
                });
                return call.send();
            });
        });
        return kj::READY_NOW;
    }
    kj::Promise<void> handleNotifyBlockTip(HandleNotifyBlockTipContext context) override
    {
        SetHandler(context, [this](messages::NotifyBlockTipCallback::Client& client) {
            return m_impl->handleNotifyBlockTip(
                [this, &client](bool initial_download, int height, int64_t block_time, double verification_progress) {
                    auto call = MakeCall(m_loop, [&]() {
                        auto request = client.callRequest();
                        request.setInitialDownload(initial_download);
                        request.setHeight(height);
                        request.setBlockTime(block_time);
                        request.setVerificationProgress(verification_progress);
                        return request;
                    });
                    return call.send();
                });
        });
        return kj::READY_NOW;
    }
    kj::Promise<void> handleNotifyHeaderTip(HandleNotifyHeaderTipContext context) override
    {
        SetHandler(context, [this](messages::NotifyHeaderTipCallback::Client& client) {
            return m_impl->handleNotifyHeaderTip(
                [this, &client](bool initial_download, int height, int64_t block_time, double verification_progress) {
                    auto call = MakeCall(m_loop, [&]() {
                        auto request = client.callRequest();
                        request.setInitialDownload(initial_download);
                        request.setHeight(height);
                        request.setBlockTime(block_time);
                        request.setVerificationProgress(verification_progress);
                        return request;
                    });
                    return call.send();
                });
        });
        return kj::READY_NOW;
    }

    EventLoop& m_loop;
    Node* m_impl;
    std::unique_ptr<RpcTimerInterface> m_timer_interface;
};

} // namespace

void StartNodeServer(ipc::Node& node, int fd)
{
    EventLoop loop;
    auto stream = loop.m_io_context.lowLevelProvider->wrapSocketFd(fd, kj::LowLevelAsyncIoProvider::TAKE_OWNERSHIP);
    auto network =
        ::capnp::TwoPartyVatNetwork(*stream, ::capnp::rpc::twoparty::Side::SERVER, ::capnp::ReaderOptions());
    loop.m_task_set.add(network.onDisconnect().then([&loop]() { loop.shutdown(); }));
    auto rpc_server = ::capnp::makeRpcServer(network, ::capnp::Capability::Client(kj::heap<NodeServer>(loop, &node)));
    loop.loop();
}

} // namespace capnp

} // namespace ipc
