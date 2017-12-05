#include <interfaces/capnp/common-types.h>

#include <clientversion.h>
#include <init.h>
#include <interfaces/capnp/chain.capnp.h>
#include <interfaces/capnp/chain.capnp.proxy-inl.h>
#include <interfaces/capnp/chain.capnp.proxy.h>
#include <interfaces/capnp/common.capnp.h>
#include <interfaces/capnp/common.capnp.proxy-inl.h>
#include <interfaces/capnp/common.capnp.proxy.h>
#include <interfaces/capnp/handler.capnp.h>
#include <interfaces/capnp/handler.capnp.proxy-inl.h>
#include <interfaces/capnp/handler.capnp.proxy.h>
#include <interfaces/capnp/init.capnp.h>
#include <interfaces/capnp/init.capnp.proxy-inl.h>
#include <interfaces/capnp/init.capnp.proxy.h>
#include <interfaces/capnp/node.capnp.h>
#include <interfaces/capnp/node.capnp.proxy-inl.h>
#include <interfaces/capnp/node.capnp.proxy.h>
#include <interfaces/capnp/proxy-inl.h>
#include <interfaces/capnp/wallet.capnp.h>
#include <interfaces/capnp/wallet.capnp.proxy-inl.h>
#include <interfaces/capnp/wallet.capnp.proxy.h>
#include <interfaces/config.h>
#include <interfaces/node.h>
#include <key.h>
#include <net.h>
#include <net_processing.h>
#include <netaddress.h>
#include <netbase.h>
#include <policy/feerate.h>
#include <policy/fees.h>
#include <primitives/transaction.h>
#include <protocol.h>
#include <pubkey.h>
#include <rpc/server.h>
#include <rpc/util.h>
#include <scheduler.h>
#include <script/ismine.h>
#include <script/script.h>
#include <streams.h>
#include <wallet/coincontrol.h>
#include <wallet/wallet.h>

#include <boost/core/explicit_operator_bool.hpp>
#include <boost/move/utility_core.hpp>
#include <boost/optional.hpp>
#include <boost/variant/get.hpp>
#include <capnp/blob.h>
#include <capnp/list.h>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <tuple>
#include <univalue.h>
#include <utility>

