#ifndef BITCOIN_INTERFACES_CAPNP_MESSAGES_IMPL_H
#define BITCOIN_INTERFACES_CAPNP_MESSAGES_IMPL_H

#include <addrdb.h>
#include <chainparams.h>
#include <clientversion.h>
#include <coins.h>
#include <interfaces/capnp/messages.capnp.h>
#include <interfaces/capnp/messages.capnp.proxy.h>
#include <interfaces/capnp/messages.h>
#include <interfaces/capnp/util.h>
#include <interfaces/node.h>
#include <interfaces/wallet.h>
#include <net.h>
#include <net_processing.h>
#include <netbase.h>
#include <pubkey.h>
#include <rpc/server.h>
#include <script/standard.h>
#include <serialize.h>
#include <validation.h>
#include <wallet/coincontrol.h>
#include <wallet/wallet.h>

#include <boost/core/underlying_type.hpp>
#include <boost/exception/diagnostic_information.hpp>
#include <capnp/capability.h>
#include <capnp/common.h>
#include <capnp/list.h>
#include <capnp/pretty-print.h>
#include <capnp/schema.h>
#include <consensus/validation.h>
#include <future>
#include <iostream>
#include <kj/async-io.h>
#include <kj/async.h>
#include <kj/common.h>
#include <kj/debug.h>
#include <kj/string.h>
#include <list>
#include <mutex>
#include <streams.h>
#include <string>
#include <support/allocators/secure.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <thread>
#include <tuple>
#include <unistd.h>
#include <univalue.h>
#include <vector>

class CCoinControl;
class CKey;
class CNodeStats;
class proxyType;
struct CNodeStateStats;
struct CRecipient;

