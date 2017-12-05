#include <init.h>
#include <interfaces/capnp/chain-inl.h>
#include <interfaces/capnp/proxy-inl.h>

namespace interfaces {
namespace capnp {

void ProxyServerMethodTraits<messages::Node::RpcSetTimerInterfaceIfUnsetParams>::invoke(Context& context)
{
    if (!context.proxy_server.m_timer_interface) {
        auto timer = MakeTimer(context.proxy_server.m_connection->m_loop);
        context.proxy_server.m_timer_interface = std::move(timer);
    }
    context.proxy_server.m_impl->rpcSetTimerInterfaceIfUnset(context.proxy_server.m_timer_interface.get());
}

void ProxyServerMethodTraits<messages::Node::RpcUnsetTimerInterfaceParams>::invoke(Context& context)
{
    context.proxy_server.m_impl->rpcUnsetTimerInterface(context.proxy_server.m_timer_interface.get());
    context.proxy_server.m_timer_interface.reset();
}

void ProxyClientCustom<messages::Node, Node>::setupServerArgs()
{
    SetupServerArgs();
    self().customSetupServerArgs();
}

bool ProxyClientCustom<messages::Node, Node>::parseParameters(int argc, const char* const argv[], std::string& error)
{
    return gArgs.ParseParameters(argc, argv, error) & self().customParseParameters(argc, argv, error);
}

bool ProxyClientCustom<messages::Node, Node>::softSetArg(const std::string& arg, const std::string& value)
{
    gArgs.SoftSetArg(arg, value);
    return self().customSoftSetArg(arg, value);
}

bool ProxyClientCustom<messages::Node, Node>::softSetBoolArg(const std::string& arg, bool value)
{
    gArgs.SoftSetBoolArg(arg, value);
    return self().customSoftSetBoolArg(arg, value);
}

bool ProxyClientCustom<messages::Node, Node>::readConfigFiles(std::string& error)
{
    return gArgs.ReadConfigFiles(error) & self().customReadConfigFiles(error);
}

void ProxyClientCustom<messages::Node, Node>::selectParams(const std::string& network)
{
    SelectParams(network);
    self().customSelectParams(network);
}

bool ProxyClientCustom<messages::Node, Node>::baseInitialize()
{
    // TODO in future PR: Refactor bitcoin startup code, dedup this with AppInit.
    SelectParams(GlobalArgsNetwork());
    InitLogging();
    InitParameterInteraction();
    return self().customBaseInitialize();
}

} // namespace capnp
} // namespace interfaces
