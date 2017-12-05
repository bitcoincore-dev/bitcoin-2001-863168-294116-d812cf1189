#ifndef BITCOIN_INTERFACE_CAPNP_INIT_H
#define BITCOIN_INTERFACE_CAPNP_INIT_H

#include <interface/init.h>

namespace interface {
namespace capnp {

ListenFn Listen;
ConnectFn Connect;

} // namespace capnp
} // namespace interface

#endif // BITCOIN_INTERFACE_CAPNP_INIT_H
