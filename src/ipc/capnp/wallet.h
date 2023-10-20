// Copyright (c) 2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_IPC_CAPNP_WALLET_H
#define BITCOIN_IPC_CAPNP_WALLET_H

#include <interfaces/wallet.h>
#include <ipc/capnp/chain.capnp.proxy.h>
#include <ipc/capnp/wallet.capnp.h>
#include <mp/proxy.h>

#include <future>
#include <memory>

class CScheduler;
namespace interfaces {
class WalletLoader;
} // namespace interfaces
namespace ipc {
namespace capnp {
namespace messages {
struct WalletLoader;
} // namespace messages
} // namespace capnp
} // namespace ipc

//! Specialization of WalletLoader proxy server needed hold a CSCheduler instance.
template <>
struct mp::ProxyServerCustom<ipc::capnp::messages::WalletLoader, interfaces::WalletLoader>
    : public mp::ProxyServerBase<ipc::capnp::messages::WalletLoader, interfaces::WalletLoader>
{
public:
    using ProxyServerBase::ProxyServerBase;
    void invokeDestroy();

    std::unique_ptr<CScheduler> m_scheduler;
    std::future<void> m_result;
};

#endif // BITCOIN_IPC_CAPNP_WALLET_H
