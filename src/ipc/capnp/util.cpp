#include <ipc/capnp/util.h>

#include <kj/async-prelude.h>
#include <kj/debug.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

namespace ipc {
namespace capnp {

void LoggingErrorHandler::taskFailed(kj::Exception&& exception)
{
    KJ_LOG(ERROR, "Uncaught exception in daemonized task.", exception);
}

EventLoop::EventLoop(std::thread&& thread) : m_io_context(kj::setupAsyncIo()), m_thread(std::move(thread))
{
    int fds[2];
    KJ_SYSCALL(socketpair(AF_UNIX, SOCK_STREAM, 0, fds));
    m_wait_fd = fds[0];
    m_post_fd = fds[1];
    if (const char* ipc_debug = getenv("IPC_DEBUG")) {
        if (atoi(ipc_debug)) {
            m_debug = true;
        }
    }
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

} // namespace capnp
} // namespace ipc