namespace interfaces {
namespace capnp {

//!@{
//! Functions to serialize / deserialize bitcoin objects that don't
//! already provide their own serialization.
void BuildMessage(const UniValue& univalue, messages::UniValue::Builder&& builder);
void ReadMessage(InvokeContext& invoke_context, const messages::UniValue::Reader& reader, UniValue& univalue);
void BuildMessage(const CTxDestination& dest, messages::TxDestination::Builder&& builder);
void ReadMessage(InvokeContext& invoke_context, const messages::TxDestination::Reader& reader, CTxDestination& dest);
void BuildMessage(CValidationState const& state, messages::ValidationState::Builder&& builder);
void ReadMessage(InvokeContext& invoke_context,
    messages::ValidationState::Reader const& reader,
    CValidationState& state);
void BuildMessage(const CKey& key, messages::Key::Builder&& builder);
void ReadMessage(InvokeContext& invoke_context, const messages::Key::Reader& reader, CKey& key);
void ReadMessage(InvokeContext& invoke_context,
    messages::NodeStats::Reader const& reader,
    std::tuple<CNodeStats, bool, CNodeStateStats>& node_stats);
void BuildMessage(const CCoinControl& coin_control, messages::CoinControl::Builder&& builder);
void ReadMessage(InvokeContext& invoke_context,
    const messages::CoinControl::Reader& reader,
    CCoinControl& coin_control);
//!@}

//! Convert kj::StringPtr to std::string.
inline std::string ToString(const kj::StringPtr& str) { return {str.cStr(), str.size()}; }

//! Convert kj::ArrayPtr to std::string.
inline std::string ToString(const kj::ArrayPtr<const kj::byte>& data)
{
    return {reinterpret_cast<const char*>(data.begin()), data.size()};
}

//! Convert array object to kj::ArrayPtr.
template <typename Array>
inline kj::ArrayPtr<const kj::byte> ToArray(const Array& array)
{
    return {reinterpret_cast<const kj::byte*>(array.data()), array.size()};
}

//! Convert base_blob to kj::ArrayPtr.
template <typename Blob>
inline kj::ArrayPtr<const kj::byte> FromBlob(const Blob& blob)
{
    return {blob.begin(), blob.size()};
}

//! Convert kj::ArrayPtr to base_blob
template <typename Blob>
inline Blob ToBlob(kj::ArrayPtr<const kj::byte> data)
{
    // TODO: Avoid temp vector.
    return Blob(std::vector<unsigned char>(data.begin(), data.begin() + data.size()));
}

//! Serialize bitcoin value.
template <typename T>
CDataStream Serialize(const T& value)
{
    CDataStream stream(SER_NETWORK, CLIENT_VERSION);
    value.Serialize(stream);
    return stream;
}

//! Deserialize bitcoin value.
template <typename T>
T Unserialize(T& value, const kj::ArrayPtr<const kj::byte>& data)
{
    // Could optimize, it unnecessarily copies the data into a temporary vector.
    CDataStream stream(reinterpret_cast<const char*>(data.begin()), reinterpret_cast<const char*>(data.end()),
        SER_NETWORK, CLIENT_VERSION);
    value.Unserialize(stream);
    return value;
}

//! Deserialize bitcoin value.
template <typename T>
T Unserialize(const kj::ArrayPtr<const kj::byte>& data)
{
    T value;
    Unserialize(value, data);
    return value;
}

#if 0
static inline void ReadMessage(InvokeContext& invoke_context, const ::capnp::List<messages::NodeStats>::Reader& reader, Node::NodesStats& stats)
{
    stats.clear();
    stats.reserve(reader.size());
    for (const auto& resultNodeStats : reader) {
        stats.emplace_back(CNodeStats(), false, CNodeStateStats());
        ReadMessage(invoke_context, resultNodeStats, std::get<0>(stats.back()));
        if (resultNodeStats.hasStateStats()) {
            std::get<1>(stats.back()) = true;
            ReadMessage(invoke_context, resultNodeStats.getStateStats(), std::get<2>(stats.back()));
        }
    }
}
#endif

template <typename LocalType, typename Reader, typename Value>
void ReadFieldUpdate(TypeList<LocalType>,
    InvokeContext& invoke_context,
    Reader&& reader,
    Value&& value,
    decltype(ReadMessage(invoke_context, reader.get(), value))* enable = nullptr)
{
    ReadMessage(invoke_context, reader.get(), value);
}

template <typename T>
using Deserializable = std::is_constructible<T, ::deserialize_type, ::CDataStream&>;

template <typename T>
struct Unserializable
{
private:
    template <typename C>
    static std::true_type test(decltype(std::declval<C>().Unserialize(std::declval<C&>()))*);
    template <typename>
    static std::false_type test(...);

public:
    static constexpr bool value = decltype(test<T>(nullptr))::value;
};

template <typename T>
struct Serializable
{
private:
    template <typename C>
    static std::true_type test(decltype(std::declval<C>().Serialize(std::declval<C&>()))*);
    template <typename>
    static std::false_type test(...);

public:
    static constexpr bool value = decltype(test<T>(nullptr))::value;
};

template <typename LocalType, typename Input, typename Emplace>
void ReadFieldNew(TypeList<LocalType>,
    InvokeContext& invoke_context,
    Input&& input,
    Emplace&& emplace,
    typename std::enable_if<Deserializable<LocalType>::value>::type* enable = nullptr)
{
    if (!input.has()) return;
    auto data = input.get();
    // Note: stream copy here is unnecessary, and can be avoided in the future
    // when `VectorReader` from #12254 is added.
    CDataStream stream(CharCast(data.begin()), CharCast(data.end()), SER_NETWORK, CLIENT_VERSION);
    emplace(deserialize, stream);
}

template <typename LocalType, typename Input, typename Value>
void ReadFieldUpdate(TypeList<LocalType>,
    InvokeContext& invoke_context,
    Input&& input,
    Value& value,
    typename std::enable_if<Unserializable<Value>::value>::type* enable = nullptr)
{
    if (!input.has()) return;
    auto data = input.get();
    // Note: stream copy here is unnecessary, and can be avoided in the future
    // when `VectorReader` from #12254 is added.
    CDataStream stream(CharCast(data.begin()), CharCast(data.end()), SER_NETWORK, CLIENT_VERSION);
    value.Unserialize(stream);
}

template <typename Input, typename Emplace>
void ReadFieldNew(TypeList<SecureString>, InvokeContext& invoke_context, Input&& input, Emplace&& emplace)
{
    auto data = input.get();
    // Copy input into SecureString. Caller needs to be responsible for calling
    // memory_cleanse on the input.
    emplace(CharCast(data.begin()), data.size());
}

template <typename Value, typename Output>
void BuildField(TypeList<SecureString>, Priority<1>, InvokeContext& invoke_context, Value&& str, Output&& output)
{
    auto result = output.init(str.size());
    // Copy SecureString into output. Caller needs to be responsible for calling
    // memory_cleanse later on the output after it is sent.
    memcpy(result.begin(), str.data(), str.size());
}

template <typename Input, typename Value>
void ReadFieldUpdate(TypeList<CReserveScript>, InvokeContext& invoke_context, Input&& input, Value&& value)
{
    ReadFieldUpdate(TypeList<decltype(value.reserveScript)>(), invoke_context, input, value.reserveScript);
}

template <typename Value, typename Output>
void BuildField(TypeList<std::tuple<CNodeStats, bool, CNodeStateStats>>,
    Priority<1>,
    InvokeContext& invoke_context,
    Value&& stats,
    Output&& output)
{
    // FIXME should pass message_builder instead of builder below to avoid
    // calling output.set twice Need ValueBuilder class analogous to
    // ValueReader for this
    BuildField(TypeList<CNodeStats>(), BuildFieldPriority(), invoke_context, std::get<0>(stats), output);
    if (std::get<1>(stats)) {
        auto message_builder = output.init();
        using Accessor = ProxyStruct<messages::NodeStats>::StateStatsAccessor;
        StructField<Accessor, messages::NodeStats::Builder> field_output{message_builder};
        BuildField(
            TypeList<CNodeStateStats>(), BuildFieldPriority(), invoke_context, std::get<2>(stats), field_output);
    }
}

template <typename LocalType, typename Value, typename Output>
void BuildField(TypeList<LocalType>,
    Priority<2>,
    InvokeContext& invoke_context,
    Value&& value,
    Output&& output,
    decltype(BuildMessage(value, output.init()))* enable = nullptr)
{
    BuildMessage(value, output.init());
}

template <typename LocalType, typename Value, typename Output>
void BuildField(TypeList<LocalType>,
    Priority<1>,
    InvokeContext& invoke_context,
    Value&& value,
    Output&& output,
    typename std::enable_if<Serializable<
        typename std::remove_cv<typename std::remove_reference<Value>::type>::type>::value>::type* enable = nullptr)
{
    CDataStream stream(SER_NETWORK, CLIENT_VERSION);
    value.Serialize(stream);
    auto result = output.init(stream.size());
    memcpy(result.begin(), stream.data(), stream.size());
}

template <typename Value, typename Output>
void BuildField(TypeList<CReserveScript>, Priority<1>, InvokeContext& invoke_context, Value&& script, Output&& output)
{
    CDataStream stream(SER_NETWORK, CLIENT_VERSION);
    script.reserveScript.Serialize(stream);
    auto result = output.init(stream.size());
    memcpy(result.begin(), stream.data(), stream.size());
}

template <typename InvokeContext>
static inline ::capnp::Void BuildPrimitive(InvokeContext& invoke_context, RPCTimerInterface*, TypeList<::capnp::Void>)
{
    return {};
}

template <typename Accessor, typename ServerContext, typename Fn, typename... Args>
auto PassField(TypeList<>, ServerContext& server_context, const Fn& fn, Args&&... args) ->
    typename std::enable_if<std::is_same<decltype(Accessor::get(server_context.call_context.getParams())),
        messages::GlobalArgs::Reader>::value>::type
{
    ReadGlobalArgs(server_context, Accessor::get(server_context.call_context.getParams()));
    return fn.invoke(server_context, std::forward<Args>(args)...);
}

template <typename CallContext, typename Server>
class AsyncClosure
{

