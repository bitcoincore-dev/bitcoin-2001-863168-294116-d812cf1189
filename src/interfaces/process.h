#ifndef BITCOIN_INTERFACES_PROCESS_H
#define BITCOIN_INTERFACES_PROCESS_H

#include <memory>

namespace interfaces {

class Init;

//! Spawn process and return file descriptor for communicating with it.
//! This function is agnostic to the socket protocol.
using ProcessSpawnFn = int(const std::string& cur_exe_path, const std::string& new_exe_name);
ProcessSpawnFn ProcessSpawn;

//! Handle incoming requests for a process it was spawned as an IPC server. If
//! successful, this returns true, and the process can exit after it returns.
//! This function is agnostic to the socket protocol.
using ProcessServeFn = bool(int argc, char* argv[], int& exit_status, Init& init);
ProcessServeFn ProcessServe;

//! Return Init interface that forwards requests over given socket descriptor.
//! There can be different implementations of this function for different socket
//! protocols.
using SocketConnectFn = std::unique_ptr<Init>(const char* exe_name, int fd);

//! Handle incoming requests from a socket by forwarding to provided Init
//! interface. This blocks until the socket is closed.
//! There can be different implementations of this function for different socket
//! protocols.
using SocketListenFn = void(const char* exe_name, int fd, Init& init);

} // namespace interfaces

#endif // BITCOIN_INTERFACES_PROCESS_H
