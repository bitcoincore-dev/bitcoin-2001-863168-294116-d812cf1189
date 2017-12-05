// Copyright (c) 2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <interfaces/capnp/ipc.h>

#include <interfaces/base.h>
#include <interfaces/capnp/init.capnp.h>
#include <interfaces/capnp/init.capnp.proxy.h>
#include <interfaces/init.h>
#include <interfaces/ipc.h>
#include <mp/proxy-io.h>
#include <mp/proxy-types.h>
#include <mp/util.h>
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

namespace {

void IpcLogFn(bool raise, std::string message)
{
    LogPrint(BCLog::IPC, "%s", message);
    if (raise) throw IpcException(message);
}

class ShutdownLoop : public CloseHook
{
public:
    ShutdownLoop(mp::EventLoop& loop, std::unique_ptr<mp::Connection> connection)
        : m_loop(loop), m_connection(std::move(connection))
    {
    }
    void onClose(Base& interface) override
    {
        if (m_connection) {
            m_loop.sync([&] { m_connection.reset(); });
        }
    }
    mp::EventLoop& m_loop;
    std::unique_ptr<mp::Connection> m_connection;
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
                m_loop.emplace(m_exe_name, &IpcLogFn);
                {
                    auto& loop = *m_loop;
                    std::unique_lock<std::mutex> lock(loop.m_mutex);
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
    boost::optional<mp::EventLoop> m_loop;
    Init& m_init;
};

std::unique_ptr<interfaces::Init> IpcProtocolImpl::connect(int fd)
{
    messages::Init::Client init_client(nullptr);
    std::unique_ptr<ShutdownLoop> cleanup;
    bool new_loop = startLoop();
    m_loop->sync([&] {
        auto stream = m_loop->m_io_context.lowLevelProvider->wrapSocketFd(fd, kj::LowLevelAsyncIoProvider::TAKE_OWNERSHIP);
        auto connection = MakeUnique<mp::Connection>(*m_loop, kj::mv(stream), !new_loop);
        init_client = connection->m_rpc_system.bootstrap(mp::ServerVatId().vat_id).castAs<messages::Init>();
        cleanup = MakeUnique<ShutdownLoop>(*m_loop, std::move(connection));
        auto* cleanup_ptr = cleanup.get();
        m_loop->m_task_set->add(cleanup->m_connection->m_network.onDisconnect().then([this, cleanup_ptr] {
            LogIpc(*m_loop, "IPC client: unexpected network disconnect.\n");
            cleanup_ptr->m_connection.reset();
        }));
    });
    auto proxy = MakeUnique<mp::ProxyClient<messages::Init>>(kj::mv(init_client), *cleanup->m_connection);
    proxy->addCloseHook(std::move(cleanup));
    return proxy;
}

void IpcProtocolImpl::serve(int fd)
{
    assert(!m_loop);
    mp::g_thread_context.thread_name = mp::ThreadName(m_exe_name);
    m_loop.emplace(m_exe_name, &IpcLogFn);
    serveStream(m_loop->m_io_context.lowLevelProvider->wrapSocketFd(fd, kj::LowLevelAsyncIoProvider::TAKE_OWNERSHIP));
    m_loop->loop();
}

void IpcProtocolImpl::serveStream(kj::Own<kj::AsyncIoStream>&& stream)
{
    // Set owned to false so proxy object doesn't attempt to delete m_init
    // object on disconnect/close.
    auto make_client = [&](mp::Connection& connection) {
        return kj::heap<mp::ProxyServer<messages::Init>>(&m_init, false, connection);
    };
    m_loop->m_incoming_connections.emplace_front(*m_loop, kj::mv(stream), make_client);
    auto it = m_loop->m_incoming_connections.begin();
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
