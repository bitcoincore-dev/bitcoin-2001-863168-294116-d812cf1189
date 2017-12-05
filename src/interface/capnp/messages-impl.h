#ifndef BITCOIN_INTERFACE_CAPNP_MESSAGES_IMPL_H
#define BITCOIN_INTERFACE_CAPNP_MESSAGES_IMPL_H

#include <addrdb.h>
#include <chainparams.h>
#include <clientversion.h>
#include <coins.h>
#include <interface/capnp/messages.capnp.h>
#include <interface/capnp/messages.h>
#include <interface/capnp/util.h>
#include <interface/node.h>
#include <interface/wallet.h>
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

namespace interface {
namespace capnp {

//!@{
//! Functions to serialize / deserialize bitcoin objects that don't
//! already provide their own serialization.
void BuildMessage(const UniValue& univalue, messages::UniValue::Builder&& builder);
void ReadMessage(const messages::UniValue::Reader& reader, UniValue& univalue);
void BuildMessage(const CTxDestination& dest, messages::TxDestination::Builder&& builder);
void ReadMessage(const messages::TxDestination::Reader& reader, CTxDestination& dest);
void BuildMessage(CValidationState const& state, messages::ValidationState::Builder&& builder);
void ReadMessage(messages::ValidationState::Reader const& reader, CValidationState& state);
void BuildMessage(const CKey& key, messages::Key::Builder&& builder);
void ReadMessage(const messages::Key::Reader& reader, CKey& key);
void ReadMessage(messages::NodeStats::Reader const& reader, std::tuple<CNodeStats, bool, CNodeStateStats>& node_stats);
void BuildMessage(const CCoinControl& coin_control, messages::CoinControl::Builder&& builder);
void ReadMessage(const messages::CoinControl::Reader& reader, CCoinControl& coin_control);
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
static inline void ReadMessage(const ::capnp::List<messages::NodeStats>::Reader& reader, Node::NodesStats& stats)
{
    stats.clear();
    stats.reserve(reader.size());
    for (const auto& resultNodeStats : reader) {
        stats.emplace_back(CNodeStats(), false, CNodeStateStats());
        ReadMessage(resultNodeStats, std::get<0>(stats.back()));
        if (resultNodeStats.hasStateStats()) {
            std::get<1>(stats.back()) = true;
            ReadMessage(resultNodeStats.getStateStats(), std::get<2>(stats.back()));
        }
    }
}
#endif

template <typename Param, typename Reader, typename Value>
void ReadFieldUpdate(TypeList<Param>,
    Reader&& reader,
    Value&& value,
    decltype(ReadMessage(reader.get(), value))* enable = nullptr)
{
    ReadMessage(reader.get(), value);
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

template <typename Param, typename Reader, typename Emplace>
void ReadFieldNew(TypeList<Param>,
    Reader&& reader,
    Emplace&& emplace,
    typename std::enable_if<Deserializable<Param>::value>::type* enable = nullptr)
{
    auto data = reader.get();
    // FIXME deserialize directly from byte array, avoiding copy.
    CDataStream stream(CharCast(data.begin()), CharCast(data.end()), SER_NETWORK, CLIENT_VERSION);
    emplace(deserialize, stream);
}

template <typename Param, typename Reader, typename Value>
void ReadFieldUpdate(TypeList<Param>,
    Reader&& reader,
    Value& value,
    typename std::enable_if<Unserializable<Value>::value>::type* enable = nullptr)
{
    auto data = reader.get();
    // FIXME unserialize directly from byte array, avoiding copy.
    CDataStream stream(CharCast(data.begin()), CharCast(data.end()), SER_NETWORK, CLIENT_VERSION);
    value.Unserialize(stream);
}


template <typename Reader, typename Emplace>
void ReadFieldNew(TypeList<SecureString>, Reader&& reader, Emplace&& emplace)
{
    auto data = reader.get();
    // FIXME maybe secure erase reader data
    emplace(CharCast(data.begin()), data.size());
}

template <typename Reader, typename Value>
void ReadFieldUpdate(TypeList<CReserveScript>, Reader&& reader, Value&& value)
{
    ReadFieldUpdate(TypeList<decltype(value.reserveScript)>(), reader, value.reserveScript);
}

template <typename FUCK, typename Value, typename Builder>
void BuildField(TypeList<SecureString>, TypeList<FUCK>, Priority<1>, EventLoop& loop, Value&& str, Builder&& builder)
{
    auto result = builder.set(str.size());
    // FIXME: should call memory_cleanse on this pointer after outgoing message is sent. similarly should call
    // memory_cleanse after readfield is called for incoming messages.
    memcpy(result.begin(), str.data(), str.size());
}

template <typename FUCK, typename Value, typename Builder>
void BuildField(TypeList<std::tuple<CNodeStats, bool, CNodeStateStats>>,
    TypeList<FUCK>,
    Priority<1>,
    EventLoop& loop,
    Value&& stats,
    Builder&& builder)
{
    // FIXME should pass message_builder instead of builder below to avoid
    // calling builder.set twice Need ValueBuilder class analogous to
    // ValueReader for this
    BuildField(TypeList<CNodeStats>(), TypeList<FUCK>(), BuildFieldPriority(), loop, std::get<0>(stats), builder);
    if (std::get<1>(stats)) {
        auto message_builder = builder.set();
        auto accessor = ProxyStruct<messages::NodeStats>::getStateStats();
        FieldBuilder<messages::NodeStats::Builder, decltype(accessor)> field_builder{message_builder, accessor};
        BuildField(
            TypeList<CNodeStateStats>(), TypeList<FUCK>(), BuildFieldPriority(), loop, std::get<2>(stats), field_builder);
    }
}

template <typename FUCK, typename Param, typename Value, typename Builder>
void BuildField(TypeList<Param>,
    TypeList<FUCK>,
    Priority<2>,
    EventLoop& loop,
    Value&& value,
    Builder&& builder,
    decltype(BuildMessage(value, builder.set()))* enable = nullptr)
{
    BuildMessage(value, builder.set());
}


template <typename FUCK, typename Param, typename Value, typename Builder>
void BuildField(TypeList<Param>,
    TypeList<FUCK>,
    Priority<1>,
    EventLoop& loop,
    Value&& value,
    Builder&& builder,
    typename std::enable_if<Serializable<
        typename std::remove_cv<typename std::remove_reference<Value>::type>::type>::value>::type* enable = nullptr)
{
    CDataStream stream(SER_NETWORK, CLIENT_VERSION);
    value.Serialize(stream);
    auto result = builder.set(stream.size());
    memcpy(result.begin(), stream.data(), stream.size());
}

template <typename FUCK, typename Value, typename Builder>
void BuildField(TypeList<CReserveScript>,
    TypeList<FUCK>,
    Priority<1>,
    EventLoop& loop,
    Value&& script,
    Builder&& builder)
{
    CDataStream stream(SER_NETWORK, CLIENT_VERSION);
    script.reserveScript.Serialize(stream);
    auto result = builder.set(stream.size());
    memcpy(result.begin(), stream.data(), stream.size());
}

// FIXME should be nonrvalue reference
messages::Chain::Client BuildPrimitive(EventLoop& loop, interface::Chain&& impl, TypeList<messages::Chain::Client>);

// FIXME should be nonrvalue reference
messages::ChainNotifications::Client BuildPrimitive(EventLoop& loop,
    interface::Chain::Notifications&& impl,
    TypeList<messages::ChainNotifications::Client>);

static inline ::capnp::Void BuildPrimitive(EventLoop& loop, RPCTimerInterface*, TypeList<::capnp::Void>) { return {}; }

struct Args : public ArgsManager
{
    using ArgsManager::cs_args;
    using ArgsManager::mapArgs;
    using ArgsManager::mapMultiArgs;
};

template <typename FUCK, typename Fn, typename Reader, typename... FnParams>
void PassField(TypeList<FUCK>,
    TypeList<>,
    Reader&& reader,
    messages::GlobalArgs::Builder&& builder,
    Fn&& fn,
    FnParams&&... fn_params)
{
    auto& args = static_cast<Args&>(::gArgs);
    {
        LOCK(args.cs_args);
        ReadField(TypeList<decltype(args.mapArgs)>(), MakeReader(reader.get().getArgs(), reader.loop()), args.mapArgs);
        ReadField(TypeList<decltype(args.mapMultiArgs)>(), MakeReader(reader.get().getMultiArgs(), reader.loop()),
            args.mapMultiArgs);
    }
}

template <typename FUCK, typename Builder>
void BuildField(TypeList<>, TypeList<FUCK>, Priority<1>, EventLoop& loop, messages::GlobalArgs::Builder&& builder)
{
    auto& args = static_cast<Args&>(::gArgs);
    {
        LOCK(args.cs_args);
        // FIXME BuildField(TypeList<decltype(args.mapArgs)>(), BuildFieldPriority(), loop, args.mapArgs, builder.getArgs());
        // FIXME BuildField(TypeList<decltype(args.mapMultiArgs)>(), BuildFieldPriority(), loop, args.mapMultiArgs,
        // builder.getMultiArgs());
    }
}

template <typename FUCK, typename Reader, typename Builder, typename Fn, typename... FnParams>
void PassField(TypeList<FUCK>,
    TypeList<int, const char* const*>,
    Reader&& reader,
    Builder&& builder,
    Fn&& fn,
    FnParams&&... fn_params)
{
    auto value = reader.get();
    std::vector<const char*> args(value.size());
    size_t i = 0;
    for (const auto& arg : value) {
        args[i] = arg.cStr();
        ++i;
    }
    int argc = args.size();
    const char* const* argv = args.data();
    fn(std::forward<FnParams>(fn_params)..., argc, argv);
}

template <typename FUCK, typename Builder>
void BuildField(TypeList<int, const char* const*>,
    TypeList<FUCK>,
    Priority<1>,
    EventLoop& loop,
    int argc,
    const char* const* argv,
    Builder&& builder)
{
    auto args = builder.set(argc);
    for (int i = 0; i < argc; ++i) {
        args.set(i, argv[i]);
    }
}

// FIXME Move to .cpp
template <typename T>
class Deleter : public CloseHook
{
public:
    Deleter(T&& value) : m_value(std::move(value)) {}
    void onClose(Base& interface, bool remote) override { T(std::move(m_value)); }
    T m_value;
};


void FillGlobalArgs(messages::GlobalArgs::Builder&&);

template <typename Builder>
void BuildField(TypeList<>, TypeList<messages::GlobalArgs::Builder>, Priority<1>, EventLoop& loop, Builder&& builder)
{
    FillGlobalArgs(builder.set());
}

} // namespace capnp

} // namespace interface

#endif // BITCOIN_INTERFACE_CAPNP_MESSAGES_IMPL_H
