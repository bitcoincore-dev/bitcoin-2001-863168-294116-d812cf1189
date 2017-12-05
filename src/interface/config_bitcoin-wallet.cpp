#include <interface/config.h>

#include <interface/capnp/init.h>
#include <interface/wallet.h>

namespace interface {

const Config g_config = {
    "bitcoin-wallet", &capnp::Listen, &capnp::Connect, nullptr /* MakeNode */, &MakeWalletClient,
};

} // namespace interface
