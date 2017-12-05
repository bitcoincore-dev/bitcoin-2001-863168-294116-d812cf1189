#ifndef BITCOIN_INTERFACES_CAPNP_WALLET_H
#define BITCOIN_INTERFACES_CAPNP_WALLET_H

#include <interfaces/capnp/wallet.capnp.h>
#include <interfaces/wallet.h>
#include <mp/proxy.h>

//! Specialization of PendingWalletTx client to manage memory of CTransaction&
//! reference returned by get().
template <>
class mp::ProxyClientCustom<interfaces::capnp::messages::PendingWalletTx, interfaces::PendingWalletTx>
    : public mp::ProxyClientBase<interfaces::capnp::messages::PendingWalletTx, interfaces::PendingWalletTx>
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
struct mp::ProxyClientMethodTraits<interfaces::capnp::messages::PendingWalletTx::CustomGetParams>
    : public FunctionTraits<CTransactionRef (interfaces::PendingWalletTx::*const)()>
{
};

#endif // BITCOIN_INTERFACES_CAPNP_WALLET_H
