#include <interfaces/config.h>

#include <interfaces/chain.h>
#include <interfaces/node.h>
#include <interfaces/wallet.h>

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

namespace interfaces {

const Config g_config = {
    "bitcoind" /* exe_name */,
    nullptr /* log_suffix */,
    nullptr /* make_ipc_process */,
    nullptr /* make_ipc_protocol */,
    &MakeNode,
#if ENABLE_WALLET
    &MakeWalletClient,
#else
    nullptr,
#endif
};

} // namespace interfaces
