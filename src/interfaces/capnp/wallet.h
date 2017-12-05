#ifndef BITCOIN_INTERFACES_CAPNP_WALLET_H
#define BITCOIN_INTERFACES_CAPNP_WALLET_H

#include <interfaces/capnp/proxy.h>
#include <interfaces/capnp/wallet.capnp.h>
#include <interfaces/chain.h>
#include <interfaces/handler.h>
#include <interfaces/init.h>
#include <interfaces/node.h>
#include <interfaces/wallet.h>
#include <rpc/server.h>
#include <scheduler.h>

#include <future>

namespace interfaces {
namespace capnp {

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

} // namespace capnp
} // namespace interfaces

#endif // BITCOIN_INTERFACES_CAPNP_WALLET_H
