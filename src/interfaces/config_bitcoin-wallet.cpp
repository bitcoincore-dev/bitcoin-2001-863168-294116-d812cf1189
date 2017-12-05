#include <interfaces/config.h>

#include <interfaces/capnp/init.h>
#include <interfaces/wallet.h>

namespace interfaces {

const Config g_config = {
    "bitcoin-wallet" /* exe_name */,
    ".wallet" /* log_suffix */,
    ProcessSpawn,
    ProcessServe,
    &capnp::SocketConnect,
    &capnp::SocketServe,
    nullptr /* make_node */,
    &MakeWalletClient,
};

} // namespace interfaces
