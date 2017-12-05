#ifndef BITCOIN_INTERFACES_CAPNP_MESSAGES_DECL_H
#define BITCOIN_INTERFACES_CAPNP_MESSAGES_DECL_H

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

template <>
class ProxyServerCustom<messages::Init, interfaces::Init> : public ProxyServerBase<messages::Init, interfaces::Init>,
                                                            public ProxyServer<Init>
{
public:
    ProxyServerCustom(interfaces::Init* impl, bool owned, EventLoop& loop)
        : ProxyServerBase(impl, owned, loop), ProxyServer(loop)
    {
    }
    std::unique_ptr<ChainClient> invokeMethod(InvokeContext& invoke_context,
        MakeWalletClientContext method_context,
        std::vector<std::string> wallet_filenames);
};

template <>
class ProxyServerCustom<messages::Chain, Chain> : public ProxyServerBase<messages::Chain, Chain>
{
public:
    using ProxyServerBase::ProxyServerBase;
    std::unique_ptr<Handler> invokeMethod(InvokeContext& invoke_context, HandleNotificationsContext method_context);
    std::unique_ptr<Handler> invokeMethod(InvokeContext& invoke_context, HandleRpcContext method_context);
};

template <>
class ProxyServerCustom<messages::ChainClient, ChainClient>
    : public ProxyServerBase<messages::ChainClient, ChainClient>
{
public:
    using ProxyServerBase::ProxyServerBase;
    ~ProxyServerCustom();
    void invokeMethod(InvokeContext& invoke_context, StartContext method_context);

    std::unique_ptr<CScheduler> m_scheduler;
    std::future<void> m_result;
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
    void invokeMethod(InvokeContext& invoke_context, RpcSetTimerInterfaceIfUnsetContext method_context);
    void invokeMethod(InvokeContext& invoke_context, RpcUnsetTimerInterfaceContext method_context);
    std::unique_ptr<::RPCTimerInterface> m_timer_interface;
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
struct ProxyMethodTraits<interfaces::capnp::messages::PendingWalletTx::CustomGetParams>
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

template <typename ServerContext, typename Accessor, typename Fn, typename... Args>
void PassField(TypeList<::capnp::Void>,
    TypeList<CScheduler&>,
    ServerContext& server_context,
    const Accessor& accessor,
    const Fn& fn,
    Args&&... args)
{
    fn.invoke(server_context, std::forward<Args>(args)...);
}

template <typename ServerContext, typename Accessor, typename Fn, typename... Args>
void PassField(TypeList<messages::RPCCommand>,
    TypeList<const CRPCCommand&>,
    ServerContext& server_context,
    const Accessor& accessor,
    const Fn& fn,
    Args&&... args)
{
    fn.invoke(server_context, std::forward<Args>(args)...);
}

template <typename ServerContext, typename Accessor, typename Fn, typename... Args>
void PassField(TypeList<::capnp::Void>,
    TypeList<RPCTimerInterface*>,
    ServerContext& server_context,
    const Accessor& accessor,
    const Fn& fn,
    Args&&... args)
{
    fn.invoke(server_context, std::forward<Args>(args)...);
}

} // namespace capnp
} // namespace interfaces

#endif // BITCOIN_INTERFACES_CAPNP_MESSAGES_DECL_H
