#include <ipc/capnp/server.h>

#include <coins.h>
#include <ipc/capnp/types.h>
#include <ipc/capnp/util.h>
#include <ipc/interfaces.h>
#include <ipc/util.h>
#include <key.h>
#include <net_processing.h>
#include <netbase.h>
#include <pubkey.h>
#include <rpc/server.h>

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif
#ifdef ENABLE_WALLET
#include <wallet/coincontrol.h>
#endif

#include <capnp/rpc-twoparty.h>
#include <ipc/capnp/messages.capnp.proxy.h>

namespace ipc {
namespace capnp {

#ifdef ENABLE_WALLET
#define CHECK_WALLET(x) x
#else
#define CHECK_WALLET(x) throw std::logic_error("Wallet function called in non-wallet build.")
#endif

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
        return new RpcTimer(m_loop, fn, millis);
    }
    EventLoop& m_loop;
};

kj::Promise<void> NodeServerBase::localRpcSetTimerInterfaceIfUnset(Node& node, RpcSetTimerInterfaceIfUnsetContext context)
{
    if (!m_timer_interface) {
        auto timer = MakeUnique<RpcTimerInterface>(m_loop);
        m_timer_interface = std::move(timer);
    }
    node.rpcSetTimerInterfaceIfUnset(m_timer_interface.get());
    return kj::READY_NOW;
}

kj::Promise<void> NodeServerBase::localRpcUnsetTimerInterface(Node& node, RpcUnsetTimerInterfaceContext context)
{
    node.rpcUnsetTimerInterface(m_timer_interface.get());
    m_timer_interface.reset();
    return kj::READY_NOW;
}

void StartNodeServer(std::unique_ptr<ipc::Node> node, int fd)
{
    EventLoop loop;
    auto stream = loop.m_io_context.lowLevelProvider->wrapSocketFd(fd, kj::LowLevelAsyncIoProvider::TAKE_OWNERSHIP);
    ::capnp::TwoPartyVatNetwork network(*stream, ::capnp::rpc::twoparty::Side::SERVER, ::capnp::ReaderOptions());
    loop.m_task_set.add(network.onDisconnect().then([&loop]() { loop.shutdown(); }));
    // using S = NodeServer;
    using S = typename Proxy<messages::Node>::Server;
    auto rpc_server = ::capnp::makeRpcServer(network, ::capnp::Capability::Client(kj::heap<S>(std::move(node), loop)));
    loop.loop();
}

} // namespace capnp

} // namespace ipc
