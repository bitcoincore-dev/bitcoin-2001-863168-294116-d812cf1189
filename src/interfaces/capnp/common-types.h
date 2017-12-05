#ifndef BITCOIN_INTERFACES_CAPNP_COMMON_TYPES_H
#define BITCOIN_INTERFACES_CAPNP_COMMON_TYPES_H

#include <chainparams.h>
#include <interfaces/capnp/chain.capnp.proxy.h>
#include <interfaces/capnp/common.capnp.proxy.h>
#include <interfaces/capnp/handler.capnp.proxy.h>
#include <interfaces/capnp/init.capnp.proxy.h>
#include <interfaces/capnp/node.capnp.proxy.h>
#include <interfaces/capnp/proxy-types.h>
#include <interfaces/capnp/wallet.capnp.proxy.h>
#include <net_processing.h>
#include <netbase.h>
#include <validation.h>
#include <wallet/coincontrol.h>

#include <consensus/validation.h>


namespace interfaces {
namespace capnp {

//!@{
//! Functions to serialize / deserialize bitcoin objects that don't
//! already provide their own serialization.
void CustomBuildMessage(InvokeContext& invoke_context,
    const UniValue& univalue,
    messages::UniValue::Builder&& builder);
void CustomReadMessage(InvokeContext& invoke_context, const messages::UniValue::Reader& reader, UniValue& univalue);
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

template <typename LocalType, typename Reader, typename Value>
void ReadFieldUpdate(TypeList<LocalType>,
    InvokeContext& invoke_context,
    Reader&& reader,
    Value&& value,
    decltype(CustomReadMessage(invoke_context, reader.get(), value))* enable = nullptr)
{
    CustomReadMessage(invoke_context, reader.get(), value);
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
void CustomBuildField(TypeList<SecureString>, Priority<1>, InvokeContext& invoke_context, Value&& str, Output&& output)
{
    auto result = output.init(str.size());
    // Copy SecureString into output. Caller needs to be responsible for calling
    // memory_cleanse later on the output after it is sent.
    memcpy(result.begin(), str.data(), str.size());
}

template <typename LocalType, typename Value, typename Output>
void CustomBuildField(TypeList<LocalType>,
    Priority<2>,
    InvokeContext& invoke_context,
    Value&& value,
    Output&& output,
    decltype(CustomBuildMessage(invoke_context, value, output.init()))* enable = nullptr)
{
    CustomBuildMessage(invoke_context, value, output.init());
}

template <typename LocalType, typename Value, typename Output>
void CustomBuildField(TypeList<LocalType>,
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

template <typename Accessor, typename ServerContext, typename Fn, typename... Args>
auto CustomPassField(TypeList<>, ServerContext& server_context, const Fn& fn, Args&&... args) ->
    typename std::enable_if<std::is_same<decltype(Accessor::get(server_context.call_context.getParams())),
        messages::GlobalArgs::Reader>::value>::type
{
    ReadGlobalArgs(server_context, Accessor::get(server_context.call_context.getParams()));
    return fn.invoke(server_context, std::forward<Args>(args)...);
}

template <typename Output>
void CustomBuildField(TypeList<>,
    Priority<1>,
    InvokeContext& invoke_context,
    Output&& output,
    typename std::enable_if<std::is_same<decltype(output.init()), messages::GlobalArgs::Builder>::value>::type*
        enable = nullptr)
{
    BuildGlobalArgs(invoke_context, output.init());
}

} // namespace capnp

} // namespace interfaces

#endif // BITCOIN_INTERFACES_CAPNP_COMMON_TYPES_H
