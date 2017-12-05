#include <interfaces/config.h>

#include <interfaces/capnp/init.h>
#include <interfaces/node.h>

namespace interfaces {

const Config g_config = {
    "bitcoin-node" /* exe_name */,
    nullptr /* log_suffix */,
    ProcessSpawn,
    ProcessServe,
    &capnp::SocketConnect,
    &capnp::SocketListen,
    &MakeNode,
    nullptr /* make_wallet_client */,
};

} // namespace interfaces
