// Copyright (c) 2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <interfaces/capnp/ipc.h>

#include <interfaces/base.h>
#include <interfaces/capnp/init.capnp.h>
#include <interfaces/capnp/init.capnp.proxy.h>
#include <interfaces/capnp/proxy-io.h>
#include <interfaces/capnp/proxy-types.h>
#include <interfaces/capnp/util.h>
#include <interfaces/init.h>
#include <interfaces/ipc.h>
#include <sync.h>
#include <util/memory.h>
#include <util/threadnames.h>

#include <assert.h>
#include <boost/optional.hpp>
#include <capnp/capability.h>
#include <capnp/common.h>
#include <capnp/message.h>
#include <capnp/rpc-twoparty.capnp.h>
#include <capnp/rpc-twoparty.h>
#include <capnp/rpc.h>
#include <future>
#include <kj/async-io.h>
#include <kj/async-prelude.h>
#include <kj/async.h>
#include <kj/common.h>
#include <kj/memory.h>
#include <list>
#include <memory>
#include <string>
#include <thread>
#include <utility>

namespace interfaces {
namespace capnp {
template <typename Interface>
struct ProxyClient;
template <typename Interface>
struct ProxyServer;

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
    ShutdownLoop(EventLoop& loop, std::unique_ptr<Connection> connection)
        : m_loop(loop), m_connection(std::move(connection))
    {
    }
    void onClose(Base& interface) override
    {
        if (m_connection) {
            m_loop.sync([&] { m_connection.reset(); });
        }
    }
    EventLoop& m_loop;
    std::unique_ptr<Connection> m_connection;
};

class IpcProtocolImpl : public IpcProtocol
{
public:
    IpcProtocolImpl(const char* exe_name, Init& init) : m_exe_name(exe_name), m_init(init) {}
    ~IpcProtocolImpl() noexcept(true)
    {
        if (m_loop_thread.joinable()) m_loop_thread.join();
    };

    std::unique_ptr<interfaces::Init> connect(int fd) override;
    void serve(int fd) override;

private:
    bool startLoop()
    {
        if (!m_loop) {
            std::promise<void> promise;
            if (m_loop_thread.joinable()) m_loop_thread.join();
            m_loop_thread = std::thread([&] {
                util::ThreadRename("capnp-loop");
                m_loop.emplace(m_exe_name);
                {
                    auto& loop = *m_loop;
                    WAIT_LOCK(loop.m_mutex, lock);
                    loop.addClient(lock);
                }
                promise.set_value();
                m_loop->loop();
                m_loop.reset();
            });
            promise.get_future().wait();
            return true;
        }
        return false;
    }
    void serveStream(kj::Own<kj::AsyncIoStream>&& stream);

    const char* m_exe_name;
    std::thread m_loop_thread;
    Optional<EventLoop> m_loop;
    Init& m_init;
};

std::unique_ptr<interfaces::Init> IpcProtocolImpl::connect(int fd)
{
    messages::Init::Client init_client(nullptr);
    std::unique_ptr<ShutdownLoop> cleanup;
    bool new_loop = startLoop();
    m_loop->sync([&] {
        auto connection = MakeUnique<Connection>(*m_loop, fd, !new_loop);
        init_client = connection->m_rpc_system.bootstrap(ServerVatId().vat_id).castAs<messages::Init>();
        cleanup = MakeUnique<ShutdownLoop>(*m_loop, std::move(connection));
        auto* cleanup_ptr = cleanup.get();
        m_loop->m_task_set->add(cleanup->m_connection->m_network.onDisconnect().then([this, cleanup_ptr] {
            LogIpc(*m_loop, "IPC client: unexpected network disconnect.\n");
            cleanup_ptr->m_connection.reset();
        }));
    });
    auto proxy = MakeUnique<ProxyClient<messages::Init>>(kj::mv(init_client), *cleanup->m_connection);
    proxy->addCloseHook(std::move(cleanup));
    return proxy;
}

void IpcProtocolImpl::serve(int fd)
{
    assert(!m_loop);
    g_thread_context.thread_name = ThreadName(m_exe_name);
    m_loop.emplace(m_exe_name);
    serveStream(m_loop->m_io_context.lowLevelProvider->wrapSocketFd(fd, kj::LowLevelAsyncIoProvider::TAKE_OWNERSHIP));
    m_loop->loop();
}

void IpcProtocolImpl::serveStream(kj::Own<kj::AsyncIoStream>&& stream)
{
    // Set owned to false so proxy object doesn't attempt to delete m_init
    // object on disconnect/close.
    ProxyServer<messages::Init>* init_server_ptr = nullptr;
    auto make_client = [&]() {
        auto init_server = kj::heap<ProxyServer<messages::Init>>(m_init, *m_loop);
        init_server_ptr = init_server.get();
        return init_server;
    };
    m_loop->m_incoming_connections.emplace_front(*m_loop, kj::mv(stream), make_client);
    auto it = m_loop->m_incoming_connections.begin();
    init_server_ptr->m_connection = &*it;
    m_loop->m_task_set->add(it->m_network.onDisconnect().then([this, it] {
        LogIpc(*m_loop, "IPC server: socket disconnected.\n");
        m_loop->m_incoming_connections.erase(it);
    }));
}

} // namespace

std::unique_ptr<IpcProtocol> MakeCapnpProtocol(const char* exe_name, Init& init)
{
    return MakeUnique<IpcProtocolImpl>(exe_name, init);
}

} // namespace capnp
} // namespace interfaces
