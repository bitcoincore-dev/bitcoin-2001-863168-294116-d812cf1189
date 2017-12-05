#include <interfaces/capnp/proxy-impl.h>
#include <interfaces/capnp/messages.capnp.h>
#include <interfaces/capnp/messages.capnp.proxy.h>
#include <interfaces/capnp/messages.capnp.proxy-impl.h>
#include <interfaces/capnp/messages-impl.h>
#include <interfaces/config.h>
#include <interfaces/node.h>
#include <netaddress.h>
#include <policy/feerate.h>
#include <policy/fees.h>
#include <primitives/transaction.h>
#include <protocol.h>
#include <pubkey.h>
#include <rpc/server.h>
#include <scheduler.h>
#include <script/ismine.h>
#include <script/script.h>
#include <streams.h>
#include <wallet/coincontrol.h>
#include <wallet/wallet.h>

#include <boost/core/explicit_operator_bool.hpp>
#include <boost/move/utility_core.hpp>
#include <boost/optional/optional.hpp>
#include <boost/variant/get.hpp>
#include <capnp/blob.h>
#include <capnp/list.h>
#include <clientversion.h>
#include <init.h>
#include <key.h>
#include <memory>
#include <net.h>
#include <net_processing.h>
#include <netbase.h>
#include <stddef.h>
#include <stdexcept>
#include <stdint.h>
#include <tuple>
#include <univalue.h>
#include <utility>

