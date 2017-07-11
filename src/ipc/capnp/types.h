#ifndef BITCOIN_IPC_CAPNP_TYPES_H
#define BITCOIN_IPC_CAPNP_TYPES_H

#include <addrdb.h>
#include <chainparams.h>
#include <clientversion.h>
#include <coins.h>
#include <ipc/capnp/messages.capnp.h>
#include <ipc/capnp/types.h>
#include <ipc/capnp/util.h>
#include <ipc/util.h>
#include <net.h>
#include <net_processing.h>
#include <netbase.h>
#include <pubkey.h>
#include <script/standard.h>
#include <serialize.h>
#include <univalue/include/univalue.h>
#include <validation.h>
#include <wallet/coincontrol.h>
#include <wallet/wallet.h>
#include <ipc/capnp/messages.capnp.h>
#include <ipc/interfaces.h>

#include <boost/core/underlying_type.hpp>
#include <boost/exception/diagnostic_information.hpp>
#include <capnp/capability.h>
#include <capnp/common.h>
#include <capnp/list.h>
#include <capnp/pretty-print.h>
#include <capnp/schema.h>
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

namespace ipc {
namespace capnp {

//!@{
//! Functions to serialize / deserialize bitcoin objects that don't
//! already provide their own serialization.
void BuildMessage(const proxyType& proxy, messages::Proxy::Builder&& builder);
void ReadMessage(const messages::Proxy::Reader& reader, proxyType& proxy);
void BuildMessage(const CNodeStats& node_stats, messages::NodeStats::Builder&& builder);
void ReadMessage(const messages::NodeStats::Reader& reader, CNodeStats& node_stats);
void BuildMessage(const CNodeStateStats& node_state_stats, messages::NodeStateStats::Builder&& builder);
void ReadMessage(const messages::NodeStateStats::Reader& reader, CNodeStateStats& node_state_stats);
void BuildMessage(const banmap_t& banmap, messages::Banmap::Builder&& builder);
void ReadMessage(const messages::Banmap::Reader& reader, banmap_t& banmap);
void BuildMessage(const UniValue& univalue, messages::UniValue::Builder&& builder);
void ReadMessage(const messages::UniValue::Reader& reader, UniValue& univalue);
void BuildMessage(const WalletValueMap& value_map, messages::WalletValueMap::Builder&& builder);
void ReadMessage(const messages::WalletValueMap::Reader& reader, WalletValueMap& value_map);
void BuildMessage(const WalletOrderForm& order_form, messages::WalletOrderForm::Builder&& builder);
void ReadMessage(const messages::WalletOrderForm::Reader& reader, WalletOrderForm& order_form);
void BuildMessage(const CTxDestination& dest, messages::TxDestination::Builder&& builder);
void ReadMessage(const messages::TxDestination::Reader& reader, CTxDestination& dest);
void BuildMessage(const CKey& key, messages::Key::Builder&& builder);
void ReadMessage(const messages::Key::Reader& reader, CKey& key);
void BuildMessage(const CCoinControl& coin_control, messages::CoinControl::Builder&& builder);
void ReadMessage(const messages::CoinControl::Reader& reader, CCoinControl& coin_control);
void BuildMessage(const Wallet::CoinsList& coins_list, messages::CoinsList::Builder&& builder);
void ReadMessage(const messages::CoinsList::Reader& reader, Wallet::CoinsList& coins_list);
void BuildMessage(const CRecipient& recipient, messages::Recipient::Builder&& builder);
void ReadMessage(const messages::Recipient::Reader& reader, CRecipient& recipient);
void BuildMessage(const WalletAddress& address, messages::WalletAddress::Builder&& builder);
void ReadMessage(const messages::WalletAddress::Reader& reader, WalletAddress& address);
void BuildMessage(const WalletBalances& balances, messages::WalletBalances::Builder&& builder);
void ReadMessage(const messages::WalletBalances::Reader& reader, WalletBalances& balances);
void BuildMessage(const WalletTx& wallet_tx, messages::WalletTx::Builder&& builder);
void ReadMessage(const messages::WalletTx::Reader& reader, WalletTx& wallet_tx);
void BuildMessage(const WalletTxOut& txout, messages::WalletTxOut::Builder&& builder);
void ReadMessage(const messages::WalletTxOut::Reader& reader, WalletTxOut& txout);
void BuildMessage(const WalletTxStatus& status, messages::WalletTxStatus::Builder&& builder);
void ReadMessage(const messages::WalletTxStatus::Reader& reader, WalletTxStatus& status);
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

//! Take consolidated value returned from ReadField and pass to fn as separate arguments.
template <typename Value, typename Value2, typename Fn, typename... FnArgs>
void PassField(TypeList<Value>, Value2&& value2, Fn&& fn, FnArgs&&... fn_args)
{
    // FIXME Weird rvalue issue static_cast below
    //  using C = typename std::conditional<std::is_same<Value, std::unique_ptr<ipc::Wallet>>::value,
    //  std::unique_ptr<ipc::Wallet>&&, Value&>::type;
    // currently thinking it should alway be lvalue reference, and then whatever replacevoid assigntuple thing will
    // turn it into r/lvalue ref whatever needed
    using C = Value&;
    fn(std::forward<FnArgs>(fn_args)..., static_cast<C>(value2));
}

// FIXME dedup below
template <typename Fn, typename... FnArgs>
void PassField(TypeList<std::string*>, boost::optional<std::string>&& value, Fn&& fn, FnArgs&&... fn_args)
{
    fn(std::forward<FnArgs>(fn_args)..., value ? &*value : nullptr);
}

template <typename Fn, typename... FnArgs>
void PassField(TypeList<int*>, boost::optional<int>&& value, Fn&& fn, FnArgs&&... fn_args)
{
    fn(std::forward<FnArgs>(fn_args)..., value ? &*value : nullptr);
}

template <typename Fn, typename... FnArgs>
void PassField(TypeList<isminetype*>, boost::optional<isminetype>&& value, Fn&& fn, FnArgs&&... fn_args)
{
    fn(std::forward<FnArgs>(fn_args)..., value ? &*value : nullptr);
}

template <typename Fn, typename... FnArgs>
void PassField(TypeList<FeeReason*>, boost::optional<FeeReason>&& value, Fn&& fn, FnArgs&&... fn_args)
{
    fn(std::forward<FnArgs>(fn_args)..., value ? &*value : nullptr);
}

template <typename Value, typename Reader, typename Getter, typename OptionalGetter>
typename std::remove_reference<Value>::type ReadField(TypeList<Value>,
    Priority<1>,
    EventLoop& loop,
    const Reader& reader,
    Getter getter,
    OptionalGetter optional_getter,
    typename std::enable_if<std::is_enum<Value>::value>::type* enable = 0)
{
    return Value((reader.*getter)());
}

template <typename Proxy>
class Callback
{
public:
    Callback(Proxy proxy) : m_proxy(std::move(proxy)) {}

