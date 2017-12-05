#ifndef BITCOIN_INTERFACES_CAPNP_COMMON_H
#define BITCOIN_INTERFACES_CAPNP_COMMON_H

#include <interfaces/capnp/common.capnp.h>
#include <interfaces/capnp/proxy.h>
#include <memory>
#include <util/system.h>

class RPCTimerInterface;

namespace interfaces {
namespace capnp {

class EventLoop;

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

//! GlobalArgs network string accessor.
std::string GlobalArgsNetwork();

//! RPC timer adapter.
std::unique_ptr<::RPCTimerInterface> MakeTimer(EventLoop& loop);

} // namespace capnp
} // namespace interfaces

#endif // BITCOIN_INTERFACES_CAPNP_COMMON_H
