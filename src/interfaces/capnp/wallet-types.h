#ifndef BITCOIN_INTERFACES_CAPNP_WALLET_TYPES_H
#define BITCOIN_INTERFACES_CAPNP_WALLET_TYPES_H

#include <interfaces/capnp/wallet.capnp.h>
#include <script/standard.h>

namespace interfaces {
namespace capnp {
struct InvokeContext;

void ReadMessage(InvokeContext& invoke_context, const messages::TxDestination::Reader& reader, CTxDestination& dest);

} // namespace capnp
} // namespace interfaces

#endif // BITCOIN_INTERFACES_CAPNP_WALLET_TYPES_H