    template <typename... CallArgs>
    auto operator()(CallArgs&&... args) -> decltype(std::declval<Proxy>()->call(std::forward<CallArgs>(args)...))
    {
        return m_proxy->call(std::forward<CallArgs>(args)...);
    }
    Proxy m_proxy;
};

template <typename Fn, typename Interface>
Fn MakeCallback(EventLoop& loop, typename Interface::Client&& client)
{
    auto proxy = std::make_shared<typename Proxy<Interface>::Client>(loop, std::move(client));
    return Callback<decltype(proxy)>(proxy);
}

template <typename FnR, typename... FnArgs, typename Reader, typename Getter, typename OptionalGetter>
std::function<FnR(FnArgs...)> ReadField(TypeList<std::function<FnR(FnArgs...)>>,
    Priority<1>,
    EventLoop& loop,
    const Reader& reader,
    Getter getter,
    OptionalGetter optional_getter)
{
    auto client = (reader.*getter)();
    using Client = decltype(client);
    using Interface = typename Client::Calls;
    return MakeCallback<std::function<FnR(FnArgs...)>, Interface>(loop, std::move(client));
}

template <typename Impl, typename Interface>
std::unique_ptr<Impl> MakeClient(EventLoop& loop, typename Interface::Client client)
{
    return MakeUnique<typename Proxy<Interface>::Client>(loop, std::move(client));
}

// FIXME dedup below
template <typename Reader, typename Getter, typename OptionalGetter>
std::unique_ptr<ipc::Wallet> ReadField(TypeList<std::unique_ptr<ipc::Wallet>>,
    Priority<1>,
    EventLoop& loop,
    const Reader& reader,
    Getter getter,
    OptionalGetter optional_getter)
{
    auto client = (reader.*getter)();
    return MakeClient<ipc::Wallet, messages::Wallet>(loop, std::move(client));
}

template <typename Reader, typename Getter, typename OptionalGetter>
std::unique_ptr<ipc::Handler> ReadField(TypeList<std::unique_ptr<ipc::Handler>>,
    Priority<1>,
    EventLoop& loop,
    const Reader& reader,
    Getter getter,
    OptionalGetter optional_getter)
{
    auto client = (reader.*getter)();
    return MakeClient<ipc::Handler, messages::Handler>(loop, std::move(client));
}

template <typename Reader, typename Getter, typename OptionalGetter>
std::unique_ptr<ipc::PendingWalletTx> ReadField(TypeList<std::unique_ptr<ipc::PendingWalletTx>>,
    Priority<1>,
    EventLoop& loop,
    const Reader& reader,
    Getter getter,
    OptionalGetter optional_getter)
{
    auto client = (reader.*getter)();
    return MakeClient<ipc::PendingWalletTx, messages::PendingWalletTx>(loop, std::move(client));
}

template <typename Value>
void ReadMessage(const ::capnp::Data::Reader& reader,
    Value& value,
    typename std::enable_if<Serializable<Value>::value>::type* enable = nullptr)
{
    CDataStream stream(CharCast(reader.begin()), CharCast(reader.end()), SER_NETWORK, CLIENT_VERSION);
    value.Unserialize(stream);
}

static inline void ReadMessage(const ::capnp::Text::Reader& reader, std::string& value)
{
    value.assign(reader.begin(), reader.end());
}

static inline void ReadMessage(const ::capnp::Data::Reader& reader, std::string& value)
{
    value.assign(CharCast(reader.begin()), CharCast(reader.end()));
}

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

template <typename Reader, typename Getter, typename OptionalGetter>
SecureString ReadField(TypeList<const SecureString&>,
    Priority<1>,
    EventLoop& loop,
    const Reader& reader,
    Getter getter,
    OptionalGetter optional_getter)
{
    auto data = (reader.*getter)();
    const char* c = CharCast(data.begin());
    size_t s = data.size();
    return {c, s};
}

template <typename R, typename T>
void ReadMessage(const R& reader, std::vector<T>& vec)
{
    vec.clear();
    vec.reserve(reader.size());
    for (const auto& elem : reader) {
        vec.emplace_back(); // FIXME: maybe call constructor
        ReadMessage(elem, vec.back());
    }
}

template <typename R, typename T>
void ReadMessage(const R& reader, std::set<T>& set)
{
    for (const auto& elem : reader) {
        T value;
        ReadMessage(elem, value);
        set.emplace(value); // FIXME: maybe call constructor
    }
}

template <typename Value, typename Reader, typename Getter, typename OptionalGetter>
typename std::remove_reference<Value>::type ReadField(TypeList<Value>,
    Priority<1>,
    EventLoop& loop,
    const Reader& reader,
    Getter getter,
    OptionalGetter optional_getter,
    decltype(ReadMessage((reader.*getter)(),
        std::declval<typename std::remove_const<typename std::remove_reference<Value>::type>::type&>()))* enable =
        nullptr)
{
    typename std::remove_const<typename std::remove_reference<Value>::type>::type value;
    ReadMessage((reader.*getter)(), value);
    return value;
}

template <typename Reader, typename Getter, typename OptionalGetter>
CTransactionRef ReadField(TypeList<CTransactionRef>,
    Priority<1>,
    EventLoop& loop,
    const Reader& reader,
    Getter getter,
    OptionalGetter optional_getter)
{
    // FIXME: check optional_getter
    auto data = (reader.*getter)();
    CDataStream stream(CharCast(data.begin()), CharCast(data.end()), SER_NETWORK, CLIENT_VERSION);
    return std::make_shared<CTransaction>(deserialize, stream);
}

template <typename Reader, typename Getter, typename OptionalGetter>
boost::optional<std::string> ReadField(TypeList<std::string*>,
    Priority<1>,
    EventLoop& loop,
    const Reader& reader,
    Getter getter,
    OptionalGetter optional_getter)
{
    if (true /* FIXME needs to access alternate reader object, params not results: (reader.*optional_getter)() */) {
        auto data = (reader.*getter)();
        const char* c = data.begin();
        size_t s = data.size();
        return std::string{c, s};
    }
    return {};
}

template <typename Reader, typename OptionalGetter>
boost::optional<std::string> ReadField(TypeList<std::string*>,
    Priority<1>,
    EventLoop& loop,
    const Reader& reader,
    std::nullptr_t,
    OptionalGetter optional_getter)
{
    return {};
}

template <typename Reader, typename Getter, typename OptionalGetter>
boost::optional<int> ReadField(TypeList<int*>,
    Priority<1>,
    EventLoop& loop,
    const Reader& reader,
    Getter getter,
    OptionalGetter optional_getter)
{
    if (true /* FIXME needs to access alternate reader object, params not results: (reader.*optional_getter)() */) {
        return (reader.*getter)();
    }
    return {};
}

template <typename Reader, typename OptionalGetter>
boost::optional<int> ReadField(TypeList<int*>,
    Priority<1>,
    EventLoop& loop,
    const Reader& reader,
    std::nullptr_t,
    OptionalGetter optional_getter)
{
    return {};
}

template <typename Reader, typename Getter, typename OptionalGetter>
boost::optional<isminetype> ReadField(TypeList<isminetype*>,
    Priority<1>,
    EventLoop& loop,
    const Reader& reader,
    Getter getter,
    OptionalGetter optional_getter)
{
    if (true /* FIXME needs to access alternate reader object, params not results: (reader.*optional_getter)() */) {
        return static_cast<isminetype>((reader.*getter)());
    }
    return {};
}

template <typename Reader, typename OptionalGetter>
boost::optional<isminetype> ReadField(TypeList<isminetype*>,
    Priority<1>,
    EventLoop& loop,
    const Reader& reader,
    std::nullptr_t,
    OptionalGetter optional_getter)
{
    return {};
}

template <typename Reader, typename Getter, typename OptionalGetter>
boost::optional<FeeReason> ReadField(TypeList<FeeReason*>,
    Priority<1>,
    EventLoop& loop,
    const Reader& reader,
    Getter getter,
    OptionalGetter optional_getter)
{
    if (true /* FIXME needs to access alternate reader object, params not results: (reader.*optional_getter)() */) {
        return static_cast<FeeReason>((reader.*getter)());
    }
    return {};
}

template <typename Reader, typename OptionalGetter>
boost::optional<FeeReason> ReadField(TypeList<FeeReason*>,
    Priority<1>,
    EventLoop& loop,
    const Reader& reader,
    std::nullptr_t,
    OptionalGetter optional_getter)
{
    return {};
}

template <typename Value, typename Reader, typename Getter, typename OptionalGetter>
typename std::remove_reference<Value>::type ReadField(TypeList<Value>,
    Priority<0>,
    EventLoop& loop,
    const Reader& reader,
    Getter getter,
    OptionalGetter optional_getter)
{
    return (reader.*getter)();
}

template <typename Value, typename Reader, typename OptionalGetter>
typename std::remove_reference<Value>::type ReadField(TypeList<Value>,
    Priority<0>,
    EventLoop& loop,
    const Reader& reader,
    std::nullptr_t getter,
    OptionalGetter optional_getter)
{
    return {};
}

template <typename Value, typename Setter, typename OptionalSetter, typename Builder>
void BuildField(TypeList<Value>,
    Priority<1>,
    EventLoop& loop,
    const std::string& str,
    Setter init,
    OptionalSetter optional_setter,
    Builder& builder)
{
    auto result = (builder.*init)(str.size());
    memcpy(result.begin(), str.data(), str.size());
}

template <typename Value, typename Setter, typename OptionalSetter, typename Builder>
void BuildField(TypeList<Value>,
    Priority<1>,
    EventLoop& loop,
    const SecureString& str,
    Setter init,
    OptionalSetter optional_setter,
    Builder& builder)
{
    auto result = (builder.*init)(str.size());
    // FIXME: should call memory_cleanse on this pointer after outgoing message is sent. similarly should call
    // memory_cleanse after readfield is called for incoming messages.
    memcpy(result.begin(), str.data(), str.size());
}

template <typename Interface, typename Fn>
kj::Own<typename Proxy<Interface>::Server> MakeServer(EventLoop& loop, Fn fn)
{
    return kj::heap<typename Proxy<Interface>::Server>(std::move(fn), loop);
}

template <typename Value,
    typename FnR,
    typename... FnArgs,
    typename OptionalSetter,
    typename Builder,
    typename Builder2,
    typename Client>
void BuildField(TypeList<Value>,
    Priority<1>,
    EventLoop& loop,
    std::function<FnR(FnArgs...)>&& value,
    void (Builder2::*set)(Client&&),
    OptionalSetter optional_setter,
    Builder& builder)
{
    using Interface = typename Client::Calls;
    (builder.*set)(MakeServer<Interface>(loop, value));
}

template <typename Value, typename Value2, typename Setter, typename OptionalSetter, typename Builder>
void BuildField(TypeList<Value> field_args,
    Priority<1>,
    EventLoop& loop,
    boost::optional<Value2>&& value,
    Setter init,
    OptionalSetter optional_setter,
    Builder& builder)
{
    if (value) {
        BuildField(field_args, Priority<2>(), loop, *value, init, optional_setter, builder);
    }
}

template <typename Value, typename Value2, typename Setter, typename OptionalSetter, typename Builder>
void BuildField(TypeList<Value> field_args,
    Priority<1>,
    EventLoop& loop,
    const std::shared_ptr<Value2>& value,
    Setter init,
    OptionalSetter optional_setter,
    Builder& builder)
{
    if (value) {
        BuildField(field_args, Priority<2>(), loop, *value, init, optional_setter, builder);
    }
}

template <typename Setter, typename OptionalSetter, typename Builder>
void BuildField(TypeList<Node::NodesStats&>,
    Priority<1>,
    EventLoop& loop,
    const Node::NodesStats& stats,
    Setter init,
    OptionalSetter optional_setter,
    Builder& builder)
{
    auto result_stats = (builder.*init)(stats.size());
    size_t i = 0;
    for (const auto& node_stats : stats) {
        BuildMessage(std::get<0>(node_stats), result_stats[i]);
        if (std::get<1>(node_stats)) {
            BuildMessage(std::get<2>(node_stats), result_stats[i].initStateStats());
        }
        ++i;
    }
}

template <typename Value, typename Value2, typename Setter, typename OptionalSetter, typename Builder>
void BuildField(TypeList<Value>,
    Priority<2>,
    EventLoop& loop,
    const Value2& value,
    Setter init,
    OptionalSetter optional_setter,
    Builder& builder,
    decltype(BuildMessage(value, (builder.*init)()))* enable = nullptr)
{
    BuildMessage(value, (builder.*init)());
}

template <typename Value, typename Value2, typename Setter, typename OptionalSetter, typename Builder>
void BuildField(TypeList<Value>,
    Priority<1>,
    EventLoop& loop,
    const std::vector<Value2>& value,
    Setter init,
    OptionalSetter optional_setter,
    Builder& builder)
{
    // FIXME dedup with set handler below
    auto list = (builder.*init)(value.size());
    size_t i = 0;
    for (const auto& elem : value) {
        ListElemBuilder<typename decltype(list)::Builds> elem_builder(list, i);
        auto method = elem_builder.getSetter();
        BuildField(TypeList<Value2>(), Priority<2>(), loop, elem, method, nullptr, elem_builder);
        ++i;
    }
}

template <typename Value, typename Value2, typename Setter, typename OptionalSetter, typename Builder>
void BuildField(TypeList<Value>,
    Priority<1>,
    EventLoop& loop,
    const std::set<Value2>& value,
    Setter init,
    OptionalSetter optional_setter,
    Builder& builder)
{
    // FIXME dededup with vector handler above
    auto list = (builder.*init)(value.size());
    size_t i = 0;
    for (const auto& elem : value) {
        ListElemBuilder<typename decltype(list)::Builds> elem_builder(list, i);
        auto method = elem_builder.getSetter();
        BuildField(TypeList<Value2>(), Priority<2>(), loop, elem, method, nullptr, elem_builder);
        ++i;
    }
}

template <typename Value, typename Value2, typename Setter, typename OptionalSetter, typename Builder>
void BuildField(TypeList<Value>,
    Priority<1>,
    EventLoop& loop,
    const Value2& value,
    Setter init,
    OptionalSetter optional_setter,
    Builder& builder,
    typename std::enable_if<Serializable<Value2>::value>::type* enable = nullptr)
{
    CDataStream stream(SER_NETWORK, CLIENT_VERSION);
    value.Serialize(stream);
    auto result = (builder.*init)(stream.size());
    memcpy(result.begin(), stream.data(), stream.size());
}

template <typename Value, typename DestValue>
DestValue BuildPrimitive(EventLoop& loop,
    const Value& value,
    TypeList<DestValue>,
    typename std::enable_if<std::is_enum<Value>::value>::type* enable = nullptr)
{
    return static_cast<DestValue>(value);
}

template <typename Value, typename DestValue>
DestValue BuildPrimitive(EventLoop& loop,
    const Value& value,
    TypeList<DestValue>,
    typename std::enable_if<std::numeric_limits<Value>::is_integer>::type* enable = nullptr)
{
    static_assert(
        std::numeric_limits<DestValue>::lowest() <= std::numeric_limits<Value>::lowest(), "mismatched integral types");
    static_assert(
        std::numeric_limits<DestValue>::max() >= std::numeric_limits<Value>::max(), "mismatched integral types");
    return value;
}

template <typename Value, typename DestValue>
DestValue BuildPrimitive(EventLoop& loop,
    const Value& value,
    TypeList<DestValue>,
    typename std::enable_if<std::numeric_limits<Value>::is_iec559>::type* enable = nullptr)
{
    static_assert(std::is_same<Value, DestValue>::value,
        "mismatched floating point types. please fix message.capnp type declaration to match wrapped interface");
    return value;
}

static inline ::capnp::Void BuildPrimitive(EventLoop& loop, RPCTimerInterface*, TypeList<::capnp::Void>) { return {}; }

// FIXME: Should take rvalue reference to impl
template <typename Impl, typename Client>
Client BuildPrimitive(EventLoop& loop, std::unique_ptr<Impl>& impl, TypeList<Client&&>)
{
    using Interface = typename Client::Calls;
    return MakeServer<Interface>(loop, std::move(impl));
}

template <typename Value,
    typename Value2,
    typename DestValue,
    typename OptionalSetter,
    typename Builder,
    typename Builder2>
void BuildField(TypeList<Value>,
    Priority<0>,
    EventLoop& loop,
    Value2&& value2,
    void (Builder::*set)(DestValue),
    OptionalSetter optional_setter,
    Builder2& builder)
{
    (builder.*set)(BuildPrimitive(loop, value2, TypeList<DestValue>()));
}

template <typename... Values, typename Value2, typename OptionalSetter, typename Builder>
void BuildField(TypeList<Values...>,
    Priority<1>,
    EventLoop& loop,
    Value2&& value2,
    std::nullptr_t setter,
    OptionalSetter optional_setter,
    Builder& builder)
{
    // FIXME should set optional setter
}

template <typename Reader, typename Getter, typename OptionalGetter>
RPCTimerInterface* ReadField(TypeList<RPCTimerInterface*>,
    Priority<1>,
    EventLoop& loop,
    const Reader& reader,
    Getter getter,
    OptionalGetter optional_getter)
{
    return {}; // FIXME
}

template <typename Reader, typename Getter, typename OptionalGetter>
::capnp::List<::capnp::Text>::Reader ReadField(TypeList<int, const char* const*>,
    Priority<1>,
    EventLoop& loop,
    const Reader& reader,
    Getter getter,
    OptionalGetter optional_getter)
{
    return (reader.*getter)();
}

template <typename Value2, typename Fn, typename... FnArgs>
void PassField(TypeList<int, const char* const*>, Value2&& value2, Fn&& fn, FnArgs&&... fn_args)
{
    std::vector<const char*> args(value2.size());
    size_t i = 0;
    for (const auto& arg : value2) {
        args[i] = arg.cStr();
        ++i;
    }
    int argc = args.size();
    const char* const* argv = args.data();
    fn(std::forward<FnArgs>(fn_args)..., argc, argv);
}

template <typename Setter, typename OptionalSetter, typename Builder>
void BuildField(TypeList<int, const char* const*>,
    Priority<1>,
    EventLoop& loop,
    int argc,
    const char* const* argv,
    Setter init,
    OptionalSetter optional_setter,
    Builder& builder)
{
    auto args = (builder.*init)(argc);
    for (int i = 0; i < argc; ++i) {
        args.set(i, argv[i]);
    }
}

class NodeServerBase : public messages::Node::Server, public ProxyServerBase
{
 public:
  //using ProxyServerBase::ProxyServerBase;
  // FIXME workaround for https://github.com/include-what-you-use/include-what-you-use/issues/402
  template<typename...A> NodeServerBase(A&&...a) : ProxyServerBase(std::forward<A>(a)...) {}

  kj::Promise<void> localRpcSetTimerInterfaceIfUnset(ipc::Node& node,
                                                     messages::Node::Server::RpcSetTimerInterfaceIfUnsetContext context);
  kj::Promise<void> localRpcUnsetTimerInterface(ipc::Node& node,
                                                messages::Node::Server::RpcUnsetTimerInterfaceContext context);
  std::unique_ptr<::RPCTimerInterface> m_timer_interface;
};

} // namespace capnp
} // namespace ipc

#endif // BITCOIN_IPC_CAPNP_TYPES_H
