#ifndef BITCOIN_INTERFACES_CAPNP_CHAIN_H
#define BITCOIN_INTERFACES_CAPNP_CHAIN_H

#include <interfaces/capnp/chain.capnp.h>
#include <interfaces/capnp/proxy.h>
#include <interfaces/chain.h>
#include <rpc/server.h>

namespace interfaces {
namespace capnp {

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
