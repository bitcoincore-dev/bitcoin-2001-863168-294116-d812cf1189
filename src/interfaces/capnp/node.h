#ifndef BITCOIN_INTERFACES_CAPNP_NODE_H
#define BITCOIN_INTERFACES_CAPNP_NODE_H

#include <interfaces/capnp/config.h>
#include <interfaces/capnp/node.capnp.h>
#include <interfaces/capnp/proxy.h>
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


//! Specialization of Node proxy server needed to add m_timer_interface
//! member used by rpcSetTimerInterfaceIfUnset and rpcUnsetTimerInterface
//! methods.
template <>
struct ProxyServerCustom<messages::Node, Node> : public ProxyServerBase<messages::Node, Node>
{
public:
    using ProxyServerBase::ProxyServerBase;
    std::unique_ptr<::RPCTimerInterface> m_timer_interface;
};

//! Specialization of pcSetTimerInterfaceIfUnset needed because it takes a
//! RPCTimerInterface* argument, which requires custom code to provide a
//! compatible timer.
template <>
struct ProxyServerMethodTraits<messages::Node::RpcSetTimerInterfaceIfUnsetParams>
{
    using Context = ServerContext<messages::Node,
        messages::Node::RpcSetTimerInterfaceIfUnsetParams,
        messages::Node::RpcSetTimerInterfaceIfUnsetResults>;
    static void invoke(Context& context);
};

//! Specialization of rpcUnsetTimerInterface needed because it takes a
//! RPCTimerInterface* argument, which requires custom code to provide a
//! compatible timer.
template <>
struct ProxyServerMethodTraits<messages::Node::RpcUnsetTimerInterfaceParams>
{
    using Context = ServerContext<messages::Node,
        messages::Node::RpcUnsetTimerInterfaceParams,
        messages::Node::RpcUnsetTimerInterfaceResults>;
    static void invoke(Context& context);
};

//! Specialization of Node client to deal with argument & config handling across
//! processes. If node and node client are running in same process it's
//! sufficient to only call parseParameters, softSetArgs, etc methods only on the
//! node object, but if node is running in a different process, the calls need to
//! be made repeated locally as well to update the state of the client process..
template <>
class ProxyClientCustom<messages::Node, Node> : public ProxyClientBase<messages::Node, Node>
{
public:
    using ProxyClientBase::ProxyClientBase;
    void setupServerArgs() override;
    bool parseParameters(int argc, const char* const argv[], std::string& error) override;
    bool softSetArg(const std::string& arg, const std::string& value) override;
    bool softSetBoolArg(const std::string& arg, bool value) override;
    bool readConfigFiles(std::string& error) override;
    void selectParams(const std::string& network) override;
    bool baseInitialize() override;
};

template <>
inline std::unique_ptr<Node> CustomMakeProxyClient<messages::Node, Node>(InvokeContext& context,
    messages::Node::Client&& client)
{
    return g_config.make_node_client(context, kj::mv(client));
}

template <>
inline kj::Own<messages::Node::Server> CustomMakeProxyServer<messages::Node, Node>(InvokeContext& context,
    std::unique_ptr<Node>&& impl)
{
    return g_config.make_node_server(context, std::move(impl));
}

} // namespace capnp
} // namespace interfaces

#endif // BITCOIN_INTERFACES_CAPNP_NODE_H
