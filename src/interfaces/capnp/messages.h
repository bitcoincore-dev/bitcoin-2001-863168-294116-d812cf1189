#ifndef BITCOIN_INTERFACES_CAPNP_MESSAGES_DECL_H
#define BITCOIN_INTERFACES_CAPNP_MESSAGES_DECL_H

#include <interfaces/capnp/proxy.h>
#include <interfaces/capnp/messages.capnp.h>
#include <interfaces/handler.h>
#include <interfaces/init.h>
#include <interfaces/node.h>
#include <interfaces/wallet.h>
#include <rpc/server.h>

#include <future>

struct EstimatorBucket;
struct EstimationResult;

namespace interfaces {
namespace capnp {

template <>
class ProxyServerCustom<messages::Init, Init> : public ProxyServerBase<messages::Init, Init>
{
public:
    using ProxyServerBase::ProxyServerBase;
    void invokeMethod(MakeWalletClientContext context);
};

using Context = std::pair<int32_t, int32_t>;

template <>
class ProxyServerCustom<messages::Chain, Chain> : public ProxyServerBase<messages::Chain, Chain>
{
public:
    using ProxyServerBase::ProxyServerBase;
    void invokeMethod(HandleNotificationsContext context);
    void invokeMethod(HandleRpcContext context);

    template <typename Callback>
    bool async(Callback&& callback)
    {
        m_lock_thread.post(std::move(callback));
        return true;
    }

    AsyncThread m_lock_thread;
};

template <>
class ProxyServerCustom<messages::ChainLock, Chain::Lock> : public ProxyServerBase<messages::ChainLock, Chain::Lock>
{
public:
    using ChainProxy = ProxyServerCustom<messages::Chain, Chain>;

    ProxyServerCustom(std::unique_ptr<Chain::Lock> impl, ChainProxy& chain_proxy)
        : ProxyServerBase(impl.release(), true /* owned */, *chain_proxy.m_loop), m_chain_proxy(chain_proxy)
    {
    }

    ~ProxyServerCustom()
    {
        m_chain_proxy.m_lock_thread.sync([&]() {
            delete m_impl;
            m_impl = nullptr;
        });
        m_chain_proxy.m_lock_thread.unblock();
    }

    ChainProxy& m_chain_proxy;
};

template <>
class ProxyServerCustom<messages::ChainClient, Chain::Client>
    : public ProxyServerBase<messages::ChainClient, Chain::Client>
{
public:
    using ProxyServerBase::ProxyServerBase;
    void invokeMethod(StartContext context);

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
    void SetBestChain(const CBlockLocator& locator) override;
    void Inventory(const uint256& hash) override;
    void ResendWalletTransactions(int64_t best_block_time) override;
};

template <>
class ProxyServerCustom<messages::Node, Node> : public ProxyServerBase<messages::Node, Node>
{
public:
    using ProxyServerBase::ProxyServerBase;
    void invokeMethod(RpcSetTimerInterfaceIfUnsetContext context);
    void invokeMethod(RpcUnsetTimerInterfaceContext context);
    std::unique_ptr<::RPCTimerInterface> m_timer_interface;
};

template <>
class ProxyClientCustom<messages::Node, Node> : public ProxyClientBase<messages::Node, Node>
{
public:
    using ProxyClientBase::ProxyClientBase;
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
    using ArgsManager::m_override_args;
    using ArgsManager::m_config_args;
    using ArgsManager::m_network;
    using ArgsManager::m_network_only_args;
};

template <typename Proxy>
messages::ChainLock::Client BuildPrimitive(Proxy& proxy,
    std::unique_ptr<Chain::Lock>&& impl,
    TypeList<messages::ChainLock::Client>)
{
    return kj::heap<ProxyServer<messages::ChainLock>>(std::move(impl), proxy);
}

} // namespace capnp
} // namespace interfaces

#endif // BITCOIN_INTERFACES_CAPNP_MESSAGES_DECL_H
