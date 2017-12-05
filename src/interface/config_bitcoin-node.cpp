#include <interface/config.h>

#include <interface/capnp/init.h>
#include <interface/node.h>

namespace interface {

const Config g_config = {
    "bitcoin-node", &capnp::Listen, &capnp::Connect, &MakeNode, nullptr /* MakeWalletClient */,
};

} // namespace interface
