#include <interfaces/capnp/ipc.h>

#include <bitcoin-config.h>
#include <capnp/rpc-twoparty.h>
#include <chainparams.h>
#include <coins.h>
#include <future>
#include <interfaces/capnp/messages.capnp.h>
#include <interfaces/capnp/messages.capnp.proxy.h>
#include <interfaces/capnp/proxy-impl.h>
#include <interfaces/capnp/proxy.h>
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
namespace {

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
        // FIXME: Shutdown can cause segfault right now because
        // close/destruction sequence doesn't wait for server objects across
        // the pipe to shut down, so e.g. things like handlers don't get a
        // chance to get deleted in right sequence. Instead with this shutdown
        // they get deleted in EventLoop destructor which can cause segfault on
        // UnregisterValidationInterface.
        // if (!remote) m_loop.shutdown();
    }
    EventLoop& m_loop;
};

struct StreamContext
{
    kj::Own<kj::AsyncIoStream> stream;
    ::capnp::TwoPartyVatNetwork network;
    ::capnp::RpcSystem<::capnp::rpc::twoparty::VatId> rpc_system;

    StreamContext(kj::Own<kj::AsyncIoStream>&& stream_, std::function<::capnp::Capability::Client()> make_client)
        : stream(kj::mv(stream_)), network(*stream, ::capnp::rpc::twoparty::Side::SERVER, ::capnp::ReaderOptions()),
          rpc_system(::capnp::makeRpcServer(network, make_client()))
    {
    }
};

class IpcProtocolImpl : public IpcProtocol
{
public:
    IpcProtocolImpl(const char* exe_name, Init& init) : m_exe_name(exe_name), m_init(init) {}
    ~IpcProtocolImpl() noexcept(true){};

    std::unique_ptr<interfaces::Init> connect(int fd) override;
    void serve(int stream_fd) override;
    void loop() override { m_loop->loop(); }
    void shutdown() override { m_loop->shutdown(); }

private:
    void listen(kj::Own<kj::ConnectionReceiver>&& listener);
    void serve(kj::Own<kj::AsyncIoStream>&& stream, bool shutdown);

    const char* m_exe_name;
    Optional<EventLoop> m_loop;
    Init& m_init;
};

void IpcProtocolImpl::serve(int stream_fd)
{
    m_loop.emplace(m_exe_name);
    serve(m_loop->m_io_context.lowLevelProvider->wrapSocketFd(stream_fd, kj::LowLevelAsyncIoProvider::TAKE_OWNERSHIP),
        true /* shutdown */);
}

void IpcProtocolImpl::serve(kj::Own<kj::AsyncIoStream>&& stream, bool shutdown)
{
    auto make_client = [&]() { return kj::heap<ProxyServer<messages::Init>>(&m_init, false /* owned */, *m_loop); };
    auto ctx = kj::heap<StreamContext>(kj::mv(stream), make_client);
    auto* ptr = ctx.get();
    m_loop->m_task_set.add(
        ptr->network.onDisconnect().then(kj::mvCapture(kj::mv(ctx), [this, shutdown](kj::Own<StreamContext>&&) {
            if (shutdown) {
                LogIpc(*m_loop, "IPC server: socket disconnected, shutting down.");
                m_loop->shutdown();
            } else {
                LogIpc(*m_loop, "IPC server: socket disconnected, waiting for new connections.");
            }
        })));
}

void IpcProtocolImpl::listen(kj::Own<kj::ConnectionReceiver>&& listener)
{
    auto* ptr = listener.get();
    m_loop->m_task_set.add(ptr->accept().then(kj::mvCapture(
        kj::mv(listener), [this](kj::Own<kj::ConnectionReceiver>&& listener, kj::Own<kj::AsyncIoStream>&& connection) {
            serve(kj::mv(connection), false /* shutdown */);
        })));
}

std::unique_ptr<interfaces::Init> IpcProtocolImpl::connect(int fd)
{
    std::promise<messages::Init::Client> init_promise;
    EventLoop* loop_ptr = nullptr;
    Connection* connection_ptr = nullptr;
    std::thread thread([&]() {
        RenameThread("capnp-connect");
        EventLoop loop(m_exe_name, std::move(thread));
        Connection connection(loop, fd);
        loop_ptr = &loop;
        connection_ptr = &connection;
        init_promise.set_value(connection.m_rpc_client.bootstrap(ServerVatId().vat_id).castAs<messages::Init>());
        loop.loop();
    });
    auto&& client = init_promise.get_future().get();
    auto proxy = MakeUnique<ProxyClient<messages::Init>>(kj::mv(client), *loop_ptr);
    proxy->addCloseHook(MakeUnique<ShutdownLoop>(*loop_ptr));
    loop_ptr->sync([&] {
        loop_ptr->m_task_set.add(connection_ptr->m_network.onDisconnect().then([loop_ptr]() {
            LogIpc(*loop_ptr, "IPC client: unexpected network disconnect, shutting down.");
            loop_ptr->shutdown();
        }));
    });
    return proxy;
}

} // namespace

std::unique_ptr<IpcProtocol> MakeCapnpProtocol(const char* exe_name, Init& init)
{
    return MakeUnique<IpcProtocolImpl>(exe_name, init);
}

} // namespace capnp
} // namespace interfaces
