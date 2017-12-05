#include <interface/capnp/init.h>


#include <bitcoin-config.h>
#include <capnp/rpc-twoparty.h>
#include <chainparams.h>
#include <coins.h>
#include <future>
#include <interface/capnp/proxy.h>
#include <interface/capnp/proxy-impl.h>
#include <interface/capnp/messages.capnp.h>
#include <interface/capnp/messages.capnp.proxy.h>
#include <interface/node.h>
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

namespace interface {
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
        if (!remote) m_loop.shutdown();
    }
    EventLoop& m_loop;
};

std::unique_ptr<Init> Connect(int fd)
{
    std::promise<std::unique_ptr<ProxyClient<messages::Init>>> init_promise;
    std::thread thread([&]() {
        EventLoop loop(std::move(thread));
        auto connection = MakeUnique<Connection>(loop, fd);
        // FIXME: Add connection->m_network.onDisconnect handler
        auto client = connection->m_rpc_client.bootstrap(ServerVatId().vat_id).castAs<messages::Init>();
        auto proxy = MakeUnique<ProxyClient<messages::Init>>(kj::mv(client), loop);
        proxy->addCloseHook(MakeUnique<ShutdownLoop>(loop));
        init_promise.set_value(std::move(proxy));
        loop.loop();
    });
    return init_promise.get_future().get();
}

void Listen(int fd, Init& init)
{
    EventLoop loop;
    auto stream = loop.m_io_context.lowLevelProvider->wrapSocketFd(fd, kj::LowLevelAsyncIoProvider::TAKE_OWNERSHIP);
    ::capnp::TwoPartyVatNetwork network(*stream, ::capnp::rpc::twoparty::Side::SERVER, ::capnp::ReaderOptions());
    loop.m_task_set.add(network.onDisconnect().then([&loop]() { loop.shutdown(); }));
    using InitServer = MainServer<ProxyServer<messages::Init>>;
    auto server = kj::heap<InitServer>(init, loop);
    auto client = ::capnp::Capability::Client(kj::mv(server));
    auto rpc_server = ::capnp::makeRpcServer(network, kj::mv(client));
    loop.loop();
}

} // namespace capnp
} // namespace interface
