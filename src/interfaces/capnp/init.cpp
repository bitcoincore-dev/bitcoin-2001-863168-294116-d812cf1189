#include <interfaces/capnp/init.h>


#include <bitcoin-config.h>
#include <capnp/rpc-twoparty.h>
#include <chainparams.h>
#include <coins.h>
#include <future>
#include <interfaces/capnp/proxy.h>
#include <interfaces/capnp/proxy-impl.h>
#include <interfaces/capnp/messages.capnp.h>
#include <interfaces/capnp/messages.capnp.proxy.h>
#include <interfaces/node.h>
#include <key.h>
#include <kj/async-unix.h>
#include <kj/debug.h>
#include <mutex>
#include <net_processing.h>
#include <netbase.h>
#include <pubkey.h>
#include <rpc/server.h>
#include <thread>
#include <univalue.h>
#include <wallet/wallet.h>

namespace interfaces {
namespace capnp {

//! VatId for server side of IPC connection.
struct ServerVatId
{
    ::capnp::word scratch[4]{};
    ::capnp::MallocMessageBuilder message{scratch};
    ::capnp::rpc::twoparty::VatId::Builder vat_id{message.getRoot<::capnp::rpc::twoparty::VatId>()};
    ServerVatId() { vat_id.setSide(::capnp::rpc::twoparty::Side::SERVER); }
};

class ShutdownLoop : public CloseHook
{
public:
    ShutdownLoop(EventLoop& loop) : m_loop(loop) {}
    void onClose(Base& interface, bool remote) override
    {
        // FIXME: Shutdown can cause segfault right now because close/destuction
        // seuqnce doesn't wait for server objects across the pipe to shut down,
        // so e.g. things like handlers dont get a chance to get deleted in
        // right sequence. Instead with this shutdown they get deleted in
        // EventLoop destructor which can cause segfault on
        // UnregisterValidationInterface.
        // if (!remote) m_loop.shutdown();
    }
    EventLoop& m_loop;
};

std::unique_ptr<interfaces::Init> Connect(const char* exe_name, int fd)
{
    std::promise<std::unique_ptr<ProxyClient<messages::Init>>> init_promise;
    std::thread thread([&]() {
        RenameThread("capnp-connect");
        EventLoop loop(exe_name, std::move(thread));
        auto connection = MakeUnique<Connection>(loop, fd);
        // FIXME: Add connection->m_network.onDisconnect handler
        auto client = connection->m_rpc_client.bootstrap(ServerVatId().vat_id).castAs<messages::Init>();
        auto proxy = MakeUnique<ProxyClient<messages::Init>>(kj::mv(client), loop);
        loop.m_init = &proxy->m_client;
        proxy->addCloseHook(MakeUnique<ShutdownLoop>(loop));
        init_promise.set_value(std::move(proxy));
        loop.loop();
    });
    return init_promise.get_future().get();
}

void Listen(const char* exe_name, int fd, interfaces::Init& init)
{
    RenameThread("capnp-listen");
    EventLoop loop(exe_name);
    auto stream = loop.m_io_context.lowLevelProvider->wrapSocketFd(fd, kj::LowLevelAsyncIoProvider::TAKE_OWNERSHIP);
    ::capnp::TwoPartyVatNetwork network(*stream, ::capnp::rpc::twoparty::Side::SERVER, ::capnp::ReaderOptions());
    loop.m_task_set.add(network.onDisconnect().then([&loop]() { loop.shutdown(); }));
    auto server = kj::heap<ProxyServer<messages::Init>>(&init, false /* owned */, loop);
    auto client = ::capnp::Capability::Client(kj::mv(server));
    auto rpc_server = ::capnp::makeRpcServer(network, kj::mv(client));
    loop.loop();
}

} // namespace capnp
} // namespace interfaces
