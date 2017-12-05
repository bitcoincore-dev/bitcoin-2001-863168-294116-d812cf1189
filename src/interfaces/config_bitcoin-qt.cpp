#include <interfaces/config.h>

#include <interfaces/node.h>
#include <interfaces/wallet.h>

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

namespace interfaces {

const Config g_config = {
    "bitcoin-qt", nullptr /* Listen */, nullptr /* Connect */, &MakeNode,
#if ENABLE_WALLET
    &MakeWalletClient,
#else
    nullptr,
#endif
};

} // namespace interfaces
