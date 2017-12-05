#include <interfaces/capnp/node.h>

#include <chainparams.h>
#include <init.h>
#include <interfaces/capnp/common-types.h>
#include <interfaces/capnp/common.h>
#include <interfaces/capnp/node-types.h>
#include <interfaces/capnp/node.capnp.h>
#include <interfaces/capnp/node.capnp.proxy-types.h>
#include <interfaces/capnp/node.capnp.proxy.h>
#include <interfaces/capnp/proxy-io.h>
#include <interfaces/capnp/proxy-types.h>
#include <interfaces/capnp/util.h>
#include <interfaces/node.h>
#include <rpc/server.h>
#include <util/memory.h>
#include <util/system.h>

#include <capnp/list.h>
#include <functional>
#include <kj/async-io.h>
#include <kj/async-prelude.h>
#include <kj/async.h>
#include <kj/memory.h>
#include <kj/time.h>
#include <kj/timer.h>
#include <kj/units.h>
#include <memory>
#include <string>
#include <tuple>
#include <utility>

class CNodeStats;
struct CNodeStateStats;

namespace interfaces {
namespace capnp {

class RpcTimer : public ::RPCTimerBase
{
public:
    RpcTimer(EventLoop& loop, std::function<void(void)>& fn, int64_t millis)
        : m_fn(fn), m_promise(loop.m_io_context.provider->getTimer()
                                  .afterDelay(millis * kj::MILLISECONDS)
                                  .then([this]() { m_fn(); })
                                  .eagerlyEvaluate(nullptr))
    {
    }
    ~RpcTimer() noexcept override {}

    std::function<void(void)> m_fn;
    kj::Promise<void> m_promise;
};

class RpcTimerInterface : public ::RPCTimerInterface
{
public:
    RpcTimerInterface(EventLoop& loop) : m_loop(loop) {}
    const char* Name() override { return "Cap'n Proto"; }
    RPCTimerBase* NewTimer(std::function<void(void)>& fn, int64_t millis) override
    {
        RPCTimerBase* result;
        m_loop.sync([&] { result = new RpcTimer(m_loop, fn, millis); });
        return result;
    }
    EventLoop& m_loop;
};

void ProxyServerMethodTraits<messages::Node::RpcSetTimerInterfaceIfUnsetParams>::invoke(Context& context)
{
    if (!context.proxy_server.m_timer_interface) {
        auto timer = MakeUnique<RpcTimerInterface>(context.proxy_server.m_connection->m_loop);
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

void CustomReadMessage(InvokeContext& invoke_context,
    messages::NodeStats::Reader const& reader,
    std::tuple<CNodeStats, bool, CNodeStateStats>& node_stats)
{
    auto&& node = std::get<0>(node_stats);
    ReadFieldUpdate(TypeList<Decay<decltype(node)>>(), invoke_context, Make<ValueField>(reader), node);
    if ((std::get<1>(node_stats) = reader.hasStateStats())) {
        auto&& state = std::get<2>(node_stats);
        ReadFieldUpdate(
            TypeList<Decay<decltype(state)>>(), invoke_context, Make<ValueField>(reader.getStateStats()), state);
    }
}

} // namespace capnp
} // namespace interfaces
