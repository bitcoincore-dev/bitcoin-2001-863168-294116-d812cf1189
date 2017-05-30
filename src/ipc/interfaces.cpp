#include "ipc/interfaces.h"

#include "ipc/local/interfaces.h"

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

namespace ipc {

std::unique_ptr<Chain> MakeChain(Protocol protocol)
{
    if (protocol == LOCAL) {
        return local::MakeChain();
    }
    return {};
}

std::unique_ptr<Chain::Client> MakeChainClient(Protocol protocol, Chain& chain, ChainClientOptions options)
{
    if (protocol == LOCAL && options.type == ChainClientOptions::WALLET) {
#ifdef ENABLE_WALLET
        return local::MakeWalletClient(chain, std::move(options));
#endif
    }
    return {};
}

} // namespace ipc
