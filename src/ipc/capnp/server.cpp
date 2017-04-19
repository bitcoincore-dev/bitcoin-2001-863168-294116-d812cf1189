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
#include "wallet/coincontrol.h"

#include <capnp/rpc-twoparty.h>

namespace ipc {
namespace capnp {
namespace {

template <typename Client>
class HandlerServer final : public messages::Handler::Server
{
public:
    template <typename MakeImpl>
    HandlerServer(Client&& client, MakeImpl&& makeImpl) : client(std::move(client)), impl(makeImpl(this->client))
    {
    }

    kj::Promise<void> disconnect(DisconnectContext context) override
    {
        impl->disconnect();
        return kj::READY_NOW;
    }

    Client client;
    std::unique_ptr<Handler> impl;
};

template <typename Context, typename MakeConnection>
void SetHandler(Context& context, MakeConnection&& makeConnection)
{
    auto client = context.getParams().getCallback();
    using Server = HandlerServer<decltype(client)>;
    context.getResults().setHandler(
        messages::Handler::Client(kj::heap<Server>(std::move(client), std::forward<MakeConnection>(makeConnection))));
}

class PendingWalletTxServer final : public messages::PendingWalletTx::Server
{
public:
    PendingWalletTxServer(EventLoop& loop, std::unique_ptr<PendingWalletTx> impl) : loop(loop), impl(std::move(impl))
    {
    }

    kj::Promise<void> get(GetContext context) override
    {
        context.getResults().setResult(ToArray(Serialize(impl->get())));
        return kj::READY_NOW;
    }
    kj::Promise<void> getVirtualSize(GetVirtualSizeContext context) override
    {
        context.getResults().setResult(impl->getVirtualSize());
        return kj::READY_NOW;
    }
    kj::Promise<void> commit(CommitContext context) override
    {
        return loop.async(kj::mvCapture(context, [this](CommitContext context) {
            WalletValueMap mapValue;
            ReadWalletValueMap(mapValue, context.getParams().getMapValue());
            WalletOrderForm orderForm;
            ReadWalletOrderForm(orderForm, context.getParams().getOrderForm());
            std::string fromAccount = context.getParams().getFromAccount();
            std::string rejectReason;
            context.releaseParams();
            context.getResults().setResult(
                impl->commit(std::move(mapValue), std::move(orderForm), std::move(fromAccount), rejectReason));
            context.getResults().setRejectReason(rejectReason);
        }));
    }

    EventLoop& loop;
    std::unique_ptr<PendingWalletTx> impl;
};

class WalletServer final : public messages::Wallet::Server
{
public:
    WalletServer(EventLoop& loop, std::unique_ptr<Wallet> impl) : loop(loop), impl(std::move(impl)) {}

