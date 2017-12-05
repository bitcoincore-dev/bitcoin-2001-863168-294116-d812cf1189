#include <interfaces/config.h>

#include <interfaces/capnp/init.h>
#include <interfaces/node.h>

namespace interfaces {

const Config g_config = {
    "bitcoin-node", &capnp::Listen, &capnp::Connect, &MakeNode, nullptr /* MakeWalletClient */,
};

} // namespace interfaces
