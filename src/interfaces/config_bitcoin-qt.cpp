#include <interfaces/config.h>

#include <interfaces/node.h>
#include <interfaces/wallet.h>

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

namespace interfaces {

const Config g_config = {
    "bitcoin-qt" /* exe_name */,
    nullptr /* log_suffix */,
    nullptr /* listen */,
    nullptr /* connect */,
    &MakeNode,
#if ENABLE_WALLET
    &MakeWalletClient,
#else
    nullptr,
#endif
};

} // namespace interfaces
