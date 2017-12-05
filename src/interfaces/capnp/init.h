#ifndef BITCOIN_INTERFACES_CAPNP_INIT_H
#define BITCOIN_INTERFACES_CAPNP_INIT_H

#include <interfaces/process.h>

namespace interfaces {
namespace capnp {

SocketServeFn SocketServe;
SocketConnectFn SocketConnect;

} // namespace capnp
} // namespace interfaces

#endif // BITCOIN_INTERFACES_CAPNP_INIT_H
