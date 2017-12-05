// Copyright (c) 2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <interfaces/capnp/ipc.h>

#include <interfaces/base.h>
#include <interfaces/capnp/init.capnp.h>
#include <interfaces/capnp/init.capnp.proxy.h>
#include <interfaces/init.h>
#include <interfaces/ipc.h>
#include <sync.h>
#include <util/memory.h>
#include <util/threadnames.h>

#include <assert.h>
#include <boost/optional.hpp>
#include <future>
#include <list>
#include <memory>
#include <mp/proxy-io.h>
#include <mp/proxy-types.h>
#include <mp/util.h>
#include <string>
#include <thread>
#include <utility>

namespace interfaces {
namespace capnp {

namespace {
void IpcLogFn(bool raise, std::string message)
{
    LogPrint(BCLog::IPC, "%s\n", message);
    if (raise) throw IpcException(message);
}

class IpcProtocolImpl : public IpcProtocol
{
public:
    IpcProtocolImpl(const char* exe_name, Init& init) : m_exe_name(exe_name), m_init(init) {}
    ~IpcProtocolImpl() noexcept(true)
    {
        if (m_loop_thread.joinable()) m_loop_thread.join();
    };

    std::unique_ptr<interfaces::Init> connect(int fd) override
    {
        bool new_loop = startLoop();
        return mp::ConnectStream<messages::Init>(*m_loop, fd, !new_loop);
    }
    void serve(int fd) override
    {
        assert(!m_loop);
        mp::g_thread_context.thread_name = mp::ThreadName(m_exe_name);
        m_loop.emplace(m_exe_name, &IpcLogFn);
        mp::ServeStream(*m_loop, fd, [&](mp::Connection& connection) {
            // Set owned to false so proxy object doesn't attempt to delete init
            // object on disconnect/close.
            return kj::heap<mp::ProxyServer<messages::Init>>(&m_init, false, connection);
        });
        m_loop->loop();
    }

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

    const char* m_exe_name;
    std::thread m_loop_thread;
    boost::optional<mp::EventLoop> m_loop;
    Init& m_init;
};
} // namespace

std::unique_ptr<IpcProtocol> MakeCapnpProtocol(const char* exe_name, Init& init)
{
    return MakeUnique<IpcProtocolImpl>(exe_name, init);
}

} // namespace capnp
} // namespace interfaces