    kj::Promise<void> encryptWallet(EncryptWalletContext context) override
    {
        context.getResults().setResult(impl->encryptWallet(ToSecureString(context.getParams().getWalletPassphrase())));
        return kj::READY_NOW;
    }
    kj::Promise<void> isCrypted(IsCryptedContext context) override
    {
        context.getResults().setResult(impl->isCrypted());
        return kj::READY_NOW;
    }
    kj::Promise<void> lock(LockContext context) override
    {
        context.getResults().setResult(impl->lock());
        return kj::READY_NOW;
    }
    kj::Promise<void> unlock(UnlockContext context) override
    {
        context.getResults().setResult(impl->unlock(ToSecureString(context.getParams().getWalletPassphrase())));
        return kj::READY_NOW;
    }
    kj::Promise<void> isLocked(IsLockedContext context) override
    {
        context.getResults().setResult(impl->isLocked());
        return kj::READY_NOW;
    }
    kj::Promise<void> changeWalletPassphrase(ChangeWalletPassphraseContext context) override
    {
        context.getResults().setResult(
            impl->changeWalletPassphrase(ToSecureString(context.getParams().getOldWalletPassphrase()),
                ToSecureString(context.getParams().getNewWalletPassphrase())));
        return kj::READY_NOW;
    }
    kj::Promise<void> backupWallet(BackupWalletContext context) override
    {
        context.getResults().setResult(impl->backupWallet(context.getParams().getFilename()));
        return kj::READY_NOW;
    }
    kj::Promise<void> getKeyFromPool(GetKeyFromPoolContext context) override
    {
        CPubKey pubKey;
        context.getResults().setResult(impl->getKeyFromPool(pubKey, context.getParams().getInternal()));
        context.getResults().setPubKey(ToArray(Serialize(pubKey)));
        return kj::READY_NOW;
    }
    kj::Promise<void> getPubKey(GetPubKeyContext context) override
    {
        CPubKey pubKey;
        context.getResults().setResult(impl->getPubKey(Unserialize<CKeyID>(context.getParams().getAddress()), pubKey));
        context.getResults().setPubKey(ToArray(Serialize(pubKey)));
        return kj::READY_NOW;
    }
    kj::Promise<void> getKey(GetKeyContext context) override
    {
        CKey key;
        context.getResults().setResult(impl->getKey(Unserialize<CKeyID>(context.getParams().getAddress()), key));
        BuildKey(context.getResults().initKey(), key);
        return kj::READY_NOW;
    }
    kj::Promise<void> haveKey(HaveKeyContext context) override
    {
        context.getResults().setResult(impl->haveKey(Unserialize<CKeyID>(context.getParams().getAddress())));
        return kj::READY_NOW;
    }
    kj::Promise<void> haveWatchOnly(HaveWatchOnlyContext context) override
    {
        context.getResults().setResult(impl->haveWatchOnly());
        return kj::READY_NOW;
    }
    kj::Promise<void> setAddressBook(SetAddressBookContext context) override
    {
        CTxDestination dest;
        ReadTxDestination(dest, context.getParams().getDest());
        context.getResults().setResult(
            impl->setAddressBook(dest, context.getParams().getName(), context.getParams().getPurpose()));
        return kj::READY_NOW;
    }
    kj::Promise<void> delAddressBook(DelAddressBookContext context) override
    {
        CTxDestination dest;
        ReadTxDestination(dest, context.getParams().getDest());
        context.getResults().setResult(impl->delAddressBook(dest));
        return kj::READY_NOW;
    }
    kj::Promise<void> getAddress(GetAddressContext context) override
    {
        CTxDestination dest;
        ReadTxDestination(dest, context.getParams().getDest());
        std::string name;
        isminetype ismine;
        context.getResults().setResult(impl->getAddress(dest, &name, &ismine));
        context.getResults().setName(name);
        context.getResults().setIsmine(ismine);
        return kj::READY_NOW;
    }
    kj::Promise<void> getAddresses(GetAddressesContext context) override
    {
        auto addresses = impl->getAddresses();
        auto resultAddresses = context.getResults().initResult(addresses.size());
        size_t i = 0;
        for (const auto& address : addresses) {
            BuildWalletAddress(resultAddresses[i], address);
            ++i;
        }
        return kj::READY_NOW;
    }
    kj::Promise<void> getAccountAddresses(GetAccountAddressesContext context) override
    {
        auto addresses = impl->getAccountAddresses(context.getParams().getAccount());
        auto resultAddresses = context.getResults().initResult(addresses.size());
        size_t i = 0;
        for (const auto& address : addresses) {
            BuildTxDestination(resultAddresses[i], address);
            ++i;
        }
        return kj::READY_NOW;
    }
    kj::Promise<void> addDestData(AddDestDataContext context) override
    {
        CTxDestination dest;
        ReadTxDestination(dest, context.getParams().getDest());
        context.getResults().setResult(
            impl->addDestData(dest, context.getParams().getKey(), context.getParams().getValue()));
        return kj::READY_NOW;
    }
    kj::Promise<void> eraseDestData(EraseDestDataContext context) override
    {
        CTxDestination dest;
        ReadTxDestination(dest, context.getParams().getDest());
        context.getResults().setResult(impl->eraseDestData(dest, context.getParams().getKey()));
        return kj::READY_NOW;
    }
    kj::Promise<void> getDestValues(GetDestValuesContext context) override
    {
        std::vector<std::string> values;
        impl->getDestValues(context.getParams().getPrefix(), values);
        auto resultValues = context.getResults().initValues(values.size());
        size_t i = 0;
        for (const auto& value : values) {
            resultValues.set(i, ToArray(value));
            ++i;
        }
        return kj::READY_NOW;
    }
    kj::Promise<void> lockCoin(LockCoinContext context) override
    {
        impl->lockCoin(Unserialize<COutPoint>(context.getParams().getOutput()));
        return kj::READY_NOW;
    }
    kj::Promise<void> unlockCoin(UnlockCoinContext context) override
    {
        impl->unlockCoin(Unserialize<COutPoint>(context.getParams().getOutput()));
        return kj::READY_NOW;
    }
    kj::Promise<void> isLockedCoin(IsLockedCoinContext context) override
    {
        context.getResults().setResult(impl->isLockedCoin(Unserialize<COutPoint>(context.getParams().getOutput())));
        return kj::READY_NOW;
    }
    kj::Promise<void> listLockedCoins(ListLockedCoinsContext context) override
    {
        std::vector<COutPoint> outputs;
        impl->listLockedCoins(outputs);
        auto resultOutputs = context.getResults().initOutputs(outputs.size());
        size_t i = 0;
        for (const auto& output : outputs) {
            resultOutputs.set(i, ToArray(Serialize(output)));
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
        CCoinControl coinControl;
        const CCoinControl* coinControlPtr = nullptr;
        if (context.getParams().hasCoinControl()) {
            ReadCoinControl(coinControl, context.getParams().getCoinControl());
            coinControlPtr = &coinControl;
        }
        int changePos = context.getParams().getChangePos();
        CAmount fee;
        std::string failReason;
        if (auto tx = impl->createTransaction(
                recipients, coinControlPtr, context.getParams().getSign(), changePos, fee, failReason)) {
            context.getResults().setResult(
                messages::PendingWalletTx::Client(kj::heap<PendingWalletTxServer>(loop, std::move(tx))));
        }
        context.getResults().setChangePos(changePos);
        context.getResults().setFee(fee);
        context.getResults().setFailReason(failReason);
        return kj::READY_NOW;
    }
    kj::Promise<void> transactionCanBeAbandoned(TransactionCanBeAbandonedContext context) override
    {
        context.getResults().setResult(
            impl->transactionCanBeAbandoned(ToBlob<uint256>(context.getParams().getTxHash())));
        return kj::READY_NOW;
    }
    kj::Promise<void> abandonTransaction(AbandonTransactionContext context) override
    {
        context.getResults().setResult(impl->abandonTransaction(ToBlob<uint256>(context.getParams().getTxHash())));
        return kj::READY_NOW;
    }
    kj::Promise<void> getTx(GetTxContext context) override
    {
        auto tx = impl->getTx(ToBlob<uint256>(context.getParams().getTxHash()));
        if (tx) {
            context.getResults().setResult(ToArray(Serialize(*tx)));
        }
        return kj::READY_NOW;
    }
    kj::Promise<void> getWalletTx(GetWalletTxContext context) override
    {
        BuildWalletTx(
            context.getResults().initResult(), impl->getWalletTx(ToBlob<uint256>(context.getParams().getTxHash())));
        return kj::READY_NOW;
    }
    kj::Promise<void> getWalletTxs(GetWalletTxsContext context) override
    {
        auto walletTxs = impl->getWalletTxs();
        auto resultWalletTxs = context.getResults().initResult(walletTxs.size());
        size_t i = 0;
        for (const auto& walletTx : walletTxs) {
            BuildWalletTx(resultWalletTxs[i], walletTx);
            ++i;
        }
        return kj::READY_NOW;
    }
    kj::Promise<void> tryGetTxStatus(TryGetTxStatusContext context) override
    {
        WalletTxStatus txStatus;
        int numBlocks;
        int64_t adjustedTime;
        context.getResults().setResult(
            impl->tryGetTxStatus(ToBlob<uint256>(context.getParams().getTxHash()), txStatus, numBlocks, adjustedTime));
        BuildWalletTxStatus(context.getResults().initTxStatus(), txStatus);
        context.getResults().setNumBlocks(numBlocks);
        context.getResults().setAdjustedTime(adjustedTime);
        return kj::READY_NOW;
    }
    kj::Promise<void> getWalletTxDetails(GetWalletTxDetailsContext context) override
    {
        WalletTxStatus txStatus;
        WalletOrderForm orderForm;
        bool inMempool;
        int numBlocks;
        int64_t adjustedTime;
        auto walletTx = impl->getWalletTxDetails(
            ToBlob<uint256>(context.getParams().getTxHash()), txStatus, orderForm, inMempool, numBlocks, adjustedTime);
        BuildWalletTx(context.getResults().initResult(), walletTx);
        BuildWalletTxStatus(context.getResults().initTxStatus(), txStatus);
        BuildWalletOrderForm(context.getResults().initOrderForm(), orderForm);
        context.getResults().setInMempool(inMempool);
        context.getResults().setNumBlocks(numBlocks);
        context.getResults().setAdjustedTime(adjustedTime);
        return kj::READY_NOW;
    }
    kj::Promise<void> getBalances(GetBalancesContext context) override
    {
        BuildWalletBalances(context.getResults().initResult(), impl->getBalances());
        return kj::READY_NOW;
    }
    kj::Promise<void> tryGetBalances(TryGetBalancesContext context) override
    {
        WalletBalances balances;
        int numBlocks;
        context.getResults().setResult(impl->tryGetBalances(balances, numBlocks));
        BuildWalletBalances(context.getResults().initBalances(), balances);
        context.getResults().setNumBlocks(numBlocks);
        return kj::READY_NOW;
    }
    kj::Promise<void> getBalance(GetBalanceContext context) override
    {
        context.getResults().setResult(impl->getBalance());
        return kj::READY_NOW;
    }
    kj::Promise<void> getUnconfirmedBalance(GetUnconfirmedBalanceContext context) override
    {
        context.getResults().setResult(impl->getUnconfirmedBalance());
        return kj::READY_NOW;
    }
    kj::Promise<void> getImmatureBalance(GetImmatureBalanceContext context) override
    {
        context.getResults().setResult(impl->getImmatureBalance());
        return kj::READY_NOW;
    }
    kj::Promise<void> getWatchOnlyBalance(GetWatchOnlyBalanceContext context) override
    {
        context.getResults().setResult(impl->getWatchOnlyBalance());
        return kj::READY_NOW;
    }
    kj::Promise<void> getUnconfirmedWatchOnlyBalance(GetUnconfirmedWatchOnlyBalanceContext context) override
    {
        context.getResults().setResult(impl->getUnconfirmedWatchOnlyBalance());
        return kj::READY_NOW;
    }
    kj::Promise<void> getImmatureWatchOnlyBalance(GetImmatureWatchOnlyBalanceContext context) override
    {
        context.getResults().setResult(impl->getImmatureWatchOnlyBalance());
        return kj::READY_NOW;
    }
    kj::Promise<void> getAvailableBalance(GetAvailableBalanceContext context) override
    {
        CCoinControl coinControl;
        ReadCoinControl(coinControl, context.getParams().getCoinControl());
        context.getResults().setResult(impl->getAvailableBalance(coinControl));
        return kj::READY_NOW;
    }
    kj::Promise<void> isMine(IsMineContext context) override
    {
        if (context.getParams().hasTxin()) {
            context.getResults().setResult(impl->isMine(Unserialize<CTxIn>(context.getParams().getTxin())));
        }
        if (context.getParams().hasTxout()) {
            context.getResults().setResult(impl->isMine(Unserialize<CTxOut>(context.getParams().getTxout())));
        }
        return kj::READY_NOW;
    }
    kj::Promise<void> getDebit(GetDebitContext context) override
    {
        context.getResults().setResult(impl->getDebit(
            Unserialize<CTxIn>(context.getParams().getTxin()), isminefilter(context.getParams().getFilter())));
        return kj::READY_NOW;
    }
    kj::Promise<void> getCredit(GetCreditContext context) override
    {
        context.getResults().setResult(impl->getCredit(
            Unserialize<CTxOut>(context.getParams().getTxout()), isminefilter(context.getParams().getFilter())));
        return kj::READY_NOW;
    }
    kj::Promise<void> listCoins(ListCoinsContext context) override
    {
        BuildCoinsList(context.getResults().initResult(), impl->listCoins());
        return kj::READY_NOW;
    }
    kj::Promise<void> getCoins(GetCoinsContext context) override
    {
        std::vector<COutPoint> outputs;
        outputs.reserve(context.getParams().getOutputs().size());
        for (const auto& output : context.getParams().getOutputs()) {
            outputs.emplace_back(Unserialize<COutPoint>(output));
        }
        auto coins = impl->getCoins(outputs);
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
        context.getResults().setResult(impl->hdEnabled());
        return kj::READY_NOW;
    }
    kj::Promise<void> handleShowProgress(HandleShowProgressContext context) override
    {
        SetHandler(context, [this](messages::ShowProgressCallback::Client& client) {
            return impl->handleShowProgress([this, &client](const std::string& title, int progress) {
                auto call = MakeCall(this->loop, [&]() {
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
            return impl->handleStatusChanged([this, &client]() {
                auto call = MakeCall(this->loop, [&]() {
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
            return impl->handleAddressBookChanged([this, &client](const CTxDestination& address,
                const std::string& label, bool isMine, const std::string& purpose, ChangeType status) {
                auto call = MakeCall(this->loop, [&]() {
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
            return impl->handleTransactionChanged([this, &client](const uint256& hashTx, ChangeType status) {
                auto call = MakeCall(this->loop, [&]() {
                    auto request = client.callRequest();
                    request.setHashTx(FromBlob(hashTx));
                    request.setStatus(status);
                    return request;
                });
                return call.send();
            });

        });
        return kj::READY_NOW;
    }
    kj::Promise<void> handleWatchonlyChanged(HandleWatchonlyChangedContext context) override
    {
        SetHandler(context, [this](messages::WatchonlyChangedCallback::Client& client) {
            return impl->handleWatchonlyChanged([this, &client](bool haveWatchOnly) {
                auto call = MakeCall(this->loop, [&]() {
                    auto request = client.callRequest();
                    request.setHaveWatchOnly(haveWatchOnly);
                    return request;
                });
                return call.send();
            });

        });
        return kj::READY_NOW;
    }

    EventLoop& loop;
    std::unique_ptr<Wallet> impl;
};

class RpcTimer : public ::RPCTimerBase
{
public:
    RpcTimer(EventLoop& loop, boost::function<void(void)>& fn, int64_t millis)
        : fn(fn), promise(loop.ioContext.provider->getTimer()
                              .afterDelay(millis * kj::MILLISECONDS)
                              .then([this]() { this->fn(); })
                              .eagerlyEvaluate(nullptr))
    {
    }
    ~RpcTimer() noexcept override {}
    boost::function<void(void)> fn;
    kj::Promise<void> promise;
};

class RpcTimerInterface : public ::RPCTimerInterface
{
public:
    RpcTimerInterface(EventLoop& loop) : loop(loop) {}
    const char* Name() override { return "Cap'n Proto"; }
    RPCTimerBase* NewTimer(boost::function<void(void)>& fn, int64_t millis) override
    {
        return new RpcTimer(loop, fn, millis);
    }
    EventLoop& loop;
};

class NodeServer final : public messages::Node::Server
{
public:
    NodeServer(EventLoop& loop, Node* impl) : loop(loop), impl(impl) {}

    kj::Promise<void> parseParameters(ParseParametersContext context) override
    {
        std::vector<const char*> argv(context.getParams().getArgv().size());
        size_t i = 0;
        for (const std::string& arg : context.getParams().getArgv()) {
            argv[i] = arg.c_str();
            ++i;
        }
        impl->parseParameters(argv.size(), argv.data());
        return kj::READY_NOW;
    }
    kj::Promise<void> softSetArg(SoftSetArgContext context) override
    {
        context.getResults().setResult(impl->softSetArg(context.getParams().getArg(), context.getParams().getValue()));
        return kj::READY_NOW;
    }
    kj::Promise<void> softSetBoolArg(SoftSetBoolArgContext context) override
    {
        context.getResults().setResult(
            impl->softSetBoolArg(context.getParams().getArg(), context.getParams().getValue()));
        return kj::READY_NOW;
    }
    kj::Promise<void> readConfigFile(ReadConfigFileContext context) override
    {
        try {
            impl->readConfigFile(context.getParams().getConfPath());
        } catch (const std::exception& e) {
            context.getResults().setError(e.what());
        }
        return kj::READY_NOW;
    }
    kj::Promise<void> selectParams(SelectParamsContext context) override
    {
        try {
            impl->selectParams(context.getParams().getNetwork());
        } catch (const std::exception& e) {
            context.getResults().setError(e.what());
        }
        return kj::READY_NOW;
    }
    kj::Promise<void> getNetwork(GetNetworkContext context) override
    {
        context.getResults().setResult(impl->getNetwork());
        return kj::READY_NOW;
    }
    kj::Promise<void> initLogging(InitLoggingContext context) override
    {
        impl->initLogging();
        return kj::READY_NOW;
    }
    kj::Promise<void> initParameterInteraction(InitParameterInteractionContext context) override
    {
        impl->initParameterInteraction();
        return kj::READY_NOW;
    }
    kj::Promise<void> getWarnings(GetWarningsContext context) override
    {
        context.getResults().setResult(impl->getWarnings(context.getParams().getType()));
        return kj::READY_NOW;
    }
    kj::Promise<void> getLogCategories(GetLogCategoriesContext context) override
    {
        context.getResults().setResult(impl->getLogCategories());
        return kj::READY_NOW;
    }
    kj::Promise<void> appInit(AppInitContext context) override
    {
        context.releaseParams();
        return loop.async(kj::mvCapture(context, [this](AppInitContext context) {
            try {
                context.getResults().setResult(impl->appInit());
            } catch (const std::exception& e) {
                context.getResults().setError(e.what());
            }
        }));
    }
    kj::Promise<void> appShutdown(AppShutdownContext context) override
    {
        impl->appShutdown();
        return kj::READY_NOW;
    }
    kj::Promise<void> startShutdown(StartShutdownContext context) override
    {
        impl->startShutdown();
        return kj::READY_NOW;
    }
    kj::Promise<void> shutdownRequested(ShutdownRequestedContext context) override
    {
        context.getResults().setResult(impl->shutdownRequested());
        return kj::READY_NOW;
    }
    kj::Promise<void> helpMessage(HelpMessageContext context) override
    {
        context.getResults().setResult(impl->helpMessage(HelpMessageMode(context.getParams().getMode())));
        return kj::READY_NOW;
    }
    kj::Promise<void> mapPort(MapPortContext context) override
    {
        impl->mapPort(context.getParams().getUseUPnP());
        return kj::READY_NOW;
    }
    kj::Promise<void> getProxy(GetProxyContext context) override
    {
        proxyType proxyInfo;
        context.getResults().setResult(impl->getProxy(Network(context.getParams().getNet()), proxyInfo));
        BuildProxy(context.getResults().initProxyInfo(), proxyInfo);
        return kj::READY_NOW;
    }
    kj::Promise<void> getNodeCount(GetNodeCountContext context) override
    {
        context.getResults().setResult(impl->getNodeCount(CConnman::NumConnections(context.getParams().getFlags())));
        return kj::READY_NOW;
    }
    kj::Promise<void> getNodesStats(GetNodesStatsContext context) override
    {
        Node::NodesStats stats;
        if (impl->getNodesStats(stats)) {
            auto resultStats = context.getResults().initStats(stats.size());
            size_t i = 0;
            for (const auto& nodeStats : stats) {
                BuildNodeStats(resultStats[i], std::get<0>(nodeStats));
                if (std::get<1>(nodeStats)) {
                    BuildNodeStateStats(resultStats[i].initStateStats(), std::get<2>(nodeStats));
                }
                ++i;
            }
        }
        return kj::READY_NOW;
    }
    kj::Promise<void> getBanned(GetBannedContext context) override
    {
        banmap_t banMap;
        if (impl->getBanned(banMap)) {
            BuildBanMap(context.getResults().initBanMap(), banMap);
        }
        return kj::READY_NOW;
    }
    kj::Promise<void> ban(BanContext context) override
    {
        context.getResults().setResult(impl->ban(Unserialize<CNetAddr>(context.getParams().getNetAddr()),
            BanReason(context.getParams().getReason()), context.getParams().getBantimeoffset()));
        return kj::READY_NOW;
    }
    kj::Promise<void> unban(UnbanContext context) override
    {
        context.getResults().setResult(impl->unban(Unserialize<CSubNet>(context.getParams().getIp())));
        return kj::READY_NOW;
    }
    kj::Promise<void> disconnect(DisconnectContext context) override
    {
        context.getResults().setResult(impl->disconnect(context.getParams().getId()));
        return kj::READY_NOW;
    }
    kj::Promise<void> getTotalBytesRecv(GetTotalBytesRecvContext context) override
    {
        context.getResults().setResult(impl->getTotalBytesRecv());
        return kj::READY_NOW;
    }
    kj::Promise<void> getTotalBytesSent(GetTotalBytesSentContext context) override
    {
        context.getResults().setResult(impl->getTotalBytesSent());
        return kj::READY_NOW;
    }
    kj::Promise<void> getMempoolSize(GetMempoolSizeContext context) override
    {
        context.getResults().setResult(impl->getMempoolSize());
        return kj::READY_NOW;
    }
    kj::Promise<void> getMempoolDynamicUsage(GetMempoolDynamicUsageContext context) override
    {
        context.getResults().setResult(impl->getMempoolDynamicUsage());
        return kj::READY_NOW;
    }
    kj::Promise<void> getHeaderTip(GetHeaderTipContext context) override
    {
        int height;
        int64_t blockTime;
        context.getResults().setResult(impl->getHeaderTip(height, blockTime));
        context.getResults().setHeight(height);
        context.getResults().setBlockTime(blockTime);
        return kj::READY_NOW;
    }
    kj::Promise<void> getNumBlocks(GetNumBlocksContext context) override
    {
        context.getResults().setResult(impl->getNumBlocks());
        return kj::READY_NOW;
    }
    kj::Promise<void> getLastBlockTime(GetLastBlockTimeContext context) override
    {
        context.getResults().setResult(impl->getLastBlockTime());
        return kj::READY_NOW;
    }
    kj::Promise<void> getVerificationProgress(GetVerificationProgressContext context) override
    {
        context.getResults().setResult(impl->getVerificationProgress());
        return kj::READY_NOW;
    }
    kj::Promise<void> isInitialBlockDownload(IsInitialBlockDownloadContext context) override
    {
        context.getResults().setResult(impl->isInitialBlockDownload());
        return kj::READY_NOW;
    }
    kj::Promise<void> getReindex(GetReindexContext context) override
    {
        context.getResults().setResult(impl->getReindex());
        return kj::READY_NOW;
    }
    kj::Promise<void> getImporting(GetImportingContext context) override
    {
        context.getResults().setResult(impl->getImporting());
        return kj::READY_NOW;
    }
    kj::Promise<void> setNetworkActive(SetNetworkActiveContext context) override
    {
        impl->setNetworkActive(context.getParams().getActive());
        return kj::READY_NOW;
    }
    kj::Promise<void> getNetworkActive(GetNetworkActiveContext context) override
    {
        context.getResults().setResult(impl->getNetworkActive());
        return kj::READY_NOW;
    }
    kj::Promise<void> getTxConfirmTarget(GetTxConfirmTargetContext context) override
    {
        context.getResults().setResult(impl->getTxConfirmTarget());
        return kj::READY_NOW;
    }
    kj::Promise<void> getWalletRbf(GetWalletRbfContext context) override
    {
        context.getResults().setResult(impl->getWalletRbf());
        return kj::READY_NOW;
    }
    kj::Promise<void> getRequiredFee(GetRequiredFeeContext context) override
    {
        context.getResults().setResult(impl->getRequiredFee(context.getParams().getTxBytes()));
        return kj::READY_NOW;
    }
    kj::Promise<void> getMinimumFee(GetMinimumFeeContext context) override
    {
        context.getResults().setResult(impl->getMinimumFee(context.getParams().getTxBytes()));
        return kj::READY_NOW;
    }
    kj::Promise<void> getMaxTxFee(GetMaxTxFeeContext context) override
    {
        context.getResults().setResult(impl->getMaxTxFee());
        return kj::READY_NOW;
    }
    kj::Promise<void> estimateSmartFee(EstimateSmartFeeContext context) override
    {
        int answerFoundAtBlocks;
        context.getResults().setResult(
            ToArray(Serialize(impl->estimateSmartFee(context.getParams().getNBlocks(), &answerFoundAtBlocks))));
        context.getResults().setAnswerFoundAtBlocks(answerFoundAtBlocks);
        return kj::READY_NOW;
    }
    kj::Promise<void> getDustRelayFee(GetDustRelayFeeContext context) override
    {
        context.getResults().setResult(ToArray(Serialize(impl->getDustRelayFee())));
        return kj::READY_NOW;
    }
    kj::Promise<void> getFallbackFee(GetFallbackFeeContext context) override
    {
        context.getResults().setResult(ToArray(Serialize(impl->getFallbackFee())));
        return kj::READY_NOW;
    }
    kj::Promise<void> getPayTxFee(GetPayTxFeeContext context) override
    {
        context.getResults().setResult(ToArray(Serialize(impl->getPayTxFee())));
        return kj::READY_NOW;
    }
    kj::Promise<void> setPayTxFee(SetPayTxFeeContext context) override
    {
        impl->setPayTxFee(Unserialize<CFeeRate>(context.getParams().getRate()));
        return kj::READY_NOW;
    }
    kj::Promise<void> executeRpc(ExecuteRpcContext context) override
    {
        UniValue params;
        ReadUniValue(params, context.getParams().getParams());
        try {
            BuildUniValue(context.getResults().initResult(),
                impl->executeRpc(ToString(context.getParams().getCommand()), params));
        } catch (const UniValue& e) {
            BuildUniValue(context.getResults().initRpcError(), e);
        } catch (const std::exception& e) {
            context.getResults().setError(e.what());
        }
        return kj::READY_NOW;
    }
    kj::Promise<void> listRpcCommands(ListRpcCommandsContext context) override
    {
        auto commands = impl->listRpcCommands();
        auto resultCommands = context.getResults().initResult(commands.size());
        size_t i = 0;
        for (const auto& command : commands) {
            resultCommands.set(i, command);
            ++i;
        }
        return kj::READY_NOW;
    }
    kj::Promise<void> rpcSetTimerInterfaceIfUnset(RpcSetTimerInterfaceIfUnsetContext context) override
    {
        if (!timerInterface) {
            timerInterface = MakeUnique<RpcTimerInterface>(loop);
        }
        impl->rpcSetTimerInterfaceIfUnset(timerInterface.get());
        return kj::READY_NOW;
    }
    kj::Promise<void> rpcUnsetTimerInterface(RpcUnsetTimerInterfaceContext context) override
    {
        impl->rpcUnsetTimerInterface(timerInterface.get());
        timerInterface.reset();
        return kj::READY_NOW;
    }
    kj::Promise<void> getUnspentOutputs(GetUnspentOutputsContext context) override
    {
        CCoins coins;
        context.getResults().setResult(
            impl->getUnspentOutputs(ToBlob<uint256>(context.getParams().getTxHash()), coins));
        context.getResults().setCoins(ToArray(Serialize(coins)));
        return kj::READY_NOW;
    }
    kj::Promise<void> getWallet(GetWalletContext context) override
    {
        context.getResults().setResult(messages::Wallet::Client(kj::heap<WalletServer>(loop, impl->getWallet())));
        return kj::READY_NOW;
    }
    kj::Promise<void> handleInitMessage(HandleInitMessageContext context) override
    {
        SetHandler(context, [this](messages::InitMessageCallback::Client& client) {
            return impl->handleInitMessage([this, &client](const std::string& message) {
                auto call = MakeCall(this->loop, [&]() {
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
            return impl->handleMessageBox(
                [this, &client](const std::string& message, const std::string& caption, unsigned int style) {
                    auto call = MakeCall(this->loop, [&]() {
                        auto request = client.callRequest();
                        request.setMessage(message);
                        request.setCaption(caption);
                        request.setStyle(style);
                        return request;
                    });
                    return call.send([&]() { return call.response->getResult(); });
                });

        });
        return kj::READY_NOW;
    }
    kj::Promise<void> handleQuestion(HandleQuestionContext context) override
    {
        SetHandler(context, [this](messages::QuestionCallback::Client& client) {
            return impl->handleQuestion([this, &client](const std::string& message,
                const std::string& nonInteractiveMessage, const std::string& caption, unsigned int style) {
                auto call = MakeCall(this->loop, [&]() {
                    auto request = client.callRequest();
                    request.setMessage(message);
                    request.setNonInteractiveMessage(nonInteractiveMessage);
                    request.setCaption(caption);
                    request.setStyle(style);
                    return request;
                });
                return call.send([&]() { return call.response->getResult(); });
            });

        });
        return kj::READY_NOW;
    }
    kj::Promise<void> handleShowProgress(HandleShowProgressContext context) override
    {
        SetHandler(context, [this](messages::ShowProgressCallback::Client& client) {
            return impl->handleShowProgress([this, &client](const std::string& title, int progress) {
                auto call = MakeCall(this->loop, [&]() {
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
        SetHandler(context, [this](messages::LoadWalletCallback::Client& client) {
            return impl->handleLoadWallet([this, &client](std::unique_ptr<Wallet> wallet) {
                auto call = MakeCall(this->loop, [&]() {
                    auto request = client.callRequest();
                    request.setWallet(messages::Wallet::Client(kj::heap<WalletServer>(this->loop, std::move(wallet))));
                    return request;
                });
                return call.send();
            });

        });
        return kj::READY_NOW;
    }
    kj::Promise<void> handleNotifyNumConnectionsChanged(HandleNotifyNumConnectionsChangedContext context) override
    {
        SetHandler(context, [this](messages::NotifyNumConnectionsChangedCallback::Client& client) {
            return impl->handleNotifyNumConnectionsChanged([this, &client](int newNumConnections) {
                auto call = MakeCall(this->loop, [&]() {
                    auto request = client.callRequest();
                    request.setNewNumConnections(newNumConnections);
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
            return impl->handleNotifyNetworkActiveChanged([this, &client](bool networkActive) {
                auto call = MakeCall(this->loop, [&]() {
                    auto request = client.callRequest();
                    request.setNetworkActive(networkActive);
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
            return impl->handleNotifyAlertChanged([this, &client]() {
                auto call = MakeCall(this->loop, [&]() {
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
            return impl->handleBannedListChanged([this, &client]() {
                auto call = MakeCall(this->loop, [&]() {
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
            return impl->handleNotifyBlockTip(
                [this, &client](bool initialDownload, int height, int64_t blockTime, double verificationProgress) {
                    auto call = MakeCall(this->loop, [&]() {
                        auto request = client.callRequest();
                        request.setInitialDownload(initialDownload);
                        request.setHeight(height);
                        request.setBlockTime(blockTime);
                        request.setVerificationProgress(verificationProgress);
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
            return impl->handleNotifyHeaderTip(
                [this, &client](bool initialDownload, int height, int64_t blockTime, double verificationProgress) {
                    auto call = MakeCall(this->loop, [&]() {
                        auto request = client.callRequest();
                        request.setInitialDownload(initialDownload);
                        request.setHeight(height);
                        request.setBlockTime(blockTime);
                        request.setVerificationProgress(verificationProgress);
                        return request;
                    });
                    return call.send();
                });

        });
        return kj::READY_NOW;
    }

    EventLoop& loop;
    Node* impl;
    std::unique_ptr<RpcTimerInterface> timerInterface;
};

} // namespace

void StartNodeServer(ipc::Node& node, int fd)
{
    EventLoop loop;
    auto stream = loop.ioContext.lowLevelProvider->wrapSocketFd(fd, kj::LowLevelAsyncIoProvider::TAKE_OWNERSHIP);
    auto network =
        ::capnp::TwoPartyVatNetwork(*stream, ::capnp::rpc::twoparty::Side::SERVER, ::capnp::ReaderOptions());
    loop.taskSet.add(network.onDisconnect().then([&loop]() { loop.shutdown(); }));
    auto rpcServer = ::capnp::makeRpcServer(network, ::capnp::Capability::Client(kj::heap<NodeServer>(loop, &node)));
    loop.loop();
}

} // namespace capnp

} // namespace ipc
