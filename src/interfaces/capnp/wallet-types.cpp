#include <interfaces/capnp/wallet-types.h>

#include <interfaces/capnp/common-types.h>
#include <interfaces/capnp/proxy-inl.h>
#include <interfaces/capnp/wallet.capnp.proxy-inl.h>

namespace interfaces {
namespace capnp {

void ReadMessage(InvokeContext& invoke_context, const messages::TxDestination::Reader& reader, CTxDestination& dest)
{
    if (reader.hasPkHash()) {
        dest = Unserialize<PKHash>(reader.getPkHash());
    } else if (reader.hasScriptHash()) {
        dest = Unserialize<ScriptHash>(reader.getScriptHash());
    } else if (reader.hasWitnessV0ScriptHash()) {
        dest = Unserialize<WitnessV0ScriptHash>(reader.getWitnessV0ScriptHash());
    } else if (reader.hasWitnessV0KeyHash()) {
        dest = Unserialize<WitnessV0KeyHash>(reader.getWitnessV0KeyHash());
    } else if (reader.hasWitnessUnknown()) {
        ReadFieldUpdate(TypeList<WitnessUnknown>(), invoke_context, Make<ValueField>(reader.getWitnessUnknown()),
            boost::get<WitnessUnknown>(dest));
    }
}

} // namespace capnp
} // namespace interfaces
