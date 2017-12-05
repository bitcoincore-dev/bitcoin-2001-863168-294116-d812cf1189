#ifndef BITCOIN_INTERFACE_CAPNP_MESSAGES_DECL_H
#define BITCOIN_INTERFACE_CAPNP_MESSAGES_DECL_H

#include <interface/capnp/proxy.h>
#include <interface/capnp/messages.capnp.h>
#include <interface/handler.h>
#include <interface/init.h>
#include <interface/node.h>
#include <interface/wallet.h>
#include <rpc/server.h>

#include <future>

struct EstimatorBucket;
struct EstimationResult;

namespace interface {
namespace capnp {

template <>
class ProxyServerCustom<messages::Init, Init> : public ProxyServerBase<messages::Init, Init>
{
public:
    using ProxyServerBase<messages::Init, Init>::ProxyServerBase;
    using ProxyServerBase<messages::Init, Init>::invokeMethod;

    template <typename Method, typename... Fields>
    kj::Promise<void> invokeMethod(MakeWalletClientContext context, const Method&, const Fields&...)
    {
        return invokeMethod(kj::mv(context));
    }

    kj::Promise<void> invokeMethod(MakeWalletClientContext context);
};

template <>
class ProxyServerCustom<messages::Chain, Chain> : public ProxyServerBase<messages::Chain, Chain>
{
public:
    using ProxyServerBase<messages::Chain, Chain>::ProxyServerBase;
    using ProxyServerBase<messages::Chain, Chain>::invokeMethod;
    template <typename Method, typename... Fields>
    kj::Promise<void> invokeMethod(HandleNotificationsContext context, const Method&, const Fields&...)
    {
        return invokeMethod(kj::mv(context));
    }
    template <typename Method, typename... Fields>
    kj::Promise<void> invokeMethod(HandleRpcContext context, const Method&, const Fields&...)
    {
        return invokeMethod(kj::mv(context));
    }
    kj::Promise<void> invokeMethod(HandleNotificationsContext context);
    kj::Promise<void> invokeMethod(HandleRpcContext context);
};

template <>
class ProxyServerCustom<messages::ChainClient, Chain::Client>
    : public ProxyServerBase<messages::ChainClient, Chain::Client>
{
public:
    using ProxyServerBase<messages::ChainClient, Chain::Client>::ProxyServerBase;
    using ProxyServerBase<messages::ChainClient, Chain::Client>::invokeMethodAsync;
    template <typename Method, typename... Fields>
    kj::Promise<void> invokeMethodAsync(StartContext context, const Method&, const Fields&...)
    {
        return invokeMethodAsync(kj::mv(context));
    }
    kj::Promise<void> invokeMethodAsync(StartContext context);
    std::unique_ptr<CScheduler> m_scheduler;
    std::future<void> m_result;
};

// FIXME: this is really terrible. all of this and corresponding code exists solely to replace lowercase capnp method names with uppercase validationinterface method names. need a new proxygen annotation that can be used for this other than X.name. maybe X.name/rename/alias for this simple renaming and X.impl/X.classMethod/X.callsMethod/X.wraps/X.serverName for overload usecase X.name is being used for
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
    void SetBestChain(const CBlockLocator& locator) override;
    void Inventory(const uint256& hash) override;
    void ResendWalletTransactions(int64_t best_block_time) override;
};

template <>
class ProxyServerCustom<messages::Node, Node> : public ProxyServerBase<messages::Node, Node>
{
public:
    using ProxyServerBase<messages::Node, Node>::ProxyServerBase;
    using ProxyServerBase<messages::Node, Node>::invokeMethod;
    template <typename Method, typename... Fields>
    kj::Promise<void> invokeMethod(RpcSetTimerInterfaceIfUnsetContext context, const Method&, const Fields&...)
    {
        return invokeMethod(kj::mv(context));
    }
    template <typename Method, typename... Fields>
    kj::Promise<void> invokeMethod(RpcUnsetTimerInterfaceContext context, const Method&, const Fields&...)
    {
        return invokeMethod(kj::mv(context));
    }
    kj::Promise<void> invokeMethod(RpcSetTimerInterfaceIfUnsetContext context);
    kj::Promise<void> invokeMethod(RpcUnsetTimerInterfaceContext context);
    std::unique_ptr<::RPCTimerInterface> m_timer_interface;
};

template <>
class ProxyClientCustom<messages::Node, Node> : public ProxyClientBase<messages::Node, Node>
{
public:
    using ProxyClientBase<messages::Node, Node>::ProxyClientBase;
    void parseParameters(int argc, const char* const argv[]) override;
    bool softSetArg(const std::string& arg, const std::string& value) override;
    bool softSetBoolArg(const std::string& arg, bool value) override;
    void readConfigFile(const std::string& conf_path) override;
    void selectParams(const std::string& network) override;
};

template <>
class ProxyClientCustom<messages::PendingWalletTx, PendingWalletTx>
    : public ProxyClientBase<messages::PendingWalletTx, PendingWalletTx>
{
public:
    using ProxyClientBase<messages::PendingWalletTx, PendingWalletTx>::ProxyClientBase;
    const CTransaction& get() override;

private:
    CTransactionRef m_tx;
};

template <>
struct ProxyMethodTraits<interface::capnp::messages::PendingWalletTx::CustomGetParams>
    : public FunctionTraits<CTransactionRef (PendingWalletTx::*const)()>
{
};

} // namespace capnp
} // namespace interface

#endif // BITCOIN_INTERFACE_CAPNP_MESSAGES_DECL_H
