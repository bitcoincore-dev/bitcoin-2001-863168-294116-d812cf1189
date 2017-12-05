#include <interfaces/capnp/proxy-impl.h>

#include <kj/async-prelude.h>
#include <kj/debug.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

namespace interfaces {
namespace capnp {

thread_local ThreadContext g_thread_context;

void LoggingErrorHandler::taskFailed(kj::Exception&& exception)
{
    KJ_LOG(ERROR, "Uncaught exception in daemonized task.", exception);
    LogIpc(m_loop, "Uncaught exception in daemonized task.\n");
}

EventLoop::EventLoop(const char* exe_name, std::thread&& thread)
    : m_exe_name(exe_name), m_thread_map(nullptr), m_io_context(kj::setupAsyncIo()), m_thread(std::move(thread))
{
    int fds[2];
    KJ_SYSCALL(socketpair(AF_UNIX, SOCK_STREAM, 0, fds));
    m_wait_fd = fds[0];
    m_post_fd = fds[1];
}

EventLoop::~EventLoop()
{
    KJ_ASSERT(!m_thread.joinable());
    KJ_ASSERT(m_wait_fd == -1);
    KJ_ASSERT(m_post_fd == -1);
    for (auto& fn : m_cleanup_fns) {
        fn();
    }
}

void EventLoop::loop()
{
    assert(!g_thread_context.loop_thread);
    TempSetter<bool> temp_setter(g_thread_context.loop_thread, true);

    kj::Own<kj::AsyncIoStream> wait_stream{
        m_io_context.lowLevelProvider->wrapSocketFd(m_wait_fd, kj::LowLevelAsyncIoProvider::TAKE_OWNERSHIP)};
    char buffer;
    for (;;) {
        size_t bytes = -1;
        wait_stream->read(&buffer, 0, 1).then([&](size_t s) { bytes = s; }).wait(m_io_context.waitScope);
        if (bytes == 0) {
            wait_stream = nullptr;
            m_wait_fd = -1;
            break;
        }
        m_post_fn();
        m_post_fn = nullptr;
        m_task_set.add(wait_stream->write(&buffer, 1));
    }
}

void EventLoop::post(std::function<void()> fn)
{
    if (std::this_thread::get_id() == m_thread_id) {
        fn();
        return;
    }
    std::lock_guard<std::mutex> lock(m_post_mutex);
    assert(m_post_fd != -1);
    m_post_fn = std::move(fn);
    char signal = '\0';
    KJ_SYSCALL(write(m_post_fd, &signal, 1));
    KJ_SYSCALL(read(m_post_fd, &signal, 1));
}

void EventLoop::shutdown()
{
    if (m_post_fd == -1) return;
    std::thread close_thread = std::move(m_thread);
    int close_fd = m_post_fd;
    m_post_fd = -1;
    KJ_SYSCALL(close(close_fd));
    if (close_thread.joinable()) {
        close_thread.join();
    }
}

ProxyServer<Thread>::ProxyServer(ThreadContext& context, bool join_thread)
    : m_context(context), m_join_thread(join_thread)
{
    assert(m_context.waiter.get() != nullptr);
}

ProxyServer<Thread>::~ProxyServer()
{
    if (!m_join_thread) return;
    // Stop std::async thread and wait for it to exit. Need to wait because
    // the waiter m_result promise which is returned by std::async and
    // destroys the std::thread object needs to outlive the thread to avoid
    // "terminate called without an active exception" error. An alternative
    // to waiting would be to use detached threads, but this would introduce
    // some nondetermism which might make code harder to debug or extend.
    assert(m_context.waiter.get());
    std::unique_ptr<Waiter> waiter;
    {
        std::unique_lock<std::mutex> lock(m_context.waiter->m_mutex);
        waiter = std::move(m_context.waiter);
        waiter->m_cv.notify_all();
    }
    assert(waiter->m_thread.joinable());
    waiter->m_thread.join();
}

kj::Promise<void> ProxyServer<Thread>::getName(GetNameContext context)
{

    context.getResults().setResult(m_context.waiter->m_name);
    return kj::READY_NOW;
}

ProxyServer<ThreadMap>::ProxyServer(EventLoop& loop) : m_loop(loop) {}

kj::Promise<void> ProxyServer<ThreadMap>::makeThread(MakeThreadContext context)
{
    Waiter* waiter = new Waiter(m_loop);
    std::string from = context.getParams().getName();
    std::promise<ThreadContext*> thread_context;
    {
        // Lock to ensure Waiter::m_result is set before thread runs.
        std::unique_lock<std::mutex> lock(waiter->m_mutex);
        waiter->m_thread = std::thread([&thread_context, waiter, from, this]() {
            g_thread_context.waiter.reset(waiter);
            std::unique_lock<std::mutex> lock(waiter->m_mutex);
            waiter->m_name = ThreadName(m_loop.m_exe_name) + " (from " + from + ")";
            thread_context.set_value(&g_thread_context);
            waiter->wait(lock, [waiter] { return !g_thread_context.waiter; });
        });
    }
    auto thread = kj::heap<ProxyServer<Thread>>(*thread_context.get_future().get(), true /* join_thread */);
    auto thread_client = m_loop.m_threads.add(kj::mv(thread));
    context.getResults().setResult(kj::mv(thread_client));
    return kj::READY_NOW;
}

std::atomic<int> server_reqs{0};


std::string LongThreadName(const char* exe_name)
{
    return g_thread_context.waiter ? g_thread_context.waiter->m_name : ThreadName(exe_name);
}

} // namespace capnp
} // namespace interfaces
