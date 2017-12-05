#include <interfaces/config.h>

#include <interfaces/capnp/ipc.h>
#include <interfaces/node.h>

namespace interfaces {

const Config g_config = {
    "bitcoin-node" /* exe_name */,
    nullptr /* log_suffix */,
    &MakeIpcProcess,
    &capnp::MakeCapnpProtocol,
    &MakeNode,
    nullptr /* make_wallet_client */,
};

} // namespace interfaces
