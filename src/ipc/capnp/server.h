#ifndef BITCOIN_IPC_CAPNP_SERVER_H
#define BITCOIN_IPC_CAPNP_SERVER_H

#include <capnp/rpc.h>
#include <memory>

namespace ipc {

class Node;

namespace capnp {

// Start server forwarding requests on socket file descriptor to ipc::Node
// interface. This blocks until node the is shut down.
void StartNodeServer(std::unique_ptr<ipc::Node> node, int fd);

} // namespace capnp
} // namespace ipc

#endif // BITCOIN_IPC_CAPNP_SERVER_H
