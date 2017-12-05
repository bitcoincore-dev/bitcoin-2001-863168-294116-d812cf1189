#ifndef BITCOIN_INTERFACES_CAPNP_MESSAGES_H
#define BITCOIN_INTERFACES_CAPNP_MESSAGES_H

#include <interfaces/capnp/proxy.h>
#include <interfaces/capnp/messages.capnp.h>
#include <interfaces/handler.h>
#include <interfaces/init.h>
#include <interfaces/node.h>
#include <interfaces/wallet.h>
#include <rpc/server.h>
#include <scheduler.h>

#include <future>

struct EstimatorBucket;
struct EstimationResult;

namespace interfaces {
namespace capnp {

void BuildGlobalArgs(InvokeContext& invoke_context, messages::GlobalArgs::Builder&& builder);
void ReadGlobalArgs(InvokeContext& invoke_context, const messages::GlobalArgs::Reader& reader);

template <>
struct ProxyServerMethodTraits<messages::Init::MakeWalletClientParams>
{
    using Context =
        ServerContext<messages::Init, messages::Init::MakeWalletClientParams, messages::Init::MakeWalletClientResults>;
    static std::unique_ptr<ChainClient> invoke(Context& context, std::vector<std::string> wallet_filenames);
};

template <>
struct ProxyServerMethodTraits<messages::Chain::HandleNotificationsParams>
{
    using Context = ServerContext<messages::Chain,
        messages::Chain::HandleNotificationsParams,
        messages::Chain::HandleNotificationsResults>;
    static std::unique_ptr<Handler> invoke(Context& context);
};

template <>
struct ProxyServerMethodTraits<messages::Chain::HandleRpcParams>
{
    using Context =
        ServerContext<messages::Chain, messages::Chain::HandleRpcParams, messages::Chain::HandleRpcResults>;
    static std::unique_ptr<Handler> invoke(Context& context);
};

template <>
class ProxyServerCustom<messages::ChainClient, ChainClient>
    : public ProxyServerBase<messages::ChainClient, ChainClient>
{
public:
    ProxyServerCustom(ChainClient* impl, bool owned, EventLoop& loop);
    ~ProxyServerCustom();

    std::unique_ptr<CScheduler> m_scheduler;
    std::unique_ptr<::RPCTimerInterface> m_timer_interface;
    std::future<void> m_result;
};

template <>
struct ProxyServerMethodTraits<messages::ChainClient::StartParams>
{
    using Context =
        ServerContext<messages::ChainClient, messages::ChainClient::StartParams, messages::ChainClient::StartResults>;
    static void invoke(Context& context);
};

// FIXME: this is really terrible. all of this and corresponding code exists solely to replace lowercase capnp method
// names with uppercase validationinterface method names. need a new proxygen annotation that can be used for this
// other than X.name. maybe X.name/rename/alias for this simple renaming and
// X.impl/X.classMethod/X.callsMethod/X.wraps/X.serverName for overload usecase X.name is being used for
template <>
class ProxyClientCustom<messages::ChainNotifications, Chain::Notifications>
    : public ProxyClientBase<messages::ChainNotifications, Chain::Notifications>
{
public:
    using ProxyClientBase::ProxyClientBase;
    void TransactionAddedToMempool(const CTransactionRef& tx) override;
    void TransactionRemovedFromMempool(const CTransactionRef& ptx) override;
    void BlockConnected(const CBlock& block,
        const uint256& block_hash,
        const std::vector<CTransactionRef>& tx_conflicted) override;
    void BlockDisconnected(const CBlock& block) override;
    void ChainStateFlushed(const CBlockLocator& locator) override;
    void Inventory(const uint256& hash) override;
    void ResendWalletTransactions(int64_t best_block_time) override;
};

template <>
class ProxyServerCustom<messages::Node, Node> : public ProxyServerBase<messages::Node, Node>
{
public:
    using ProxyServerBase::ProxyServerBase;
    std::unique_ptr<::RPCTimerInterface> m_timer_interface;
};

template <>
struct ProxyServerMethodTraits<messages::Node::RpcSetTimerInterfaceIfUnsetParams>
{
    using Context = ServerContext<messages::Node,
        messages::Node::RpcSetTimerInterfaceIfUnsetParams,
        messages::Node::RpcSetTimerInterfaceIfUnsetResults>;
    static void invoke(Context& context);
};
template <>
struct ProxyServerMethodTraits<messages::Node::RpcUnsetTimerInterfaceParams>
{
    using Context = ServerContext<messages::Node,
        messages::Node::RpcUnsetTimerInterfaceParams,
        messages::Node::RpcUnsetTimerInterfaceResults>;
    static void invoke(Context& context);
};

template <>
class ProxyClientCustom<messages::Node, Node> : public ProxyClientBase<messages::Node, Node>
{
public:
    using ProxyClientBase::ProxyClientBase;
    bool parseParameters(int argc, const char* const argv[], std::string& error) override;
    bool softSetArg(const std::string& arg, const std::string& value) override;
    bool softSetBoolArg(const std::string& arg, bool value) override;
    bool readConfigFiles(std::string& error) override;
    void selectParams(const std::string& network) override;
};

template <>
class ProxyClientCustom<messages::PendingWalletTx, PendingWalletTx>
    : public ProxyClientBase<messages::PendingWalletTx, PendingWalletTx>
{
public:
    using ProxyClientBase::ProxyClientBase;
    const CTransaction& get() override;

private:
    CTransactionRef m_tx;
};

template <>
struct ProxyClientMethodTraits<messages::PendingWalletTx::CustomGetParams>
    : public FunctionTraits<CTransactionRef (PendingWalletTx::*const)()>
{
};

struct GlobalArgs : public ArgsManager
{
    using ArgsManager::cs_args;
    using ArgsManager::m_config_args;
    using ArgsManager::m_network;
    using ArgsManager::m_network_only_args;
    using ArgsManager::m_override_args;
};

template <typename Accessor, typename ServerContext, typename Fn, typename... Args>
void PassField(TypeList<CScheduler&>, ServerContext& server_context, const Fn& fn, Args&&... args)
{
    fn.invoke(server_context, std::forward<Args>(args)...);
}

template <typename Accessor, typename ServerContext, typename Fn, typename... Args>
void PassField(TypeList<const CRPCCommand&>, ServerContext& server_context, const Fn& fn, Args&&... args)
{
    fn.invoke(server_context, std::forward<Args>(args)...);
}

template <typename Accessor, typename ServerContext, typename Fn, typename... Args>
void PassField(TypeList<RPCTimerInterface*>, ServerContext& server_context, const Fn& fn, Args&&... args)
{
    fn.invoke(server_context, std::forward<Args>(args)...);
}

} // namespace capnp
} // namespace interfaces

#endif // BITCOIN_INTERFACES_CAPNP_MESSAGES_H
