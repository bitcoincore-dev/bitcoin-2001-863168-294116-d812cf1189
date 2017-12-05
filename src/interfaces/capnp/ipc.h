#ifndef BITCOIN_INTERFACES_CAPNP_IPC_H
#define BITCOIN_INTERFACES_CAPNP_IPC_H

#include <interfaces/ipc.h>

namespace interfaces {
namespace capnp {

MakeIpcProtocolFn MakeCapnpProtocol;

} // namespace capnp
} // namespace interfaces

#endif // BITCOIN_INTERFACES_CAPNP_IPC_H
