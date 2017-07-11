#ifndef BITCOIN_IPC_CAPNP_INTERFACES_H
#define BITCOIN_IPC_CAPNP_INTERFACES_H

#include <memory>

namespace ipc {

class Node;

namespace capnp {

//! Return capnp node client communicating across given socket descriptor.
std::unique_ptr<Node> MakeNode(int fd);

} // namespace capnp

} // namespace ipc

#endif // BITCOIN_IPC_CAPNP_INTERFACES_H
