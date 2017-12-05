#include <interfaces/config.h>

#include <interfaces/capnp/init.h>

namespace interfaces {

const Config g_config = {
    "bitcoin-gui" /* exe_name */,
    ".gui" /* log_suffix */,
    &capnp::Listen,
    &capnp::Connect,
    nullptr /* make_node */,
    nullptr /* make_wallet_client */,
};

} // namespace interfaces