    CallContext call_context;
    Server& server;
};

// Invoke promise1, then promise2, and return result of promise2.y
template <typename T, typename U>
kj::Promise<U> JoinPromises(kj::Promise<T>&& prom1, kj::Promise<U>&& prom2)
{
    return prom1.then(kj::mvCapture(prom2, [](kj::Promise<U> prom2) { return prom2; }));
}

template <typename Output>
void BuildField(TypeList<>,
    Priority<1>,
    InvokeContext& invoke_context,
    Output&& output,
    typename std::enable_if<std::is_same<decltype(output.init()), messages::GlobalArgs::Builder>::value>::type*
        enable = nullptr)
{
    BuildGlobalArgs(invoke_context, output.init());
}

template <typename Accessor, typename ServerContext, typename Fn, typename... Args>
void PassField(TypeList<int, const char* const*>, ServerContext& server_context, const Fn& fn, Args&&... args)
{
    const auto& params = server_context.call_context.getParams();
    const auto& value = Accessor::get(params);
    std::vector<const char*> argv(value.size());
    size_t i = 0;
    for (const auto arg : value) {
        argv[i] = arg.cStr();
        ++i;
    }
    return fn.invoke(server_context, std::forward<Args>(args)..., argv.size(), argv.data());
}

template <typename Output>
void BuildField(TypeList<int, const char* const*>,
    Priority<1>,
    InvokeContext& invoke_context,
    int argc,
    const char* const* argv,
    Output&& output)
{
    auto args = output.init(argc);
    for (int i = 0; i < argc; ++i) {
        args.set(i, argv[i]);
    }
}

} // namespace capnp

} // namespace interfaces

#endif // BITCOIN_INTERFACES_CAPNP_MESSAGES_IMPL_H
