#include <interfaces/capnp/proxy-impl.h>

#include <kj/async-prelude.h>
#include <kj/debug.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

namespace interfaces {
namespace capnp {
namespace {
constexpr char POST_SIGNAL = 1;
constexpr char SHUTDOWN_SIGNAL = 2;
} // namespace

thread_local ThreadContext g_thread_context;

void LoggingErrorHandler::taskFailed(kj::Exception&& exception)
{
    KJ_LOG(ERROR, "Uncaught exception in daemonized task.", exception);
    LogIpc(m_loop, "Uncaught exception in daemonized task.\n");
}

Connection::~Connection()
{
    // When a connection is closed, capnp will call all ProxyServer
    // object destructors and trigger an onDisconnect callback. Order On the listening side, it will trigger a onDisconnect
    // handler which will delete the Connection object from the m_incoming_connections list and call the destructor which calls Connection::disconnect.
    // On the connecting side, the connection object is owned by the client and sticks around for as long as client wants it, but onDisconnect handler will call Connection::disconnect. This lets ProxyServer destructors run, and allows any new client calls to throw clientInvoke exceptions without trying to do i/o and sending more obscure capnp i/o errors to caller.

    // Sync callbacks set ProxyClient capability pointers to null, so new method calls on client objects fail without triggering i/o or relying on event loop. Async callbacks call ProxyServer impl destructors, which need to run on a separate thread because they call external code which could be blocking. Calling async cleanup after sync cleanup avoids race condition with destructor code trying to invoke methods on proxyclient object that was just disconnected.

    // FIXME: Rename loop.m_connections, loop.m_incoming_connections
    // FIXME: Add Connection.disconnect method which will call all these cleanup functions. Call from destructor
    // FIXME: Replace addCleanup with addSyncCleanup/addAsyncCleanup variants. sync versions will all run first, followed by async functions
    // FIXME: Maybe remove loop.shutdown method. SHould be no need for ShutdownLoop to call it given addClient/removeClient calls in ProxyClient.
    while (!m_cleanup_fns.empty()) {
        m_cleanup_fns.front()();
        m_cleanup_fns.pop_front();
    }
}

CleanupIt Connection::addCleanup(std::function<void()> fn)
{
    std::unique_lock<std::mutex> lock(m_loop.m_mutex);
    m_loop.m_cv.notify_all();
    return m_cleanup_fns.emplace(m_cleanup_fns.begin(), std::move(fn));
}

void Connection::removeCleanup(CleanupIt it)
{
    std::unique_lock<std::mutex> lock(m_loop.m_mutex);
    m_loop.m_cv.notify_all();
    m_cleanup_fns.erase(it);
}

EventLoop::EventLoop(const char* exe_name) : m_exe_name(exe_name), m_io_context(kj::setupAsyncIo())
{
    int fds[2];
    KJ_SYSCALL(socketpair(AF_UNIX, SOCK_STREAM, 0, fds));
    m_wait_fd = fds[0];
    m_post_fd = fds[1];
    m_task_set.emplace(m_error_handler);
}

EventLoop::~EventLoop()
{
    if (m_async_thread.joinable()) m_async_thread.join();
    std::lock_guard<std::mutex> lock(m_mutex);
    KJ_ASSERT(m_post_fn == nullptr);
    KJ_ASSERT(m_async_fns.empty());
    KJ_ASSERT(m_wait_fd == -1);
    KJ_ASSERT(m_post_fd == -1);
    KJ_ASSERT(m_shutdown);
    KJ_ASSERT(m_num_clients == 0);

    // Spin event loop. wait for any promises triggered by RPC shutdown.
    // auto cleanup = kj::evalLater([]{});
    // cleanup.wait(m_io_context.waitScope);
}

void EventLoop::loop()
{
    assert(!g_thread_context.loop_thread);
    TempSetter<bool> temp_setter(g_thread_context.loop_thread, true);

    kj::Own<kj::AsyncIoStream> wait_stream{
        m_io_context.lowLevelProvider->wrapSocketFd(m_wait_fd, kj::LowLevelAsyncIoProvider::TAKE_OWNERSHIP)};
    char buffer = 0;
    for (;;) {
        wait_stream->read(&buffer, 0, 1).wait(m_io_context.waitScope);
        std::unique_lock<std::mutex> lock(m_mutex, std::defer_lock);
        if (buffer == POST_SIGNAL) {
            assert(m_post_fn);
            (*m_post_fn)();
            lock.lock();
            m_post_fn = nullptr;
        } else if (buffer == SHUTDOWN_SIGNAL) {
            LogIpc(*this, "EventLoop::loop shutdown received, disconnecting %i connections.\n", m_connections.size());
            m_connections.clear();
            lock.lock();
            assert(m_shutdown);
        } else {
            throw std::logic_error("Unexpected EventLoop signal.");
        }
        m_cv.notify_all();
        if (done()) {
            LogIpc(*this, "EventLoop::loop done, cancelling tasks.\n");
            m_task_set.reset();

            // Wait for any promises triggered by cleanup_fns calls.
            // auto cleanup = kj::evalLater([]{});
            // cleanup.wait(m_io_context.waitScope);

            LogIpc(*this, "EventLoop::loop bye.\n");
            break;
        }
    }
    wait_stream = nullptr;
    m_wait_fd = -1;
    KJ_SYSCALL(::close(m_post_fd));
    m_post_fd = -1;
}

void EventLoop::post(const std::function<void()>& fn)
{
    if (std::this_thread::get_id() == m_thread_id) {
        return fn();
    }
    std::unique_lock<std::mutex> lock(m_mutex);
    m_cv.wait(lock, [this] { return m_post_fn == nullptr; });
    m_post_fn = &fn;
    Unlock(m_mutex, [&] {
        char signal = POST_SIGNAL;
        KJ_SYSCALL(write(m_post_fd, &signal, 1));
    });
    m_cv.wait(lock, [this, &fn] { return m_post_fn != &fn; });
}

void EventLoop::shutdown()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    if (!m_shutdown) {
        m_shutdown = true;
        lock.unlock();
        char signal = SHUTDOWN_SIGNAL;
        KJ_SYSCALL(write(m_post_fd, &signal, 1));
    }
}

void EventLoop::addClient()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_num_clients += 1;
    m_cv.notify_all();
}