namespace interfaces {
namespace capnp {

void BuildGlobalArgs(InvokeContext& invoke_context, messages::GlobalArgs::Builder&& builder)
{

    const auto& args = static_cast<const GlobalArgs&>(::gArgs);
    LOCK(args.cs_args);
    BuildField(TypeList<GlobalArgs>(), BuildFieldPriority(), invoke_context, args, Make<ValueField>(builder));
}


void ReadGlobalArgs(InvokeContext& invoke_context, const messages::GlobalArgs::Reader& reader)
{
    auto& args = static_cast<GlobalArgs&>(::gArgs);
    {
        LOCK(args.cs_args);
        ReadFieldUpdate(TypeList<GlobalArgs>(), invoke_context, Make<ValueField>(reader), args);
    }
    // TODO in future PR: Refactor bitcoin startup code, dedup this with AppInit.
    SelectParams(gArgs.GetChainName());
    InitLogging();
    InitParameterInteraction();
    if (!AppInitBasicSetup() || !AppInitParameterInteraction() || !AppInitSanityChecks(false /* lock_data_dir */)) {
        throw std::runtime_error("Bitcoin application initialization failed");
    }
    // TODO in future PR: Refactor bitcoin startup code, dedup this with AppInitMain.
    if (g_logger->m_print_to_file && !g_logger->OpenDebugLog()) {
        throw std::runtime_error("Could not open wallet debug log file");
    }
}

void BuildMessage(const UniValue& univalue, messages::UniValue::Builder&& builder)
{
    builder.setType(univalue.getType());
    if (univalue.getType() == UniValue::VARR || univalue.getType() == UniValue::VOBJ) {
        builder.setValue(univalue.write());
    } else {
        builder.setValue(univalue.getValStr());
    }
}

void ReadMessage(InvokeContext& invoke_context, const messages::UniValue::Reader& reader, UniValue& univalue)
{
    if (reader.getType() == UniValue::VARR || reader.getType() == UniValue::VOBJ) {
        if (!univalue.read(ToString(reader.getValue()))) {
            throw std::runtime_error("Could not parse UniValue");
        }
    } else {
        univalue = UniValue(UniValue::VType(reader.getType()), ToString(reader.getValue()));
    }
}

void BuildMessage(const CTxDestination& dest, messages::TxDestination::Builder&& builder)
{
    if (const CKeyID* keyId = boost::get<CKeyID>(&dest)) {
        builder.setKeyId(ToArray(Serialize(*keyId)));
    } else if (const CScriptID* scriptId = boost::get<CScriptID>(&dest)) {
        builder.setScriptId(ToArray(Serialize(*scriptId)));
    }
}

void ReadMessage(InvokeContext& invoke_context, const messages::TxDestination::Reader& reader, CTxDestination& dest)
{
    if (reader.hasKeyId()) {
        dest = Unserialize<CKeyID>(reader.getKeyId());
    } else if (reader.hasScriptId()) {
        dest = Unserialize<CScriptID>(reader.getScriptId());
    }
}

void BuildMessage(CValidationState const& state, messages::ValidationState::Builder&& builder)
{
    int dos = 0;
    builder.setValid(!state.IsInvalid(dos));
    builder.setError(state.IsError());
    builder.setDosCode(dos);
    builder.setRejectCode(state.GetRejectCode());
    const std::string& reject_reason = state.GetRejectReason();
    if (!reject_reason.empty()) {
        builder.setRejectReason(reject_reason);
    }
    builder.setCorruptionPossible(state.CorruptionPossible());
    const std::string& debug_message = state.GetDebugMessage();
    if (!debug_message.empty()) {
        builder.setDebugMessage(debug_message);
    }
}

void ReadMessage(InvokeContext& invoke_context,
    messages::ValidationState::Reader const& reader,
    CValidationState& state)
{
    if (reader.getValid()) {
        assert(!reader.getError());
        assert(!reader.getDosCode());
        assert(!reader.getRejectCode());
        assert(!reader.hasRejectReason());
        if (reader.getCorruptionPossible()) {
            state.SetCorruptionPossible();
        }
        assert(!reader.hasDebugMessage());
    } else {
        state.DoS(reader.getDosCode(), false /* ret */, reader.getRejectCode(), reader.getRejectReason(),
            reader.getCorruptionPossible(), reader.getDebugMessage());
        if (reader.getError()) {
            state.Error({} /* reject reason */);
        }
    }
}

void BuildMessage(const CKey& key, messages::Key::Builder&& builder)
{
    builder.setSecret(FromBlob(key));
    builder.setIsCompressed(key.IsCompressed());
}

void ReadMessage(InvokeContext& invoke_context, const messages::Key::Reader& reader, CKey& key)
{
    auto secret = reader.getSecret();
    key.Set(secret.begin(), secret.end(), reader.getIsCompressed());
}

void ReadMessage(InvokeContext& invoke_context,
    messages::NodeStats::Reader const& reader,
    std::tuple<CNodeStats, bool, CNodeStateStats>& node_stats)
{
    auto&& node = std::get<0>(node_stats);
    ReadFieldUpdate(TypeList<Decay<decltype(node)>>(), invoke_context, Make<ValueField>(reader), node);
    if ((std::get<1>(node_stats) = reader.hasStateStats())) {
        auto&& state = std::get<2>(node_stats);
        ReadFieldUpdate(
            TypeList<Decay<decltype(state)>>(), invoke_context, Make<ValueField>(reader.getStateStats()), state);
    }
}

void BuildMessage(const CCoinControl& coin_control, messages::CoinControl::Builder&& builder)
{
    BuildMessage(coin_control.destChange, builder.initDestChange());
    if (coin_control.m_change_type) {
        builder.setHasChangeType(true);
        builder.setChangeType(static_cast<int>(*coin_control.m_change_type));
    }
    builder.setAllowOtherInputs(coin_control.fAllowOtherInputs);
    builder.setAllowWatchOnly(coin_control.fAllowWatchOnly);
    builder.setOverrideFeeRate(coin_control.fOverrideFeeRate);
    if (coin_control.m_feerate) {
        builder.setFeeRate(ToArray(Serialize(*coin_control.m_feerate)));
    }
    if (coin_control.m_confirm_target) {
        builder.setHasConfirmTarget(true);
        builder.setConfirmTarget(*coin_control.m_confirm_target);
    }
    if (coin_control.m_signal_bip125_rbf) {
        builder.setHasSignalRbf(true);
        builder.setSignalRbf(*coin_control.m_signal_bip125_rbf);
    }
    builder.setFeeMode(int32_t(coin_control.m_fee_mode));
    std::vector<COutPoint> selected;
    coin_control.ListSelected(selected);
    auto builder_selected = builder.initSetSelected(selected.size());
    size_t i = 0;
    for (const COutPoint& output : selected) {
        builder_selected.set(i, ToArray(Serialize(output)));
        ++i;
    }
}

void ReadMessage(InvokeContext& invoke_context,
    const messages::CoinControl::Reader& reader,
    CCoinControl& coin_control)
{
    ReadMessage(invoke_context, reader.getDestChange(), coin_control.destChange);
    if (reader.getHasChangeType()) {
        coin_control.m_change_type = OutputType(reader.getChangeType());
    }
    coin_control.fAllowOtherInputs = reader.getAllowOtherInputs();
    coin_control.fAllowWatchOnly = reader.getAllowWatchOnly();
    coin_control.fOverrideFeeRate = reader.getOverrideFeeRate();
    if (reader.hasFeeRate()) {
        coin_control.m_feerate = Unserialize<CFeeRate>(reader.getFeeRate());
    }
    if (reader.getHasConfirmTarget()) {
        coin_control.m_confirm_target = reader.getConfirmTarget();
    }
    if (reader.getHasSignalRbf()) {
        coin_control.m_signal_bip125_rbf = reader.getSignalRbf();
    }
    coin_control.m_fee_mode = FeeEstimateMode(reader.getFeeMode());
    for (const auto output : reader.getSetSelected()) {
        coin_control.Select(Unserialize<COutPoint>(output));
    }
}


std::unique_ptr<ChainClient> ProxyServerMethodTraits<messages::Init::MakeWalletClientParams>::invoke(Context& context,
    std::vector<std::string> wallet_filenames)
{
    auto params = context.call_context.getParams();

    auto chain = MakeUnique<ProxyClient<messages::Chain>>(params.getChain(), *context.proxy_server.m_loop);
    auto client = context.proxy_server.m_impl->makeWalletClient(*chain, std::move(wallet_filenames));
    client->addCloseHook(MakeUnique<Deleter<decltype(chain)>>(std::move(chain)));
    return client;
}

std::unique_ptr<Handler> ProxyServerMethodTraits<messages::Chain::HandleNotificationsParams>::invoke(Context& context)
{
    auto params = context.call_context.getParams();
    auto notifications =
        MakeUnique<ProxyClient<messages::ChainNotifications>>(params.getNotifications(), *context.proxy_server.m_loop);
    auto handler = context.proxy_server.m_impl->handleNotifications(*notifications);
    handler->addCloseHook(MakeUnique<Deleter<decltype(notifications)>>(std::move(notifications)));
    return handler;
}

std::unique_ptr<Handler> ProxyServerMethodTraits<messages::Chain::HandleRpcParams>::invoke(Context& context)
{
    auto params = context.call_context.getParams();
    auto command = params.getCommand();

    // FIXME: Should use ReadFieldReturn
    CRPCCommand::Actor actor;
    ReadFieldUpdate(TypeList<decltype(actor)>(), context, Make<ValueField>(command.getActor()), actor);
    std::vector<std::string> args;
    ReadFieldUpdate(TypeList<decltype(args)>(), context, Make<ValueField>(command.getArgNames()), args);

    auto rpc_command = MakeUnique<CRPCCommand>(
        command.getCategory(), command.getName(), std::move(actor), std::move(args), command.getUniqueId());
    auto handler = context.proxy_server.m_impl->handleRpc(*rpc_command);
    handler->addCloseHook(MakeUnique<Deleter<decltype(rpc_command)>>(std::move(rpc_command)));
    return handler;
}

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
        RPCTimerBase* result;
        m_loop.sync([&] { result = new RpcTimer(m_loop, fn, millis); });
        return result;
    }
    EventLoop& m_loop;
};

