#include <interfaces/capnp/proxy-impl.h>

#include <kj/async-prelude.h>
#include <kj/debug.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/syscall.h> /* For SYS_xxx definitions */
#include <unistd.h>

namespace interfaces {
namespace capnp {

thread_local ThreadContext g_thread_context;

void LoggingErrorHandler::taskFailed(kj::Exception&& exception)
{
    KJ_LOG(ERROR, "Uncaught exception in daemonized task.", exception);
    LogIpc(m_loop, "Uncaught exception in daemonized task.");
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
    m_post_fn = std::move(fn);
    char signal = '\0';
    KJ_SYSCALL(write(m_post_fd, &signal, 1));
    KJ_SYSCALL(read(m_post_fd, &signal, 1));
}

void EventLoop::shutdown()
{
    std::thread close_thread = std::move(m_thread);
    int close_fd = m_post_fd;
    m_post_fd = -1;
    KJ_SYSCALL(close(close_fd));
    if (close_thread.joinable()) {
        close_thread.join();
    }
}

std::string ThreadName(const char* exe_name)
{
    char thread_name[17] = {0};
    prctl(PR_GET_NAME, thread_name, 0L, 0L, 0L);
    return strprintf("%s-%i/%s-%i", exe_name ? exe_name : "", getpid(), thread_name, int(syscall(SYS_gettid)));
}

std::string LongThreadName(const char* exe_name)
{
    return g_thread_context.waiter ? g_thread_context.waiter->m_name : ThreadName(exe_name);
}

ProxyServer<ThreadMap>::ProxyServer(EventLoop& loop) : m_loop(loop) {}

kj::Promise<void> ProxyServer<ThreadMap>::makeThread(MakeThreadContext context)
{
    Waiter* waiter = new Waiter(m_loop);
    std::string from = context.getParams().getName();
    {
        // Lock to ensure m_result is set before thread runs.
        std::unique_lock<std::mutex> lock(waiter->m_mutex);
        waiter->m_result = std::async([waiter, from, this]() {
            g_thread_context.waiter.reset(waiter);
            std::unique_lock<std::mutex> lock(waiter->m_mutex);
            waiter->m_name = ThreadName(m_loop.m_exe_name) + " (from " + from + ")";
            waiter->wait(lock, [waiter] { return !waiter->m_result.valid(); });
        });
    }
    auto thread = kj::heap<ProxyServer<Thread>>(*waiter);
    auto thread_client = m_loop.m_threads.add(kj::mv(thread));
    context.getResults().setResult(kj::mv(thread_client));
    return kj::READY_NOW;
}

// FIXME remove
void BreakPoint() {}

std::atomic<int> server_reqs{0};

} // namespace capnp
} // namespace interfaces
