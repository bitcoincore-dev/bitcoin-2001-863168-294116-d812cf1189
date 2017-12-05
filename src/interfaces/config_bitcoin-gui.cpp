#include <interfaces/config.h>

#include <interfaces/capnp/ipc.h>

namespace interfaces {

const Config g_config = {
    "bitcoin-gui" /* exe_name */,
    ".gui" /* log_suffix */,
    &MakeIpcProcess,
    &capnp::MakeCapnpProtocol,
    nullptr /* make_node */,
    nullptr /* make_wallet_client */,
};

} // namespace interfaces
