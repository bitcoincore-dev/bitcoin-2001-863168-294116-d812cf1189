#ifndef BITCOIN_INTERFACES_CAPNP_MESSAGES_H
#define BITCOIN_INTERFACES_CAPNP_MESSAGES_H

#include <interfaces/capnp/messages.capnp.h>
#include <interfaces/capnp/proxy.h>
#include <interfaces/chain.h>
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

//! Specialization of makeWalletClient needed because it takes a Chain& reference
//! argument, not a unique_ptr<Chain> argument, so a manual addCloseHook()
//! callback is needed to clean up the ProxyServer<messages::Chain> proxy object.
template <>
struct ProxyServerMethodTraits<messages::Init::MakeWalletClientParams>
{
    using Context =
        ServerContext<messages::Init, messages::Init::MakeWalletClientParams, messages::Init::MakeWalletClientResults>;
    static std::unique_ptr<ChainClient> invoke(Context& context, std::vector<std::string> wallet_filenames);
};

//! Specialization of handleNotifications needed because it takes a
//! Notifications& reference argument, not a unique_ptr<Notifications> argument,
//! so a manual addCloseHook() callback is needed to clean up the
//! ProxyServer<messages::ChainNotifications> proxy object.
template <>
struct ProxyServerMethodTraits<messages::Chain::HandleNotificationsParams>
{
    using Context = ServerContext<messages::Chain,
        messages::Chain::HandleNotificationsParams,
        messages::Chain::HandleNotificationsResults>;
    static std::unique_ptr<Handler> invoke(Context& context);
};

//! Specialization of handleRpc needed because it takes a CRPCCommand& reference
//! argument so a manual addCloseHook() callback is needed to free the passed
//! CRPCCommand struct and proxy ActorCallback object.
template <>
struct ProxyServerMethodTraits<messages::Chain::HandleRpcParams>
{
    using Context =
        ServerContext<messages::Chain, messages::Chain::HandleRpcParams, messages::Chain::HandleRpcResults>;
    static std::unique_ptr<Handler> invoke(Context& context);
};

//! Specialization of ChainClient proxy server needed to call
//! RPCSetTimerInterface and RPCUnsetTimerInterface and provide CSCheduler and
//! RPCTimerInterface implementations.
template <>
struct ProxyServerCustom<messages::ChainClient, ChainClient>
    : public ProxyServerBase<messages::ChainClient, ChainClient>
{
public:
    ProxyServerCustom(ChainClient* impl, bool owned, EventLoop& loop);
    ~ProxyServerCustom();

    std::unique_ptr<CScheduler> m_scheduler;
    std::unique_ptr<::RPCTimerInterface> m_timer_interface;
    std::future<void> m_result;
};

//! Specialization of start method needed to provide CScheduler& reference
//! argument.
template <>
struct ProxyServerMethodTraits<messages::ChainClient::StartParams>
{
    using Context =
        ServerContext<messages::ChainClient, messages::ChainClient::StartParams, messages::ChainClient::StartResults>;
    static void invoke(Context& context);
};

//! Specialization of ChainNotifications client to deal with different
//! capitalization of method names. Cap'n Proto requires all method names to be
//! lowercase, so this forwards the calls.
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
    void ResendWalletTransactions(int64_t best_block_time) override;
};

//! Specialization of ChainClient proxy server needed to add m_timer_interface
//! member used by rpcSetTimerInterfaceIfUnset and rpcUnsetTimerInterface
//! methods.
template <>
struct ProxyServerCustom<messages::Node, Node> : public ProxyServerBase<messages::Node, Node>
{
public:
    using ProxyServerBase::ProxyServerBase;
    std::unique_ptr<::RPCTimerInterface> m_timer_interface;
};

