#include <interface/config.h>

#include <interface/node.h>
#include <interface/wallet.h>

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

namespace interface {

const Config g_config = {
    "bitcoind", nullptr /* Listen */, nullptr /* Connect */, &MakeNode,
#if ENABLE_WALLET
    &MakeWalletClient,
#else
    nullptr,
#endif
};

} // namespace interface