void EventLoop::removeClient()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_num_clients -= 1;
    m_cv.notify_all();
    if (done()) {
        lock.unlock();
        char signal = SHUTDOWN_SIGNAL;
        KJ_SYSCALL(write(m_post_fd, &signal, 1));
    }
}

void EventLoop::async(std::function<void()> fn)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_async_fns.push_back(std::move(fn));
    if (!m_async_thread.joinable()) {
        m_async_thread = std::thread([this] {
            std::unique_lock<std::mutex> lock(m_mutex);
            for (;;) {
                if (!m_async_fns.empty()) {
                    std::function<void()> fn = std::move(m_async_fns.front());
                    m_async_fns.pop_front();
                    Unlock(m_mutex, fn);
                } else if (m_shutdown) {
                    Unlock(m_mutex, [&] {
                        char signal = 3;
                        KJ_SYSCALL(write(m_post_fd, &signal, 1));
                    });
                    if (done()) break;
                }
                m_cv.wait(lock);
            }
        });
    }
}

void AddClient(EventLoop& loop) { loop.addClient(); }
void RemoveClient(EventLoop& loop) { loop.removeClient(); }

ProxyServer<Thread>::ProxyServer(ThreadContext& thread_context, std::thread&& thread)
    : m_thread_context(thread_context), m_thread(std::move(thread))
{
    assert(m_thread_context.waiter.get() != nullptr);
}

ProxyServer<Thread>::~ProxyServer()
{
    if (!m_thread.joinable()) return;
    // Stop async thread and wait for it to exit. Need to wait because the
    // m_thread handle needs to outlive the thread to avoid "terminate called
    // without an active exception" error. An alternative to waiting would be
    // detach the thread, but this would introduce nondeterminism which could
    // make code harder to debug or extend.
    assert(m_thread_context.waiter.get());
    std::unique_ptr<Waiter> waiter;
    {
        std::unique_lock<std::mutex> lock(m_thread_context.waiter->m_mutex);
        //! Reset thread context waiter pointer, as shutdown signal for done
        //! lambda passed as waiter->wait() argument in makeThread code below.
        waiter = std::move(m_thread_context.waiter);
        //! Assert waiter is idle. This destructor shouldn't be getting called if it is busy.
        assert(!waiter->m_fn);
        // Clear client maps now to avoid deadlock in m_thread.join() call
        // below. The maps contain Thread::Client objects that need to be
        // destroyed from the event loop thread (this thread), which can't
        // happen if this thread is busy calling join.
        m_thread_context.request_threads.clear();
        m_thread_context.callback_threads.clear();
        //! Ping waiter.
        waiter->m_cv.notify_all();
    }
    m_thread.join();
}

kj::Promise<void> ProxyServer<Thread>::getName(GetNameContext context)
{
    context.getResults().setResult(m_thread_context.thread_name);
    return kj::READY_NOW;
}

ProxyServer<ThreadMap>::ProxyServer(Connection& connection) : m_connection(connection) {}

kj::Promise<void> ProxyServer<ThreadMap>::makeThread(MakeThreadContext context)
{
    std::string from = context.getParams().getName();
    std::promise<ThreadContext*> thread_context;
    std::thread thread([&thread_context, from, this]() {
        g_thread_context.thread_name = ThreadName(m_connection.m_loop.m_exe_name) + " (from " + from + ")";
        g_thread_context.waiter = MakeUnique<Waiter>();
        thread_context.set_value(&g_thread_context);
        std::unique_lock<std::mutex> lock(g_thread_context.waiter->m_mutex);
        // Wait for shutdown signal from ProxyServer<Thread> destructor (signal
        // is just waiter getting set to null.)
        g_thread_context.waiter->wait(lock, [] { return !g_thread_context.waiter; });
    });
    auto thread_server = kj::heap<ProxyServer<Thread>>(*thread_context.get_future().get(), std::move(thread));
    auto thread_client = m_connection.m_threads.add(kj::mv(thread_server));
    context.getResults().setResult(kj::mv(thread_client));
    return kj::READY_NOW;
}

std::atomic<int> server_reqs{0};


std::string LongThreadName(const char* exe_name)
{
    return g_thread_context.thread_name.empty() ? ThreadName(exe_name) : g_thread_context.thread_name;
}

} // namespace capnp
} // namespace interfaces