//! Specialization of pcSetTimerInterfaceIfUnset needed because it takes a
//! RPCTimerInterface* argument, which requires custom code to provide a
//! compatible timer.
template <>
struct ProxyServerMethodTraits<messages::Node::RpcSetTimerInterfaceIfUnsetParams>
{
    using Context = ServerContext<messages::Node,
        messages::Node::RpcSetTimerInterfaceIfUnsetParams,
        messages::Node::RpcSetTimerInterfaceIfUnsetResults>;
    static void invoke(Context& context);
};

//! Specialization of rpcUnsetTimerInterface needed because it takes a
//! RPCTimerInterface* argument, which requires custom code to provide a
//! compatible timer.
template <>
struct ProxyServerMethodTraits<messages::Node::RpcUnsetTimerInterfaceParams>
{
    using Context = ServerContext<messages::Node,
        messages::Node::RpcUnsetTimerInterfaceParams,
        messages::Node::RpcUnsetTimerInterfaceResults>;
    static void invoke(Context& context);
};

//! Specialization of Node client to deal with argument & config handling across
//! processes. If node and node client are running in same process it's
//! sufficient to only call softSetArgs, etc methods only on the node object,
//! but if node is running in a different process, the calls need to be made
//! repeated locally as well to update the state of the client process..
template <>
class ProxyClientCustom<messages::Node, Node> : public ProxyClientBase<messages::Node, Node>
{
public:
    using ProxyClientBase::ProxyClientBase;
    bool softSetArg(const std::string& arg, const std::string& value) override;
    bool softSetBoolArg(const std::string& arg, bool value) override;
    bool readConfigFiles(std::string& error) override;
    void selectParams(const std::string& network) override;
};

//! Specialization of PendingWalletTx client to manage memory of CTransaction&
//! reference returned by get().
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

//! Specialization of PendingWalletTx::get client code to manage memory of
//! CTransaction& reference returned by get().
template <>
struct ProxyClientMethodTraits<messages::PendingWalletTx::CustomGetParams>
    : public FunctionTraits<CTransactionRef (PendingWalletTx::*const)()>
{
};

//! Wrapper around GlobalArgs struct to expose public members.
struct GlobalArgs : public ArgsManager
{
    using ArgsManager::cs_args;
    using ArgsManager::m_config_args;
    using ArgsManager::m_network;
    using ArgsManager::m_network_only_args;
    using ArgsManager::m_override_args;
};

//! GlobalArgs client-side argument handling. Builds message from ::gArgs variable.
void BuildGlobalArgs(InvokeContext& invoke_context, messages::GlobalArgs::Builder&& builder);

//! GlobalArgs server-side argument handling. Reads message into ::gArgs variable.
void ReadGlobalArgs(InvokeContext& invoke_context, const messages::GlobalArgs::Reader& reader);

//! CScheduler& server-side argument handling. Skips argument so it can
//! be handled by ProxyServerCustom code.
template <typename Accessor, typename ServerContext, typename Fn, typename... Args>
void CustomPassField(TypeList<CScheduler&>, ServerContext& server_context, const Fn& fn, Args&&... args)
{
    fn.invoke(server_context, std::forward<Args>(args)...);
}

//! CRPCCommand& server-side argument handling. Skips argument so it can
//! be handled by ProxyServerCustom code.
template <typename Accessor, typename ServerContext, typename Fn, typename... Args>
void CustomPassField(TypeList<const CRPCCommand&>, ServerContext& server_context, const Fn& fn, Args&&... args)
{
    fn.invoke(server_context, std::forward<Args>(args)...);
}

//! RPCTimerInterface* server-side argument handling. Skips argument so it can
//! be handled by ProxyServerCustom code.
template <typename Accessor, typename ServerContext, typename Fn, typename... Args>
void CustomPassField(TypeList<RPCTimerInterface*>, ServerContext& server_context, const Fn& fn, Args&&... args)
{
    fn.invoke(server_context, std::forward<Args>(args)...);
}

} // namespace capnp
} // namespace interfaces

#endif // BITCOIN_INTERFACES_CAPNP_MESSAGES_H
