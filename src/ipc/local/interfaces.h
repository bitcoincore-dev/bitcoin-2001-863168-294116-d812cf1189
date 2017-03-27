#ifndef BITCOIN_IPC_LOCAL_INTERFACES_H
#define BITCOIN_IPC_LOCAL_INTERFACES_H

#include <memory>

namespace ipc {

class Node;

namespace local {

//! Return local, in-process implementation of ipc::Node interface.
std::unique_ptr<Node> MakeNode();

} // namespace local

} // namespace ipc

#endif // BITCOIN_IPC_LOCAL_INTERFACES_H
