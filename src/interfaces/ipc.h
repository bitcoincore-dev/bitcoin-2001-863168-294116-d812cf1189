#ifndef BITCOIN_INTERFACES_IPC_H
#define BITCOIN_INTERFACES_IPC_H

#include <fs.h>
#include <interfaces/init.h>

#include <memory>

namespace interfaces {

struct Config;

//! IPC process interface for spawning bitcoin processes and serving requests
//! for processes that have been spawned. These functions are independent of the
//! IPC protocol and should work with any protocol that can communicate across
//! socket descriptors.
class IpcProcess
{
public:
    virtual ~IpcProcess() = default;

    //! Serve request if current process is spawned subprocess.
    virtual bool serve(int& exit_status) = 0;

    //! Spawn process and return file descriptor for communicating with it.
    virtual int spawn(const std::string& new_exe_name) = 0;
};

//! IPC protocol interface for communicating with other processes over socket
//! descriptors.
class IpcProtocol
{
public:
    virtual ~IpcProtocol() = default;

    //! Return Init interface that forwards requests over given socket descriptor.
    //!
    //! @note It could be potentially useful in the future to add
    //! std::function<void()> on_disconnect callback argument here. But there
    //! isn't an immediate need, because the protocol should responsible for
    //! cleaning up its own state (calling ProxyServer destructors, etc) on disconnection, and
    //! any new calls made from the client calls will be just throw IpcException.
    virtual std::unique_ptr<Init> connect(int fd) = 0;

    //! Handle requests on provided socket descriptor. If on_disconnect is
    //! non-null, it will be invoked during the loop if the client disconnects.
    //! If on_disconnect is null, the loop keep running as long as the client is
    //! connected.
    virtual void serve(int stream_fd, std::function<void()> on_disconnect = {}) = 0;

    //! Run event loop handling requests. Blocks until shutdown is called or a
    //! client that did not specify an on_disconnect handler disconnects.
    virtual void loop() = 0;

    //! Shut down event loop.
    virtual void shutdown() = 0;
};

class IpcException : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

using MakeIpcProcessFn = std::unique_ptr<IpcProcess>(int argc, char* argv[], const Config& config, Init& init);
MakeIpcProcessFn MakeIpcProcess;

using MakeIpcProtocolFn = std::unique_ptr<IpcProtocol>(const char* exe_name, Init& init);

} // namespace interfaces

#endif // BITCOIN_INTERFACES_IPC_H
