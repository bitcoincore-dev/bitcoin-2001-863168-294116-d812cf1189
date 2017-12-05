#ifndef BITCOIN_INTERFACES_IPC_H
#define BITCOIN_INTERFACES_IPC_H

#include <fs.h>
#include <interfaces/init.h>

#include <memory>

namespace interfaces {

struct Config;

//! IPC process interface for spawning bitcoin processes and serving requests
//! in processes that have been spawned.
//!
//! There will different implementations of this interface depending on the
//! platform (e.g. unix, windows).
class IpcProcess
{
public:
    virtual ~IpcProcess() = default;

    //! Spawn process and return socket file descriptor for communicating with
    //! it.
    virtual int spawn(const std::string& new_exe_name, int& pid) = 0;

    //! Wait for spawned process to exit and return exit code.
    virtual int wait(int pid) = 0;

    //! Serve request if current process is a spawned subprocess. Blocks until
    //! socket for communicating with the parent process is disconnected.
    virtual bool serve(int& exit_status) = 0;
};

//! IPC protocol interface for calling IPC methods over sockets.
//!
//! There may be different implementations of this interface for different IPC
//! protocols (e.g. Cap'n Proto, gRPC, JSON-RPC, or custom protocols).
//!
//! An implementation of this interface needs to provide an `interface::Init`
//! object that translates method calls into requests sent over a socket, and it
//! needs to implement a handler that translates requests received over a socket
//! into method calls on a provided `interface::Init` object.
class IpcProtocol
{
public:
    virtual ~IpcProtocol() = default;

    //! Return Init interface that forwards requests over given socket descriptor.
    virtual std::unique_ptr<Init> connect(int fd) = 0;

    //! Handle requests on provided socket descriptor. Blocks until client
    //! disconnects.
    virtual void serve(int fd) = 0;
};

//! Exception class thrown when a call to remote method fails due to an IPC
//! error, like a socket getting disconnected.
class IpcException : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

//! Constructor for IpcProcess interface. Implementation will vary depending on
//! the platform (unix or windows).
using MakeIpcProcessFn = std::unique_ptr<IpcProcess>(int argc, char* argv[], const Config& config, Init& init);
MakeIpcProcessFn MakeIpcProcess;

//! Constructor for the IpcProtocol interface. Implementation will vary
//! depending on how the executable is linked (contents of the
//! `interfaces::g_config` global struct).
using MakeIpcProtocolFn = std::unique_ptr<IpcProtocol>(const char* exe_name, Init& init);

} // namespace interfaces

#endif // BITCOIN_INTERFACES_IPC_H
