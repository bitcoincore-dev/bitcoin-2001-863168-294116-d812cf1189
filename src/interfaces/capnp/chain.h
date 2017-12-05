#ifndef BITCOIN_INTERFACES_CAPNP_CHAIN_H
#define BITCOIN_INTERFACES_CAPNP_CHAIN_H

#include <interfaces/capnp/chain.capnp.h>
#include <interfaces/capnp/proxy.h>
#include <interfaces/chain.h>
#include <policy/fees.h>
#include <rpc/server.h>

namespace interfaces {
namespace capnp {

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

//! Specialization of requestMempoolTransactions needed because it takes a
//! Notifications& reference argument, not a unique_ptr<Notifications> argument,
//! so a manual deletion is needed to clean up the
//! ProxyServer<messages::ChainNotifications> proxy object.
template <>
struct ProxyServerMethodTraits<messages::Chain::RequestMempoolTransactionsParams>
{
    using Context = ServerContext<messages::Chain,
        messages::Chain::RequestMempoolTransactionsParams,
        messages::Chain::RequestMempoolTransactionsResults>;
    static void invoke(Context& context);
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

//! Specialization of ChainClient proxy server needed hold a CSCheduler instance.
template <>
struct ProxyServerCustom<messages::ChainClient, ChainClient>
    : public ProxyServerBase<messages::ChainClient, ChainClient>
{
public:
    ProxyServerCustom(ChainClient* impl, bool owned, Connection& connection);
    void invokeDestroy();

    std::unique_ptr<CScheduler> m_scheduler;
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
    void BlockConnected(const CBlock& block, const std::vector<CTransactionRef>& tx_conflicted) override;
    void BlockDisconnected(const CBlock& block) override;
    void ChainStateFlushed(const CBlockLocator& locator) override;
};

} // namespace capnp
} // namespace interfaces

#endif // BITCOIN_INTERFACES_CAPNP_CHAIN_H