ProxyServerCustom<messages::ChainClient, ChainClient>::ProxyServerCustom(ChainClient* impl,
    bool owned,
    EventLoop& loop)
    : ProxyServerBase(impl, owned, loop), m_timer_interface(MakeUnique<RpcTimerInterface>(*m_loop))
{
    RPCSetTimerInterface(m_timer_interface.get());
}

ProxyServerCustom<messages::ChainClient, ChainClient>::~ProxyServerCustom()
{
    if (m_scheduler) {
        m_scheduler->stop();
        m_result.wait();
        m_scheduler.reset();
    }
    if (m_timer_interface) {
        RPCUnsetTimerInterface(m_timer_interface.get());
        m_timer_interface.reset();
    }
}


void ProxyServerMethodTraits<messages::ChainClient::StartParams>::invoke(Context& context)
{
    if (!context.proxy_server.m_scheduler) {
        context.proxy_server.m_scheduler = MakeUnique<CScheduler>();
        CScheduler* scheduler = context.proxy_server.m_scheduler.get();
        context.proxy_server.m_result = std::async([scheduler]() {
            RenameThread("schedqueue");
            scheduler->serviceQueue();
        });
    }
    context.proxy_server.m_impl->start(*context.proxy_server.m_scheduler);
}

void ProxyClientCustom<messages::ChainNotifications, Chain::Notifications>::TransactionAddedToMempool(
    const CTransactionRef& tx)
{
    self().transactionAddedToMempool(tx);
}
void ProxyClientCustom<messages::ChainNotifications, Chain::Notifications>::TransactionRemovedFromMempool(
    const CTransactionRef& ptx)
{
    self().transactionRemovedFromMempool(ptx);
}
void ProxyClientCustom<messages::ChainNotifications, Chain::Notifications>::BlockConnected(const CBlock& block,
    const uint256& block_hash,
    const std::vector<CTransactionRef>& tx_conflicted)
{
    self().blockConnected(block, block_hash, tx_conflicted);
}
void ProxyClientCustom<messages::ChainNotifications, Chain::Notifications>::BlockDisconnected(const CBlock& block)
{
    self().blockDisconnected(block);
}
void ProxyClientCustom<messages::ChainNotifications, Chain::Notifications>::ChainStateFlushed(
    const CBlockLocator& locator)
{
    self().chainStateFlushed(locator);
}
void ProxyClientCustom<messages::ChainNotifications, Chain::Notifications>::Inventory(const uint256& hash)
{
    self().inventory(hash);
}
void ProxyClientCustom<messages::ChainNotifications, Chain::Notifications>::ResendWalletTransactions(
    int64_t best_block_time)
{
    self().resendWalletTransactions(best_block_time);
}

