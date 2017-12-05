#include <interfaces/config.h>

#include <interfaces/capnp/init.h>
#include <interfaces/wallet.h>

namespace interfaces {

const Config g_config = {
    "bitcoin-wallet", &capnp::Listen, &capnp::Connect, nullptr /* MakeNode */, &MakeWalletClient,
};

} // namespace interfaces
