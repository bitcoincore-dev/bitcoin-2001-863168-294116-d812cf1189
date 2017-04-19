#include "ipc/capnp/util.h"

#include <kj/debug.h>

#include <future>

#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace ipc {
namespace capnp {

void LoggingErrorHandler::taskFailed(kj::Exception&& exception)
{
    KJ_LOG(ERROR, "Uncaught exception in daemonized task.", exception);
}

EventLoop::EventLoop(std::thread&& thread) : ioContext(kj::setupAsyncIo()), thread(std::move(thread))
{
    int fds[2];
    KJ_SYSCALL(socketpair(AF_UNIX, SOCK_STREAM, 0, fds));
    waitFd = fds[0];
    postFd = fds[1];
    if (const char* ipc_debug = getenv("IPC_DEBUG")) {
        if (atoi(ipc_debug)) {
            debug = true;
        }
    }
}

EventLoop::~EventLoop()
{
    KJ_ASSERT(!thread.joinable());
    KJ_ASSERT(waitFd == -1);
    KJ_ASSERT(postFd == -1);
}

void EventLoop::loop()
{
    kj::Own<kj::AsyncIoStream> waitStream{
        ioContext.lowLevelProvider->wrapSocketFd(waitFd, kj::LowLevelAsyncIoProvider::TAKE_OWNERSHIP)};
    char buffer;
    for (;;) {
        size_t bytes = -1;
        waitStream->read(&buffer, 0, 1).then([&](size_t s) { bytes = s; }).wait(ioContext.waitScope);
        if (bytes == 0) {
            waitStream = nullptr;
            waitFd = -1;
            break;
        }
        postFn();
        postFn = nullptr;
        taskSet.add(waitStream->write(&buffer, 1));
    }
}

void EventLoop::post(std::function<void()> fn)
{
    KJ_ASSERT(std::this_thread::get_id() != thread_id,
        "Error: IPC call from IPC callback handler not supported (use async)");
    std::lock_guard<std::mutex> lock(postMutex);
    postFn = std::move(fn);
    char signal = '\0';
    KJ_SYSCALL(write(postFd, &signal, 1));
    KJ_SYSCALL(read(postFd, &signal, 1));
}

void EventLoop::shutdown()
{
    std::thread closeThread = std::move(thread);
    int closeFd = postFd;
    postFd = -1;
    KJ_SYSCALL(close(closeFd));
    if (closeThread.joinable()) {
        closeThread.join();
    }
}

} // namespace capnp
} // namespace ipc
