#include <interfaces/config.h>

#include <interfaces/capnp/init.h>

namespace interfaces {

const Config g_config = {
    "bitcoin-gui", &capnp::Listen, &capnp::Connect, nullptr /* MakeNode */, nullptr /* MakeWalletClient */,
};

} // namespace interfaces
