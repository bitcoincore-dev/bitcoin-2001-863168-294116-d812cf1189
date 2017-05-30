#ifndef BITCOIN_IPC_LOCAL_INTERFACES_H
#define BITCOIN_IPC_LOCAL_INTERFACES_H

#include "ipc/interfaces.h"

#include <memory>

namespace ipc {
namespace local {

//! Return local, in-process implementation of ipc::Chain interface.
std::unique_ptr<Chain> MakeChain();

//! Return local, in-process implementation of ipc::Chain::Client interface.
std::unique_ptr<Chain::Client> MakeWalletClient(Chain& chain, ChainClientOptions options);

} // namespace local
} // namespace ipc

#endif // BITCOIN_IPC_LOCAL_INTERFACES_H
