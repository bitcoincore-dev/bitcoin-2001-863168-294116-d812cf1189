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
    virtual std::unique_ptr<Init> connect(int fd) = 0;

    //! Handle requests on provided socket descriptor. Shuts down event loop on
    //! disconnect.
    virtual void serve(int stream_fd) = 0;

    //! Run event loop handling requests. Blocks until shutdown is called or a
    //! provided stream_fd disconnects.
    virtual void loop() = 0;

    //! Shut down event loop.
    virtual void shutdown() = 0;
};

using MakeIpcProcessFn = std::unique_ptr<IpcProcess>(int argc, char* argv[], const Config& config, Init& init);
MakeIpcProcessFn MakeIpcProcess;

using MakeIpcProtocolFn = std::unique_ptr<IpcProtocol>(const char* exe_name, Init& init);

} // namespace interfaces

#endif // BITCOIN_INTERFACES_IPC_H