void ProxyServerMethodTraits<messages::Node::RpcSetTimerInterfaceIfUnsetParams>::invoke(Context& context)
{
    if (!context.proxy_server.m_timer_interface) {
        auto timer = MakeUnique<RpcTimerInterface>(*context.proxy_server.m_loop);
        context.proxy_server.m_timer_interface = std::move(timer);
    }
    context.proxy_server.m_impl->rpcSetTimerInterfaceIfUnset(context.proxy_server.m_timer_interface.get());
}

void ProxyServerMethodTraits<messages::Node::RpcUnsetTimerInterfaceParams>::invoke(Context& context)
{
    context.proxy_server.m_impl->rpcUnsetTimerInterface(context.proxy_server.m_timer_interface.get());
    context.proxy_server.m_timer_interface.reset();
}

bool ProxyClientCustom<messages::Node, Node>::parseParameters(int argc, const char* const argv[], std::string& error)
{
    gArgs.ParseParameters(argc, argv, error);
    return self().customParseParameters(argc, argv, error);
}

bool ProxyClientCustom<messages::Node, Node>::softSetArg(const std::string& arg, const std::string& value)
{
    gArgs.SoftSetArg(arg, value);
    return self().customSoftSetArg(arg, value);
}

bool ProxyClientCustom<messages::Node, Node>::softSetBoolArg(const std::string& arg, bool value)
{
    gArgs.SoftSetBoolArg(arg, value);
    return self().customSoftSetBoolArg(arg, value);
}

bool ProxyClientCustom<messages::Node, Node>::readConfigFiles(std::string& error)
{
    gArgs.ReadConfigFiles(error);
    return self().customReadConfigFiles(error);
}

void ProxyClientCustom<messages::Node, Node>::selectParams(const std::string& network)
{
    SelectParams(network);
    self().customSelectParams(network);
}

const CTransaction& ProxyClientCustom<messages::PendingWalletTx, PendingWalletTx>::get()
{
    if (!m_tx) {
        m_tx = self().customGet();
    }
    return *m_tx;
}

} // namespace capnp
} // namespace interfaces
