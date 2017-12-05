#ifndef BITCOIN_INTERFACES_CAPNP_CHAIN_TYPES_H
#define BITCOIN_INTERFACES_CAPNP_CHAIN_TYPES_H

#include <interfaces/capnp/common-types.h>

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

//! Specialization of start method needed to provide CScheduler& reference
//! argument.
template <>
struct ProxyServerMethodTraits<messages::ChainClient::StartParams>
{
    using Context =
        ServerContext<messages::ChainClient, messages::ChainClient::StartParams, messages::ChainClient::StartResults>;
    static void invoke(Context& context);
};

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

//! Chain::Notifications& server-side argument handling. Skips argument so it can
//! be handled by ProxyServerCustom code.
template <typename Accessor, typename ServerContext, typename Fn, typename... Args>
void CustomPassField(TypeList<Chain::Notifications&>, ServerContext& server_context, const Fn& fn, Args&&... args)
{
    fn.invoke(server_context, std::forward<Args>(args)...);
}

//! Chain& server-side argument handling. Skips argument so it can
//! be handled by ProxyServerCustom code.
template <typename Accessor, typename ServerContext, typename Fn, typename... Args>
void CustomPassField(TypeList<Chain&>, ServerContext& server_context, const Fn& fn, Args&&... args)
{
    fn.invoke(server_context, std::forward<Args>(args)...);
}

void CustomBuildMessage(InvokeContext& invoke_context,
    CValidationState const& state,
    messages::ValidationState::Builder&& builder);
void CustomReadMessage(InvokeContext& invoke_context,
    messages::ValidationState::Reader const& reader,
    CValidationState& state);
bool CustomHasValue(InvokeContext& invoke_context, const Coin& coin);

} // namespace capnp
} // namespace interfaces

#endif // BITCOIN_INTERFACES_CAPNP_CHAIN_TYPES_H
