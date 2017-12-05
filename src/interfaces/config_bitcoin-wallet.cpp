#include <interfaces/config.h>

#include <interfaces/capnp/init.h>
#include <interfaces/wallet.h>

namespace interfaces {

const Config g_config = {
    "bitcoin-wallet" /* exe_name */,
    ".wallet" /* log_suffix */,
    &capnp::Listen,
    &capnp::Connect,
    nullptr /* make_node */,
    &MakeWalletClient,
};

} // namespace interfaces
