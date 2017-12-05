#include <interface/config.h>

#include <interface/capnp/init.h>

namespace interface {

const Config g_config = {
    "bitcoin-gui", &capnp::Listen, &capnp::Connect, nullptr /* MakeNode */, nullptr /* MakeWalletClient */,
};

} // namespace interface
