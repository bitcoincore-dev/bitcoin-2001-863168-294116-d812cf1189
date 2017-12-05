// Copyright (c) 2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

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
    ShutdownLoop(EventLoop& loop, std::thread&& thread) : m_loop(loop), m_thread(std::move(thread)) {}
    void onClose(Base& interface) override
    {
        m_loop.shutdown();
        m_thread.join();
    }
    EventLoop& m_loop;
    std::thread m_thread;
};

class IpcProtocolImpl : public IpcProtocol
{
public:
    IpcProtocolImpl(const char* exe_name, Init& init) : m_exe_name(exe_name), m_init(init) {}
    ~IpcProtocolImpl() noexcept(true){};

    std::unique_ptr<interfaces::Init> connect(int fd) override;
    void serve(int stream_fd, std::function<void()> on_disconnect) override;
    void loop() override { m_loop->loop(); }
    void shutdown() override { m_loop->shutdown(); }

private:
    void listen(kj::Own<kj::ConnectionReceiver>&& listener);
    void serve(kj::Own<kj::AsyncIoStream>&& stream, std::function<void()>&& on_disconnect);

    const char* m_exe_name;
    Optional<EventLoop> m_loop;
    Init& m_init;
};

void IpcProtocolImpl::serve(int stream_fd, std::function<void()> on_disconnect = {})
{
    g_thread_context.thread_name = ThreadName(m_exe_name);
    m_loop.emplace(m_exe_name);
    serve(m_loop->m_io_context.lowLevelProvider->wrapSocketFd(stream_fd, kj::LowLevelAsyncIoProvider::TAKE_OWNERSHIP),
        std::move(on_disconnect));
}

void IpcProtocolImpl::serve(kj::Own<kj::AsyncIoStream>&& stream, std::function<void()>&& on_disconnect = {})
{
    // Set owned to false so proxy object doesn't attempt to delete m_init
    // object on disconnect/close.
    ProxyServer<messages::Init>* init_server_ptr = nullptr;
    auto make_client = [&]() {
        auto init_server =
            kj::heap<ProxyServer<messages::Init>>(&m_init, false /* owned */, *m_loop, nullptr /* connection */);
        init_server_ptr = init_server.get();
        return init_server;
    };
    m_loop->m_connections.emplace_front(*m_loop, kj::mv(stream), make_client);
    auto it = m_loop->m_connections.begin();
    init_server_ptr->m_connection = &*it;
    m_loop->m_task_set->add(it->m_network.onDisconnect().then(
        kj::mvCapture(kj::mv(on_disconnect), [this, it](std::function<void()>&& on_disconnect) {
            LogIpc(*m_loop, "IPC server: socket disconnected.\n");
            if (on_disconnect) on_disconnect();
            m_loop->m_connections.erase(it);
        })));
    m_loop->m_shutdown = true; // Shut down server when last client is disconnected.
}

void IpcProtocolImpl::listen(kj::Own<kj::ConnectionReceiver>&& listener)
{
    auto* ptr = listener.get();
    m_loop->m_task_set->add(ptr->accept().then(
        kj::mvCapture(kj::mv(listener), [this](kj::Own<kj::ConnectionReceiver>&& listener,
                                            kj::Own<kj::AsyncIoStream>&& connection) { serve(kj::mv(connection)); })));
}

// FIXME move above to match class method order
std::unique_ptr<interfaces::Init> IpcProtocolImpl::connect(int fd)
{
    std::promise<messages::Init::Client> init_promise;
    Connection* connection = nullptr;
    std::thread thread([&]() {
        RenameThread("capnp-connect");
        EventLoop loop(m_exe_name);
        loop.m_connections.emplace_front(loop, fd);
        connection = &loop.m_connections.front();
        loop.m_task_set->add(connection->m_network.onDisconnect().then(
            [&loop]() { LogIpc(loop, "IPC client: unexpected network disconnect.\n"); }));
        init_promise.set_value(connection->m_rpc_system.bootstrap(ServerVatId().vat_id).castAs<messages::Init>());
        loop.loop();
    });
    auto&& client = init_promise.get_future().get();
    auto proxy = MakeUnique<ProxyClient<messages::Init>>(kj::mv(client), *connection);
    proxy->addCloseHook(MakeUnique<ShutdownLoop>(connection->m_loop, std::move(thread)));
    return proxy;
}

} // namespace

std::unique_ptr<IpcProtocol> MakeCapnpProtocol(const char* exe_name, Init& init)
{
    return MakeUnique<IpcProtocolImpl>(exe_name, init);
}

} // namespace capnp
} // namespace interfaces
