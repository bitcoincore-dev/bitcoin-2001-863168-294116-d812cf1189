#include <interfaces/capnp/common.capnp.proxy-types.h>

namespace interfaces {
namespace capnp {

void CustomBuildMessage(InvokeContext& invoke_context, const UniValue& univalue, messages::UniValue::Builder&& builder)
{
    builder.setType(univalue.getType());
    if (univalue.getType() == UniValue::VARR || univalue.getType() == UniValue::VOBJ) {
        builder.setValue(univalue.write());
    } else {
        builder.setValue(univalue.getValStr());
    }
}

void CustomReadMessage(InvokeContext& invoke_context, const messages::UniValue::Reader& reader, UniValue& univalue)
{
    if (reader.getType() == UniValue::VARR || reader.getType() == UniValue::VOBJ) {
        if (!univalue.read(ToString(reader.getValue()))) {
            throw std::runtime_error("Could not parse UniValue");
        }
    } else {
        univalue = UniValue(UniValue::VType(reader.getType()), ToString(reader.getValue()));
    }
}

void BuildGlobalArgs(InvokeContext& invoke_context, messages::GlobalArgs::Builder&& builder)
{

    const auto& args = static_cast<const GlobalArgs&>(::gArgs);
    LOCK(args.cs_args);
    BuildField(TypeList<GlobalArgs>(), invoke_context, Make<ValueField>(builder), args);
}

void ReadGlobalArgs(InvokeContext& invoke_context, const messages::GlobalArgs::Reader& reader)
{
    auto& args = static_cast<GlobalArgs&>(::gArgs);
    LOCK(args.cs_args);
    ReadFieldUpdate(TypeList<GlobalArgs>(), invoke_context, Make<ValueField>(reader), args);
}

std::string GlobalArgsNetwork()
{
    auto& args = static_cast<GlobalArgs&>(::gArgs);
    LOCK(args.cs_args);
    return args.m_network;
}

} // namespace capnp
} // namespace interfaces