namespace interfaces {
namespace capnp {

void BuildMessage(InvokeContext& invoke_context, const UniValue& univalue, messages::UniValue::Builder&& builder)
{
    builder.setType(univalue.getType());
    if (univalue.getType() == UniValue::VARR || univalue.getType() == UniValue::VOBJ) {
        builder.setValue(univalue.write());
    } else {
        builder.setValue(univalue.getValStr());
    }
}

void ReadMessage(InvokeContext& invoke_context, const messages::UniValue::Reader& reader, UniValue& univalue)
{
    if (reader.getType() == UniValue::VARR || reader.getType() == UniValue::VOBJ) {
        if (!univalue.read(ToString(reader.getValue()))) {
            throw std::runtime_error("Could not parse UniValue");
        }
    } else {
        univalue = UniValue(UniValue::VType(reader.getType()), ToString(reader.getValue()));
    }
}

void BuildMessage(InvokeContext& invoke_context,
    const CTxDestination& dest,
    messages::TxDestination::Builder&& builder)
{
    if (const PKHash* pkHash = boost::get<PKHash>(&dest)) {
        builder.setPkHash(ToArray(Serialize(*pkHash)));
    } else if (const ScriptHash* scriptHash = boost::get<ScriptHash>(&dest)) {
        builder.setScriptHash(ToArray(Serialize(*scriptHash)));
    } else if (const WitnessV0ScriptHash* witnessV0ScriptHash = boost::get<WitnessV0ScriptHash>(&dest)) {
        builder.setWitnessV0ScriptHash(ToArray(Serialize(*witnessV0ScriptHash)));
    } else if (const WitnessV0KeyHash* witnessV0KeyHash = boost::get<WitnessV0KeyHash>(&dest)) {
        builder.setWitnessV0KeyHash(ToArray(Serialize(*witnessV0KeyHash)));
    } else if (const WitnessUnknown* witnessUnknown = boost::get<WitnessUnknown>(&dest)) {
        BuildField(TypeList<WitnessUnknown>(), invoke_context, Make<ValueField>(builder.initWitnessUnknown()),
            *witnessUnknown);
    }
}


void BuildMessage(InvokeContext& invoke_context,
    CValidationState const& state,
    messages::ValidationState::Builder&& builder)
{
    int dos = 0;
    if (state.IsValid()) {
        builder.setValid(true);
        assert(!state.IsInvalid());
        assert(!state.IsError());
    } else if (state.IsError()) {
        builder.setError(true);
        assert(!state.IsInvalid());
    } else {
        assert(state.IsInvalid());
    }
    builder.setReason(int32_t(state.GetReason()));
    builder.setRejectCode(state.GetRejectCode());
    const std::string& reject_reason = state.GetRejectReason();
    if (!reject_reason.empty()) {
        builder.setRejectReason(reject_reason);
    }
    const std::string& debug_message = state.GetDebugMessage();
    if (!debug_message.empty()) {
        builder.setDebugMessage(debug_message);
    }
}

void ReadMessage(InvokeContext& invoke_context,
    messages::ValidationState::Reader const& reader,
    CValidationState& state)
{
    if (reader.getValid()) {
        assert(!reader.getError());
        assert(!reader.getReason());
        assert(!reader.getRejectCode());
        assert(!reader.hasRejectReason());
        assert(!reader.hasDebugMessage());
    } else {
        state.Invalid(ValidationInvalidReason(reader.getReason()), false /* ret */, reader.getRejectCode(),
            reader.getRejectReason(), reader.getDebugMessage());
        if (reader.getError()) {
            state.Error({} /* reject reason */);
        }
    }
}

void BuildMessage(InvokeContext& invoke_context, const CKey& key, messages::Key::Builder&& builder)
{
    builder.setSecret(FromBlob(key));
    builder.setIsCompressed(key.IsCompressed());
}

void ReadMessage(InvokeContext& invoke_context, const messages::Key::Reader& reader, CKey& key)
{
    auto secret = reader.getSecret();
    key.Set(secret.begin(), secret.end(), reader.getIsCompressed());
}

void ReadMessage(InvokeContext& invoke_context,
    messages::NodeStats::Reader const& reader,
    std::tuple<CNodeStats, bool, CNodeStateStats>& node_stats)
{
    auto&& node = std::get<0>(node_stats);
    ReadFieldUpdate(TypeList<Decay<decltype(node)>>(), invoke_context, Make<ValueField>(reader), node);
    if ((std::get<1>(node_stats) = reader.hasStateStats())) {
        auto&& state = std::get<2>(node_stats);
        ReadFieldUpdate(
            TypeList<Decay<decltype(state)>>(), invoke_context, Make<ValueField>(reader.getStateStats()), state);
    }
}

void BuildMessage(InvokeContext& invoke_context,
    const CCoinControl& coin_control,
    messages::CoinControl::Builder&& builder)
{
    BuildMessage(invoke_context, coin_control.destChange, builder.initDestChange());
    if (coin_control.m_change_type) {
        builder.setHasChangeType(true);
        builder.setChangeType(static_cast<int>(*coin_control.m_change_type));
    }
    builder.setAllowOtherInputs(coin_control.fAllowOtherInputs);
    builder.setAllowWatchOnly(coin_control.fAllowWatchOnly);
    builder.setOverrideFeeRate(coin_control.fOverrideFeeRate);
    if (coin_control.m_feerate) {
        builder.setFeeRate(ToArray(Serialize(*coin_control.m_feerate)));
    }
    if (coin_control.m_confirm_target) {
        builder.setHasConfirmTarget(true);
        builder.setConfirmTarget(*coin_control.m_confirm_target);
    }
    if (coin_control.m_signal_bip125_rbf) {
        builder.setHasSignalRbf(true);
        builder.setSignalRbf(*coin_control.m_signal_bip125_rbf);
    }
    builder.setFeeMode(int32_t(coin_control.m_fee_mode));
    builder.setMinDepth(coin_control.m_min_depth);
    std::vector<COutPoint> selected;
    coin_control.ListSelected(selected);
    auto builder_selected = builder.initSetSelected(selected.size());
    size_t i = 0;
    for (const COutPoint& output : selected) {
        builder_selected.set(i, ToArray(Serialize(output)));
        ++i;
    }
}

void ReadMessage(InvokeContext& invoke_context,
    const messages::CoinControl::Reader& reader,
    CCoinControl& coin_control)
{
    ReadMessage(invoke_context, reader.getDestChange(), coin_control.destChange);
    if (reader.getHasChangeType()) {
        coin_control.m_change_type = OutputType(reader.getChangeType());
    }
    coin_control.fAllowOtherInputs = reader.getAllowOtherInputs();
    coin_control.fAllowWatchOnly = reader.getAllowWatchOnly();
    coin_control.fOverrideFeeRate = reader.getOverrideFeeRate();
    if (reader.hasFeeRate()) {
        coin_control.m_feerate = Unserialize<CFeeRate>(reader.getFeeRate());
    }
    if (reader.getHasConfirmTarget()) {
        coin_control.m_confirm_target = reader.getConfirmTarget();
    }
    if (reader.getHasSignalRbf()) {
        coin_control.m_signal_bip125_rbf = reader.getSignalRbf();
    }
    coin_control.m_fee_mode = FeeEstimateMode(reader.getFeeMode());
    coin_control.m_min_depth = reader.getMinDepth();
    for (const auto output : reader.getSetSelected()) {
        coin_control.Select(Unserialize<COutPoint>(output));
    }
}

bool CustomHasValue(InvokeContext& invoke_context, const Coin& coin)
{
    // Spent coins cannot be serialized due to an assert in Coin::Serialize.
    return !coin.IsSpent();
}
} // namespace capnp
} // namespace interfaces
