#ifndef BITCOIN_INTERFACES_CAPNP_WALLET_TYPES_H
#define BITCOIN_INTERFACES_CAPNP_WALLET_TYPES_H

#include <interfaces/capnp/common-types.h>

class CCoinControl;
class CKey;

namespace interfaces {
namespace capnp {
void CustomBuildMessage(InvokeContext& invoke_context,
    const CTxDestination& dest,
    messages::TxDestination::Builder&& builder);
void CustomReadMessage(InvokeContext& invoke_context,
    const messages::TxDestination::Reader& reader,
    CTxDestination& dest);
void CustomBuildMessage(InvokeContext& invoke_context, const CKey& key, messages::Key::Builder&& builder);
void CustomReadMessage(InvokeContext& invoke_context, const messages::Key::Reader& reader, CKey& key);
void CustomBuildMessage(InvokeContext& invoke_context,
    const CCoinControl& coin_control,
    messages::CoinControl::Builder&& builder);
void CustomReadMessage(InvokeContext& invoke_context,
    const messages::CoinControl::Reader& reader,
    CCoinControl& coin_control);
} // namespace capnp
} // namespace interfaces

#endif // BITCOIN_INTERFACES_CAPNP_WALLET_TYPES_H
