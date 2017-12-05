#ifndef BITCOIN_INTERFACES_CAPNP_PROXY_H
#define BITCOIN_INTERFACES_CAPNP_PROXY_H

#include <interfaces/capnp/proxy.h>

#include <interfaces/capnp/util.h>
#include <util.h>

#include <algorithm>
#include <boost/exception/diagnostic_information.hpp>
#include <boost/optional.hpp>
#include <capnp/capability.h>
#include <capnp/common.h>
#include <capnp/orphan.h>
#include <capnp/pretty-print.h>
#include <capnp/rpc-twoparty.h>
#include <capnp/schema.h>
#include <fstream>
#include <functional>
#include <future>
#include <iostream>
#include <kj/async-inl.h>
#include <kj/async-io.h>
#include <kj/async.h>
#include <kj/common.h>
#include <kj/debug.h>
#include <kj/exception.h>
#include <kj/memory.h>
#include <kj/string.h>
#include <list>
#include <memory>
#include <mutex>
#include <set>
#include <stdio.h>
#include <string>
#include <sys/types.h>
#include <thread>
#include <tuple>
#include <type_traits>
#include <unistd.h>
#include <utility>

namespace interfaces {
namespace capnp {

//! Invoke callable and save return value or exception state in promise.
template <typename T, typename Callable>
void SetPromise(std::promise<T>& promise, Callable&& callable)
{
    try {
        promise.set_value(callable());
    } catch (...) {
        promise.set_exception(std::current_exception());
    }
}

//! Invoke void callable and save exception state in promise.
template <typename Callable>
void SetPromise(std::promise<void>& promise, Callable&& callable)
{
    try {
        callable();
        promise.set_value();
    } catch (...) {
        promise.set_exception(std::current_exception());
    }
}

//! Handler for kj::TaskSet failed task events.
class LoggingErrorHandler : public kj::TaskSet::ErrorHandler
{
public:
    void taskFailed(kj::Exception&& exception) override;
};

//! Get return type of a callable type.
template <typename Callable>
using ResultOf = decltype(std::declval<Callable>()());

//! Wrapper around callback function for compatibility with std::async.
//!
//! std::async requires callbacks to be copyable and requires noexcept
//! destructors, but this doesn't work well with kj types which are generally
//! move-only and not noexcept.
template <typename Callable>
struct AsyncCallable
{
    AsyncCallable(Callable&& callable) : m_callable(std::make_shared<Callable>(std::move(callable))) {}
    AsyncCallable(const AsyncCallable&) = default;
    AsyncCallable(AsyncCallable&&) = default;
    ~AsyncCallable() noexcept {}
    ResultOf<Callable> operator()() { return (*m_callable)(); }
    mutable std::shared_ptr<Callable> m_callable;
};

//! Construct AsyncCallable object.
template <typename Callable>
AsyncCallable<typename std::remove_reference<Callable>::type> MakeAsyncCallable(Callable&& callable)
{
    return std::move(callable);
}

//! Event loop implementation.
//!
//! Based on https://groups.google.com/d/msg/capnproto/TuQFF1eH2-M/g81sHaTAAQAJ
class EventLoop
{
public:
    //! Construct event loop object.
    //!
    //! @param[in]  thread  optional thread handle to join on destruction.
    EventLoop(std::thread&& thread = {});
    ~EventLoop();

    //! Run event loop. Does not return until shutdown.
    void loop();

    //! Run callable on event loop thread. Does not return until callable completes.
    template <typename Callable>
    void sync(Callable&& callable)
    {
        post(std::ref(callable));
    }

    //! Send shutdown signal to event loop. Returns immediately.
    void shutdown();

    //! Run function on event loop thread. Does not return until function completes.
    void post(std::function<void()> fn);

    //! Invoke callable and save return value or exception state in promise.
    template <typename T, typename Callable>
    void fulfillPromise(kj::PromiseFulfiller<T>& fulfiller, Callable&& callable)
    {
        KJ_IF_MAYBE(exception, kj::runCatchingExceptions([&]() {
                        auto result = callable();
                        sync([&] { fulfiller.fulfill(std::move(result)); });
                    }))
        {
            sync([&]() { fulfiller.reject(kj::mv(*exception)); });
        }
    }

    //! Invoke void callable and save exception state in promise.
    template <typename Callable>
    void fulfillPromise(kj::PromiseFulfiller<void>& fulfiller, Callable&& callable)
    {
        KJ_IF_MAYBE(exception, kj::runCatchingExceptions([&]() {
                        callable();
                        sync([&]() { fulfiller.fulfill(); });
                    }))
        {
            sync([&]() { fulfiller.reject(kj::mv(*exception)); });
        }
    }


    CleanupIt addCleanup(std::function<void()> fn)
    {
        return m_cleanup_fns.emplace(m_cleanup_fns.begin(), std::move(fn));
    }

    void removeCleanup(CleanupIt it) { m_cleanup_fns.erase(it); }

    kj::AsyncIoContext m_io_context;
    LoggingErrorHandler m_error_handler;
    kj::TaskSet m_task_set{m_error_handler};
    std::thread m_thread;
    std::thread::id m_thread_id = std::this_thread::get_id();
    std::mutex m_post_mutex;
    std::function<void()> m_post_fn;
    CleanupList m_cleanup_fns;
    int m_wait_fd = -1;
    int m_post_fd = -1;
    bool m_debug = false;
};

class SyncDestroy
{
    virtual ~SyncDestroy() {}
    virtual void shutdown(EventLoop& loop) {}
};

struct Connection
{
    Connection(EventLoop& loop, int fd)
        : m_stream(loop.m_io_context.lowLevelProvider->wrapSocketFd(fd, kj::LowLevelAsyncIoProvider::TAKE_OWNERSHIP)),
          m_network(*m_stream, ::capnp::rpc::twoparty::Side::CLIENT, ::capnp::ReaderOptions()),
          m_rpc_client(::capnp::makeRpcClient(m_network))
    {
    }

    kj::Own<kj::AsyncIoStream> m_stream;
    ::capnp::TwoPartyVatNetwork m_network;
    ::capnp::RpcSystem<::capnp::rpc::twoparty::VatId> m_rpc_client;
};

// two shutdown sequences need to be supported, one where event loop thread exits before class is destroyed, one where
// class being destroyed shuts down event loop.
//
// event loop thread exits
//  class cleanup callback called
//  std::move(m_client) to temporary
//  m_connection = nullptr
//  m_loop = nullptr
//
// class is destroyed
//  if m_loop:
//  m_loop->sync:
//    m_loop->removecleanup
//      std::move(m_client) to temporary
//      m_connection = nullptr
//      m_loop->shutdown
//      m_loop = nullptr
template <typename Interface, typename Class>
ProxyClientBase<Interface, Class>::ProxyClientBase(typename Interface::Client client, EventLoop& loop)
    : m_client(std::move(client)), m_loop(&loop)
{
    m_cleanup = loop.addCleanup([this]() { cleanup(true); });
}

template <typename Interface, typename Class>
ProxyClientBase<Interface, Class>::~ProxyClientBase() noexcept
{
    if (m_loop) {
        m_loop->sync([&]() {
            m_loop->removeCleanup(m_cleanup);
            cleanup(false);
        });
    }
}

template <typename Interface, typename Class>
void ProxyClientBase<Interface, Class>::cleanup(bool remote)
{
    typename Interface::Client(std::move(m_client));
    close(remote);
    m_loop = nullptr;
}


//! FIXME Hack to create server that doesn't delete underlying interface ptr.
// Need to figure out destruction and get rid of this and mainclient crap.
template <typename Base>
class MainServer : public Base
{
public:
    template <typename Impl, typename... Args>
    MainServer(Impl& impl, Args&&... args) : Base(std::unique_ptr<Impl>(&impl), std::forward<Args>(args)...)
    {
    }

    ~MainServer() { this->m_impl.release(); }
};

//! Invoke callback returning true if callback returns void
template <typename Callable,
    typename Result = ResultOf<Callable>,
    typename Enable = typename std::enable_if<!std::is_same<Result, void>::value>::type>
Result ReturnTrue(Callable&& callable)
{
    return callable();
}

template <typename Callable,
    typename Result = ResultOf<Callable>,
    typename Enable = typename std::enable_if<std::is_same<Result, void>::value>::type>
bool ReturnTrue(Callable&& callable)
{
    callable();
    return true;
}

template <typename Reader, typename Accessor>
class FieldInput
{
    const Reader& m_reader;
    const Accessor& m_accessor;

public:
    using Getter = decltype(std::declval<Accessor>().getter);
    static constexpr bool can_get = !std::is_same<Getter, std::nullptr_t>::value;
    using CapType = typename CapValueTraits<typename FunctionTraits<Getter>::Result>::CapType;

    FieldInput(const Reader& reader, const Accessor& accessor) : m_reader(reader), m_accessor(accessor) {}

    template <typename Result = void>
    auto get() const -> decltype(CallMethod<Result>(this->m_reader, m_accessor.getter))
    {
        // FIXME should be compile error to call this function if accessor can_get not true
        assert(has());
        return CallMethod<Result>(m_reader, m_accessor.getter);
    }

    bool has() const
    {
        return m_accessor.has_getter == nullptr || CallMethod<bool>(this->m_reader, m_accessor.has_getter);
    }
    bool want() const
    {
        return m_accessor.want_getter == nullptr || CallMethod<bool>(this->m_reader, m_accessor.want_getter);
    }
};

template <typename Value>
class Emplace
{
    Value& m_value;

    template <typename T, typename... Params>
    static T& call(boost::optional<T>& value, Params&&... params)
    {
        value.emplace(std::forward<Params>(params)...);
        return *value;
    }

    template <typename T, typename... Params>
    static T& call(std::vector<T>& value, Params&&... params)
    {
        value.emplace_back(std::forward<Params>(params)...);
        return value.back();
    }

    template <typename T, typename... Params>
    static const T& call(std::set<T>& value, Params&&... params)
    {
        return *value.emplace(std::forward<Params>(params)...).first;
    }

    template <typename K, typename V, typename... Params>
    static std::pair<const K, V>& call(std::map<K, V>& value, Params&&... params)
    {
        return *value.emplace(std::forward<Params>(params)...).first;
    }

    template <typename T, typename... Params>
    static T& call(std::shared_ptr<T>& value, Params&&... params)
    {
        value = std::make_shared<T>(std::forward<Params>(params)...);
        return *value;
    }

    template <typename T, typename... Params>
    static T& call(std::reference_wrapper<T>& value, Params&&... params)
    {
        value.get().~T();
        new (&value.get()) T(std::forward<Params>(params)...);
        return value.get();
    }

public:
    static constexpr bool emplace = true;

    Emplace(Value& value) : m_value(value) {}

    // Needs to be declared after m_value for compiler to understand declaration.
    template <typename... Params>
    auto operator()(Params&&... params) -> AUTO_RETURN(Emplace::call(this->m_value, std::forward<Params>(params)...))
};

template <typename LocalType, typename Proxy, typename Input, typename DestValue>
void ReadFieldUpdate(TypeList<boost::optional<LocalType>>, Proxy& proxy, Input&& input, DestValue&& value)
{
    if (!input.has()) {
        value.reset();
        return;
    }
    if (value) {
        ReadField(TypeList<LocalType>(), proxy, input, *value);
    } else {
        ReadField(TypeList<LocalType>(), proxy, input, Emplace<DestValue>(value));
    }
}

template <typename LocalType, typename Proxy, typename Input, typename DestValue>
void ReadFieldUpdate(TypeList<std::shared_ptr<LocalType>>, Proxy& proxy, Input&& input, DestValue&& value)
{
    if (!input.has()) {
        value.reset();
        return;
    }
    if (value) {
        ReadField(TypeList<LocalType>(), proxy, input, *value);
    } else {
        ReadField(TypeList<LocalType>(), proxy, input, Emplace<DestValue>(value));
    }
}

template <typename LocalType, typename Proxy, typename Input, typename DestValue>
void ReadFieldUpdate(TypeList<std::shared_ptr<const LocalType>>, Proxy& proxy, Input&& input, DestValue&& value)
{

    if (!input.has()) {
        value.reset();
        return;
    }
    ReadField(TypeList<LocalType>(), proxy, input, Emplace<DestValue>(value));
}

template <typename LocalType, typename Proxy, typename Input, typename DestValue>
void ReadFieldUpdate(TypeList<std::vector<LocalType>>, Proxy& proxy, Input&& input, DestValue&& value)
{
    auto data = input.get();
    value.clear();
    value.reserve(data.size());
    for (auto&& item : data) {
        ReadField(TypeList<LocalType>(), proxy, MakeValueInput(item), Emplace<DestValue>(value));
    }
}

template <typename LocalType, typename Proxy, typename Input, typename DestValue>
void ReadFieldUpdate(TypeList<std::set<LocalType>>, Proxy& proxy, Input&& input, DestValue&& value)
{
    auto data = input.get();
    value.clear();
    for (auto&& item : data) {
        ReadField(TypeList<LocalType>(), proxy, MakeValueInput(item), Emplace<DestValue>(value));
    }
}

template <typename KeyLocalType, typename ValueLocalType, typename Proxy, typename Input, typename DestValue>
void ReadFieldUpdate(TypeList<std::map<KeyLocalType, ValueLocalType>>, Proxy& proxy, Input&& input, DestValue&& value)
{
    auto data = input.get();
    value.clear();
    for (auto&& item : data) {
        ReadField(TypeList<std::pair<KeyLocalType, ValueLocalType>>(), proxy, MakeValueInput(item),
            Emplace<DestValue>(value));
    }
}

template <typename Fn>
struct TupleEmplace
{
    static constexpr bool emplace = true;
    Fn& m_fn;
    template <typename... Params>
    auto operator()(Params&&... params) -> AUTO_RETURN(this->m_fn(std::forward_as_tuple(params...)))
};

template <typename Fn>
TupleEmplace<Fn> MakeTupleEmplace(Fn&& fn)
{
    return {fn};
}

template <typename Exception>
struct ThrowEmplace
{
    static constexpr bool emplace = true;

    template <typename... Params>
    void operator()(Params&&... params)
    {
        throw Exception(std::forward<Params>(params)...);
    }
};

template <>
struct ThrowEmplace<std::exception> : ThrowEmplace<std::runtime_error>
{
};

template <typename ValueLocalType, typename Proxy, typename Input, typename Emplace>
struct PairValueEmplace
{
    Proxy& m_context;
    Input& m_input;
    Emplace& m_emplace;
    template <typename KeyTuple>
    void operator()(KeyTuple&& key_tuple)
    {
        auto value_accessor = ProxyStruct<typename Plain<Input>::CapType>::get(std::integral_constant<size_t, 1>());
        FieldInput<Plain<decltype(m_input.get())>, decltype(value_accessor)> value_input{
            m_input.get(), value_accessor};
        ReadField(TypeList<ValueLocalType>(), m_context, value_input,
            MakeTupleEmplace(Make<Compose>(Get<1>(), Bind(m_emplace, std::piecewise_construct, key_tuple))));
    }
};

template <typename KeyLocalType, typename ValueLocalType, typename Proxy, typename Input, typename Emplace>
void ReadFieldNew(TypeList<std::pair<KeyLocalType, ValueLocalType>>, Proxy& proxy, Input&& input, Emplace&& emplace)
{
    /* This could be simplified a lot with c++14 generic lambdas. All it is doing is:
    ReadField(TypeList<KeyLocalType>(), proxy, MakeValueInput(input.get().getKey()), [&](auto&&... key_args) {
        ReadField(TypeList<ValueLocalType>(), proxy, MakeValueInput(input.get().getValue()), [&](auto&&... value_args)
    {
            emplace(std::piecewise_construct, std::forward_as_tuple(key_args...),
    std::forward_as_tuple(value_args...));
        })
    });
    */
    auto key_accessor = ProxyStruct<typename Plain<Input>::CapType>::get(std::integral_constant<size_t, 0>());
    FieldInput<Plain<decltype(input.get())>, decltype(key_accessor)> key_input{input.get(), key_accessor};
    ReadField(TypeList<KeyLocalType>(), proxy, key_input,
        MakeTupleEmplace(PairValueEmplace<ValueLocalType, Proxy, Input, Emplace>{proxy, input, emplace}));
}

template <typename KeyLocalType, typename ValueLocalType, typename Proxy, typename Input, typename Tuple>
void ReadFieldUpdate(TypeList<std::tuple<KeyLocalType, ValueLocalType>>, Proxy& proxy, Input&& input, Tuple&& tuple)
{
    auto key_accessor = ProxyStruct<typename Plain<Input>::CapType>::get(std::integral_constant<size_t, 0>());
    FieldInput<Plain<decltype(input.get())>, decltype(key_accessor)> key_input{input.get(), key_accessor};

    auto value_accessor = ProxyStruct<typename Plain<Input>::CapType>::get(std::integral_constant<size_t, 1>());
    FieldInput<Plain<decltype(input.get())>, decltype(value_accessor)> value_input{input.get(), value_accessor};

    ReadField(TypeList<KeyLocalType>(), proxy, key_input, std::get<0>(tuple));
    ReadField(TypeList<ValueLocalType>(), proxy, value_input, std::get<1>(tuple));
}

// FIXME move to util
template <int priority, bool enable>
using PriorityEnable = typename std::enable_if<enable, Priority<priority>>::type;

template <typename LocalType, typename Proxy, typename Input, typename Emplace>
void ReadFieldNew(TypeList<LocalType>,
    Proxy& proxy,
    Input&& input,
    Emplace&& emplace,
    typename std::enable_if<std::is_enum<LocalType>::value>::type* enable = 0)
{
    emplace(static_cast<LocalType>(input.get()));
}

template <typename LocalType, typename Proxy, typename Input, typename Emplace>
void ReadFieldNew(TypeList<LocalType>,
    Proxy& proxy,
    Input&& input,
    Emplace&& emplace,
    typename std::enable_if<std::is_integral<LocalType>::value>::type* enable = 0)
{
    auto value = input.get();
    if (value < std::numeric_limits<LocalType>::min() || value > std::numeric_limits<LocalType>::max()) {
        throw std::range_error("out of bound int received");
    }
    emplace(static_cast<LocalType>(value));
}

template <typename LocalType, typename Proxy, typename Input, typename Emplace>
void ReadFieldNew(TypeList<LocalType>,
    Proxy& proxy,
    Input&& input,
    Emplace&& emplace,
    typename std::enable_if<std::is_floating_point<LocalType>::value>::type* enable = 0)
{
    auto value = input.get();
    static_assert(std::is_same<LocalType, decltype(value)>::value, "floating point type mismatch");
    emplace(value);
}

template <typename Proxy, typename Input, typename Emplace>
void ReadFieldNew(TypeList<std::string>, Proxy& proxy, Input&& input, Emplace&& emplace)
{
    auto data = input.get();
    emplace(CharCast(data.begin()), data.size());
}

template <typename Proxy, typename Input, typename Emplace>
void ReadFieldNew(TypeList<std::exception>, Proxy& proxy, Input&& input, Emplace&& emplace)
{
    auto data = input.get();
    emplace(std::string(CharCast(data.begin()), data.size()));
}

template <typename LocalType, typename Proxy, typename Input, typename Emplace>
void ReadFieldNew(TypeList<std::unique_ptr<LocalType>>,
    Proxy& proxy,
    Input&& input,
    Emplace&& emplace,
    typename Plain<decltype(input.get())>::Calls* enable = nullptr)
{
    using Interface = typename Plain<decltype(input.get())>::Calls;
    emplace(MakeUnique<ProxyClient<Interface>>(std::move(input.get()), *proxy.m_loop));
}

// Callback class is needed because c++11 doesn't support auto lambda parameters.
// It's equivalent c++14: [proxy](auto&& params) { proxy->call(std::forward<decltype(params)>(params)...)
template <typename Proxy>
struct Callback
{
    Proxy m_proxy;

    template <typename... CallParams>
    auto operator()(CallParams&&... params) -> AUTO_RETURN(this->m_proxy->call(std::forward<CallParams>(params)...))
};

template <typename FnR, typename... FnParams, typename Proxy, typename Input, typename Emplace>
void ReadFieldNew(TypeList<std::function<FnR(FnParams...)>>, Proxy& proxy, Input&& input, Emplace&& emplace)
{
    using Interface = typename Plain<decltype(input.get())>::Calls;
    auto client = std::make_shared<ProxyClient<Interface>>(input.get(), *proxy.m_loop);
    emplace(Callback<decltype(client)>{std::move(client)});
};

template <typename Value>
struct RefEmplace
{
    RefEmplace(Value& value) : m_value(value) {}

    template <typename... Params>
    Value& operator()(Params&&... params)
    {
        return m_value = Value(std::forward<Params>(params)...);
    }
    Value& m_value;
};

template <typename... LocalTypes, typename Proxy, typename Input, typename... Values>
auto ReadFieldImpl(TypeList<LocalTypes...>, Priority<5>, Proxy& proxy, Input&& input, Values&&... values) ->
    typename std::enable_if<!Plain<Input>::can_get>::type
{
}

template <typename Param, typename Enable = void>
struct IsEmplace : std::false_type
{
};

template <typename Param>
struct IsEmplace<Param, typename std::enable_if<Param::emplace>::type> : public std::true_type
{
};

template <typename LocalType, typename Proxy, typename Input, typename Value>
void ReadFieldUpdate(TypeList<LocalType>,
    Proxy& proxy,
    Input&& input,
    Value&& value,
    decltype(ReadFieldNew(TypeList<LocalType>(), proxy, input, std::declval<Emplace<decltype(std::ref(value))>>()))*
        enable = nullptr)
{
    auto ref = std::ref(value);
    ReadFieldNew(TypeList<LocalType>(), proxy, input, Emplace<decltype(ref)>(ref));
}

template <size_t index, typename LocalType, typename Proxy, typename Input, typename Value>
void ReadOne(TypeList<LocalType> param,
    Proxy& proxy,
    Input&& input,
    Value&& value,
    typename std::enable_if<index != ProxyType<LocalType>::fields>::type* enable = nullptr)
{
    using Index = std::integral_constant<size_t, index>;
    using Struct = typename ProxyType<LocalType>::Struct;
    auto accessor = ProxyStruct<Struct>::get(Index());
    FieldInput<typename Struct::Reader, decltype(accessor)> field_reader{input.get(), accessor};
    auto&& field_value = value.*ProxyType<LocalType>::get(Index());
    ReadField(TypeList<decltype(field_value)>(), proxy, field_reader, field_value);
    ReadOne<index + 1>(param, proxy, input, value);
}

template <size_t index, typename LocalType, typename Proxy, typename Input, typename Value>
void ReadOne(TypeList<LocalType> param,
    Proxy& proxy,
    Input& input,
    Value& value,
    typename std::enable_if<index == ProxyType<LocalType>::fields>::type* enable = nullptr)
{
}

template <typename LocalType, typename Proxy, typename Input, typename Value>
void ReadFieldUpdate(TypeList<LocalType> param,
    Proxy& proxy,
    Input&& input,
    Value&& value,
    typename ProxyType<LocalType>::Struct* enable = nullptr)
{
    ReadOne<0>(param, proxy, input, value);
}

template <typename LocalType, typename Proxy, typename Input, typename Value>
void ReadFieldImpl(TypeList<LocalType>,
    Priority<4>,
    Proxy& proxy,
    Input&& input,
    Value&& value,
    typename std::enable_if<!IsEmplace<Value>::value>::type* enable = nullptr)
{
    ReadFieldUpdate(TypeList<Plain<LocalType>>(), proxy, input, std::forward<Value>(value));
}

template <typename LocalType, typename Proxy, typename Input, typename Emplace>
void ReadFieldImpl(TypeList<LocalType>,
    Priority<3>,
    Proxy& proxy,
    Input&& input,
    Emplace&& emplace,
    decltype(
        ReadFieldNew(TypeList<Plain<LocalType>>(), proxy, input, std::forward<Emplace>(emplace)))* enable = nullptr)
{
    ReadFieldNew(TypeList<Plain<LocalType>>(), proxy, input, std::forward<Emplace>(emplace));
}

template <typename LocalType, typename Proxy, typename Input, typename Emplace>
void ReadFieldImpl(TypeList<LocalType>,
    Priority<2>,
    Proxy& proxy,
    Input&& input,
    Emplace&& emplace,
    typename std::enable_if<!std::is_const<decltype(emplace())>::value,
        decltype(ReadFieldUpdate(TypeList<Plain<LocalType>>(), proxy, input, emplace()))>::type* enable = nullptr)
{
    auto&& ref = emplace();
    ReadFieldUpdate(TypeList<Plain<LocalType>>(), proxy, input, ref);
}

template <typename LocalType, typename Proxy, typename Input, typename Emplace>
void ReadFieldImpl(TypeList<LocalType>,
    Priority<1>,
    Proxy& proxy,
    Input&& input,
    Emplace&& emplace,
    decltype(ReadFieldUpdate(TypeList<Plain<LocalType>>(), proxy, input, std::declval<Plain<LocalType>&>()))* enable =
        nullptr)
{
    Plain<LocalType> temp;
    ReadFieldUpdate(TypeList<Plain<LocalType>>(), proxy, input, temp);
    emplace(std::move(temp));
}

template <typename LocalTypes, typename Proxy, typename Input, typename... Values>
void ReadField(LocalTypes, Proxy& proxy, Input&& input, Values&&... values)
{
    ReadFieldImpl(LocalTypes(), Priority<5>(), proxy, input, std::forward<Values>(values)...);
}

template <typename LocalTypes, typename Proxy, typename Input, typename... Values>
void ClientReadField(LocalTypes, Proxy& proxy, Input&& input, Values&&... values)
{
    ReadField(LocalTypes(), proxy, input, std::forward<Values>(values)...);
}

template <typename LocalType, typename Proxy, typename Input, typename Value>
void ClientReadField(TypeList<LocalType*>, Proxy& proxy, Input&& input, Value* value)
{
    if (value) {
        ReadField(TypeList<LocalType>(), proxy, input, *value);
    }
}

template <typename... LocalTypes, typename Value, typename Output>
void BuildField(TypeList<LocalTypes...>,
    Priority<2>,
    EventLoop& loop,
    Value&& value,
    Output&& output,
    typename std::enable_if<!Plain<Output>::can_set>::type* enable = nullptr)
{
}

template <typename Value, typename Output>
void BuildField(TypeList<std::string>, Priority<1>, EventLoop& loop, Value&& value, Output&& output)
{
    auto result = output.set(value.size());
    memcpy(result.begin(), value.data(), value.size());
}

template <typename Interface, typename Impl>
kj::Own<ProxyServer<Interface>> MakeServer(Impl impl, EventLoop& loop)
{
    return kj::heap<ProxyServer<Interface>>(std::move(impl), loop);
}

template <typename Value, typename FnR, typename... FnParams, typename Output>
void BuildField(TypeList<std::function<FnR(FnParams...)>>,
    Priority<1>,
    EventLoop& loop,
    Value&& value,
    Output&& output)
{
    using Interface = typename Plain<Output>::Type::Calls;
    // fixme: probably std::forward instead of move
    output.set(kj::heap<ProxyServer<Interface>>(MakeProxyCallback(std::move(value)), loop));
}

template <typename LocalType, typename Value, typename Output>
void BuildField(TypeList<LocalType*>, Priority<3>, EventLoop& loop, Value&& value, Output&& output)
{
    if (value) {
        output.setWant();
        // FIXME std::move probably wrong
        BuildField(TypeList<LocalType>(), BuildFieldPriority(), loop, std::move(*value), output);
    }
}

template <typename LocalType, typename Value, typename Output>
void BuildField(TypeList<std::shared_ptr<LocalType>>, Priority<1>, EventLoop& loop, Value&& value, Output&& output)
{
    if (value) {
        BuildField(TypeList<LocalType>(), BuildFieldPriority(), loop, *value, output);
    }
}

template <typename LocalType, typename Value, typename Output>
void BuildField(TypeList<std::vector<LocalType>>, Priority<1>, EventLoop& loop, Value&& value, Output&& output)
{
    // FIXME dedup with set handler below
    auto list = output.set(value.size());
    size_t i = 0;
    for (auto& elem : value) {
        BuildField(TypeList<LocalType>(), BuildFieldPriority(), loop, std::move(elem),
            ListOutput<typename decltype(list)::Builds>(list, i));
        ++i;
    }
}

template <typename LocalType, typename Value, typename Output>
void BuildField(TypeList<std::set<LocalType>>, Priority<1>, EventLoop& loop, Value&& value, Output&& output)
{
    // FIXME dededup with vector handler above
    auto list = output.set(value.size());
    size_t i = 0;
    for (const auto& elem : value) {
        BuildField(TypeList<LocalType>(), BuildFieldPriority(), loop, elem,
            ListOutput<typename decltype(list)::Builds>(list, i));
        ++i;
    }
}

template <typename KeyLocalType, typename ValueLocalType, typename Value, typename Output>
void BuildField(TypeList<std::map<KeyLocalType, ValueLocalType>>,
    Priority<1>,
    EventLoop& loop,
    Value&& value,
    Output&& output)
{
    // FIXME dededup with vector handler above
    auto list = output.set(value.size());
    size_t i = 0;
    for (const auto& elem : value) {
        BuildField(TypeList<std::pair<KeyLocalType, ValueLocalType>>(), BuildFieldPriority(), loop, elem,
            ListOutput<typename decltype(list)::Builds>(list, i));
        ++i;
    }
}

template <typename Value>
::capnp::Void BuildPrimitive(EventLoop& loop, Value&&, TypeList<::capnp::Void>)
{
    return {};
}

template <typename LocalType, typename Value>
LocalType BuildPrimitive(EventLoop& loop,
    const Value& value,
    TypeList<LocalType>,
    typename std::enable_if<std::is_enum<Value>::value>::type* enable = nullptr)
{
    return static_cast<LocalType>(value);
}

template <typename LocalType, typename Value>
LocalType BuildPrimitive(EventLoop& loop,
    const Value& value,
    TypeList<LocalType>,
    typename std::enable_if<std::is_integral<Value>::value, int>::type* enable = nullptr)
{
    static_assert(
        std::numeric_limits<LocalType>::lowest() <= std::numeric_limits<Value>::lowest(), "mismatched integral types");
    static_assert(
        std::numeric_limits<LocalType>::max() >= std::numeric_limits<Value>::max(), "mismatched integral types");
    return value;
}

template <typename LocalType, typename Value>
LocalType BuildPrimitive(EventLoop& loop,
    const Value& value,
    TypeList<LocalType>,
    typename std::enable_if<std::is_floating_point<Value>::value>::type* enable = nullptr)
{
    static_assert(std::is_same<Value, LocalType>::value,
        "mismatched floating point types. please fix message.capnp type declaration to match wrapped interface");
    return value;
}

// FIXME: Should take rvalue reference to impl
template <typename Impl, typename Client>
Client BuildPrimitive(EventLoop& loop, std::unique_ptr<Impl>&& impl, TypeList<Client>)
{
    using Interface = typename Client::Calls;
    return MakeServer<Interface>(std::move(impl), loop);
}

template <typename LocalType, typename Value, typename Output>
void BuildField(TypeList<boost::optional<LocalType>>, Priority<1>, EventLoop& loop, Value&& value, Output&& output)
{
    if (value) {
        // FIXME: should std::move value if destvalue is rref?
        BuildField(TypeList<LocalType>(), BuildFieldPriority(), loop, *value, output);
    }
}

template <typename Output>
void BuildField(TypeList<std::exception>, Priority<1>, EventLoop& loop, const std::exception& value, Output&& output)
{
    BuildField(TypeList<std::string>(), BuildFieldPriority(), loop, std::string(value.what()), output);
}

template <typename Output>
struct KeyOutput
{
    Output& m_output;
    template <typename... Params>
    auto set(Params&&... params) -> AUTO_RETURN(this->m_output.initKey(std::forward<Params>(params)...))
};

template <typename Output, typename Enable = void>
struct ValueOutput
{
    Output& m_output;

    template <typename Param>
    void set(Param&& param)
    {
        m_output.setValue(std::forward<Param>(param));
    }
    using Type = typename CapSetterMethodTraits<decltype(&Output::setValue)>::Type;
};

template <typename Output>
struct ValueOutput<Output, Void<decltype(std::declval<Output>().initValue())>>
{
    Output& m_output;
    template <typename... Params>
    auto set(Params&&... params) -> AUTO_RETURN(this->m_output.initValue(std::forward<Params>(params)...))
};

template <typename KeyLocalType, typename ValueLocalType, typename Value, typename Output>
void BuildField(TypeList<std::pair<KeyLocalType, ValueLocalType>>,
    Priority<1>,
    EventLoop& loop,
    Value&& value,
    Output&& output)
{
    auto kv = output.set();
    BuildField(TypeList<KeyLocalType>(), BuildFieldPriority(), loop, value.first, KeyOutput<decltype(kv)>{kv});
    BuildField(TypeList<ValueLocalType>(), BuildFieldPriority(), loop, value.second, ValueOutput<decltype(kv)>{kv});
}

template <typename KeyLocalType, typename ValueLocalType, typename Value, typename Output>
void BuildField(TypeList<std::tuple<KeyLocalType, ValueLocalType>>,
    Priority<1>,
    EventLoop& loop,
    Value&& value,
    Output&& output)
{
    auto kv = output.set();
    BuildField(TypeList<KeyLocalType>(), BuildFieldPriority(), loop, std::get<0>(value), KeyOutput<decltype(kv)>{kv});
    BuildField(
        TypeList<ValueLocalType>(), BuildFieldPriority(), loop, std::get<1>(value), ValueOutput<decltype(kv)>{kv});
}

template <typename LocalType, typename Output, typename Value>
void BuildField(TypeList<const LocalType>, Priority<0>, EventLoop& loop, Value&& value, Output&& output)
{
    BuildField(TypeList<LocalType>(), BuildFieldPriority(), loop, std::forward<Value>(value), output);
}

template <typename LocalType, typename Output, typename Value>
void BuildField(TypeList<LocalType&>,
    Priority<0>,
    EventLoop& loop,
    Value&& value,
    Output&& output,
    void* enable = nullptr)
{
    BuildField(TypeList<LocalType>(), BuildFieldPriority(), loop, std::forward<Value>(value), output);
}

template <typename LocalType, typename Output, typename Value>
void BuildField(TypeList<LocalType&&>, Priority<0>, EventLoop& loop, Value&& value, Output&& output)
{
    BuildField(TypeList<LocalType>(), BuildFieldPriority(), loop, std::forward<Value>(value), output);
}

template <typename LocalType, typename Output, typename Value>
void BuildField(TypeList<LocalType>, Priority<0>, EventLoop& loop, Value&& value, Output&& output)
{
    output.set(BuildPrimitive(loop, std::forward<Value>(value), TypeList<typename Plain<Output>::Type>()));
}

template <size_t index, typename LocalType, typename Output, typename Value>
void BuildOne(TypeList<LocalType> param,
    EventLoop& loop,
    Value&& value,
    Output&& output,
    typename std::enable_if < index<ProxyType<LocalType>::fields>::type * enable = nullptr)
{
    using Index = std::integral_constant<size_t, index>;
    using Struct = typename ProxyType<LocalType>::Struct;
    auto accessor = ProxyStruct<Struct>::get(Index());
    FieldOutput<typename Struct::Builder, decltype(accessor)> field_output{output, accessor};
    auto&& field_value = value.*ProxyType<LocalType>::get(Index());
    BuildField(TypeList<decltype(field_value)>(), BuildFieldPriority(), loop, field_value, field_output);
    BuildOne<index + 1>(param, loop, value, output);
}

template <size_t index, typename LocalType, typename Output, typename Value>
void BuildOne(TypeList<LocalType> param,
    EventLoop& loop,
    Value& value,
    Output& output,
    typename std::enable_if<index == ProxyType<LocalType>::fields>::type* enable = nullptr)
{
}

template <typename LocalType, typename Value, typename Output>
void BuildField(TypeList<LocalType> local_type,
    Priority<1>,
    EventLoop& loop,
    Value&& value,
    Output&& output,
    typename ProxyType<LocalType>::Struct* enable = nullptr)
{
    BuildOne<0>(local_type, loop, value, output.set());
}

template <typename LocalType, typename Proxy, typename Input, typename Output, typename Fn, typename... FnParams>
bool PassField(TypeList<LocalType*>, Proxy& proxy, Input&& input, Output&& output, Fn&& fn, FnParams&&... fn_params)
{
    boost::optional<Plain<LocalType>> param;
    bool want = input.want();
    if (want) {
        ReadField(TypeList<LocalType>(), proxy, input, Emplace<decltype(param)>(param));
        if (!param) param.emplace();
    }
    if (!fn(std::forward<FnParams>(fn_params)..., param ? &*param : nullptr)) return false;
    if (want) BuildField(TypeList<LocalType>(), BuildFieldPriority(), *proxy.m_loop, *param, output);
    return true;
}

template <typename LocalType, typename Proxy, typename Input, typename Output, typename Fn, typename... FnParams>
bool PassField(TypeList<LocalType>, Proxy& proxy, Input&& input, Output&& output, Fn&& fn, FnParams&&... fn_params)
{
    boost::optional<Plain<LocalType>> param;
    ReadField(TypeList<LocalType>(), proxy, input, Emplace<decltype(param)>(param));
    if (!fn(std::forward<FnParams>(fn_params)..., *param)) return false;
    BuildField(TypeList<LocalType>(), BuildFieldPriority(), *proxy.m_loop, *param, output);
    return true;
}

template <typename Derived, size_t N = 0>
struct FieldChainHelper
{
    template <typename Arg1, typename Arg2, typename ParamList, typename NextFn, typename... NextFnArgs>
    void handleChain(Arg1&& arg1, Arg2&& arg2, ParamList, NextFn&& next_fn, NextFnArgs&&... next_fn_args)
    {
        using S = Split<N, ParamList>;
        handleChain(std::forward<Arg1>(arg1), std::forward<Arg2>(arg2), typename S::First());
        next_fn.handleChain(std::forward<Arg1>(arg1), std::forward<Arg2>(arg2), typename S::Second(),
            std::forward<NextFnArgs>(next_fn_args)...);
    }

    template <typename Arg1, typename Arg2, typename ParamList>
    void handleChain(Arg1&& arg1, Arg2&& arg2, ParamList)
    {
        static_cast<Derived*>(this)->handleField(std::forward<Arg1>(arg1), std::forward<Arg2>(arg2), ParamList());
    }
};

struct FieldChain : FieldChainHelper<FieldChain, 0>
{
    template <typename Arg1, typename Arg2, typename ParamList>
    void handleField(Arg1&&, Arg2&&, ParamList)
    {
    }
};

// template <typename Exception, typename Accessor>
// FIXME struct ClientException
// https://stackoverflow.com/a/7858971

template <typename Exception, typename Accessor>
struct ClientException
{
    ClientException(Accessor accessor) : m_accessor(accessor) {}

    struct BuildParams : FieldChainHelper<BuildParams, 0>
    {
        template <typename Proxy, typename Params, typename ParamList>
        void handleField(Proxy& proxy, Params& params, ParamList)
        {
        }

        BuildParams(ClientException* client_exception) : m_client_exception(client_exception) {}
        ClientException* m_client_exception;
    };

    struct ReadResults : FieldChainHelper<ReadResults, 0>
    {
        template <typename Proxy, typename Results, typename ParamList>
        void handleField(Proxy& proxy, Results& results, ParamList)
        {
            FieldInput<Results, Accessor> input(results, m_client_exception->m_accessor);
            if (input.has()) {
                ReadField(TypeList<Exception>(), proxy, input, ThrowEmplace<Exception>());
            }
        }

        ReadResults(ClientException* client_exception) : m_client_exception(client_exception) {}
        ClientException* m_client_exception;
    };

    Accessor m_accessor;
};

template <typename Exception, typename Accessor>
ClientException<Exception, Accessor> MakeClientException(Accessor accessor)
{
    return {std::move(accessor)};
}

template <int argn, typename Accessor, typename... Types>
struct ClientParam
{
    ClientParam(Accessor accessor, Types&... values) : m_accessor(accessor), m_values(values...) {}

    struct BuildParams : FieldChainHelper<BuildParams, sizeof...(Types)>
    {
        template <typename Params, typename ParamList>
        void handleField(EventLoop& loop, Params& params, ParamList)
        {
            CallBuild<0>(ParamList(), loop, params, Priority<1>());
        }

        template <size_t I, typename FieldParams, typename Params, typename... Values>
        void CallBuild(FieldParams field_params,
            EventLoop& loop,
            Params& params,
            PriorityEnable<1, (I < sizeof...(Types))> priority,
            Values&&... values)
        {
            // std::move below wonky
            CallBuild<I + 1>(field_params, loop, params, priority, std::forward<Values>(values)...,
                std::move(std::get<I>(m_client_param->m_values)));
        }

        template <size_t I, typename FieldParams, typename Params, typename... Values>
        void CallBuild(FieldParams field_params, EventLoop& loop, Params& params, Priority<0>, Values&&... values)
        {
            using Output = FieldOutput<Params, Accessor>;

            BuildField(field_params, BuildFieldPriority(), loop, std::forward<Values>(values)...,
                Output(params, m_client_param->m_accessor));
        }

        BuildParams(ClientParam* client_param) : m_client_param(client_param) {}
        ClientParam* m_client_param;
    };

    struct ReadResults : FieldChainHelper<ReadResults, sizeof...(Types)>
    {
        template <typename Proxy, typename Results, typename ParamList>
        void handleField(Proxy& proxy, Results& results, ParamList)
        {
            FieldInput<Results, Accessor> reader(results, m_client_param->m_accessor);
            readTuple<0>(ParamList(), proxy, reader);
        }

        static constexpr size_t argc = sizeof...(Types);

        template <int n, typename... Args>
        typename std::enable_if<(n < argc)>::type readTuple(Args&&... args)
        {
            readTuple<n + 1>(std::forward<Args>(args)..., std::get<n>(m_client_param->m_values));
        }

        template <int n, typename ParamList, typename Proxy, typename Input, typename... Args>
        typename std::enable_if<n == argc && 0 < argc>::type readTuple(ParamList,
            Proxy& proxy,
            Input&& input,
            Args&&... args)
        {
            ClientReadField(ParamList(), proxy, input, std::forward<Args>(args)...);
        }

        template <int n, typename ParamList, typename Proxy, typename Input>
        typename std::enable_if<n == 0 && argc == 0>::type readTuple(ParamList, Proxy&, Input&&)
        {
            //! Special case for argn == 0, no-param fields
        }

        ReadResults(ClientParam* client_param) : m_client_param(client_param) {}
        ClientParam* m_client_param;
    };

    Accessor m_accessor;
    std::tuple<Types&...> m_values;
};

template <int argn, typename Accessor, typename... Types>
ClientParam<argn, Accessor, Types...> MakeClientParam(Accessor accessor, Types&... values)
{
    return {std::move(accessor), values...};
}

template <typename A, typename B, typename C, typename D, typename Fn, typename... FnParams>
bool Call4(A&& a, B&& b, C&& c, D&& d, Fn&& fn, FnParams&&... fn_params)
{
    return fn(std::forward<A>(a), std::forward<B>(b), std::forward<C>(c), std::forward<D>(d),
        std::forward<FnParams>(fn_params)...);
}

template <typename A, typename B, typename C, typename D>
bool Call4(A&&, B&&, C&&, D&&)
{
    return true;
}

//! Safely convert char pointer to kj pointer.
static inline kj::byte* ByteCast(char* c) { return reinterpret_cast<kj::byte*>(c); }
static inline const kj::byte* ByteCast(const char* c) { return reinterpret_cast<const kj::byte*>(c); }

template <int argc, typename Accessor>
struct ServerField
{
    template <typename MethodParams,
        typename Server,
        typename Params,
        typename Results,
        typename Fn,
        typename... FwdParams>
    bool operator()(MethodParams,
        Server& server,
        const Params& params,
        Results& results,
        Fn&& fn,
        FwdParams&&... fwd_params) const
    {
        using SplitParams = Split<argc, MethodParams>;
        using Output = FieldOutput<Results, Accessor>;

        // FIXME Weird rvalue issue std::move below
        return PassField(typename SplitParams::First(), server, FieldInput<Params, Accessor>(params, m_accessor),
            Output(results, m_accessor), fn, typename SplitParams::Second(), server, params, results,
            std::forward<FwdParams>(fwd_params)...);
    }

    Accessor m_accessor;
};

template <typename Accessor>
struct ServerFieldRet
{
    template <typename Server, typename Params, typename Results, typename Fn, typename... FwdParams>
    bool operator()(TypeList<> local_types,
        Server& server,
        const Params& params,
        Results& results,
        Fn&& fn,
        FwdParams&&... fwd_params) const
    {
        using Output = FieldOutput<Results, Accessor>;

        // FIXME Weird rvalue issue static_cast below
        auto&& result = fn(local_types, server, params, results, static_cast<FwdParams&>(fwd_params)...);
        using FieldType = TypeList<decltype(result)>;
        BuildField(FieldType(), BuildFieldPriority(), *server.m_loop, std::move(result), Output(results, m_accessor));
        return true;
    }
    Accessor m_accessor;
};

template <typename Exception, typename Accessor>
struct ServerFieldExcept
{
    template <typename Server, typename Params, typename Results, typename Fn, typename... FwdParams>
    bool operator()(TypeList<> local_types,
        Server& server,
        const Params& params,
        Results& results,
        Fn&& fn,
        FwdParams&&... fwd_params) const
    {
        using Output = FieldOutput<Results, Accessor>;

        // FIXME Weird rvalue issue static_cast below
        try {
            fn(local_types, server, params, results, static_cast<FwdParams&>(fwd_params)...);
        } catch (const Exception& e) {
            using FieldType = TypeList<decltype(e)>;
            BuildField(FieldType(), BuildFieldPriority(), *server.m_loop, e, Output(results, m_accessor));
        }
        return true;
    }
    Accessor m_accessor;
};

template <int argc = 1, typename Accessor>
ServerField<argc, Accessor> MakeServerField(Accessor accessor)
{
    return {std::move(accessor)};
}

template <typename Accessor>
ServerFieldRet<Accessor> MakeServerFieldRet(Accessor accessor)
{
    return {std::move(accessor)};
}

template <typename Exception, typename Accessor>
ServerFieldExcept<Exception, Accessor> MakeServerFieldExcept(Accessor accessor)
{
    return {std::move(accessor)};
}

template <typename Request>
struct CapRequestTraits;

template <typename _Params, typename _Results>
struct CapRequestTraits<::capnp::Request<_Params, _Results>>
{
    using Params = _Params;
    using Results = _Results;
};

template <typename ProxyClient, typename ParamsBuilder>
struct ClientBuild
{
    ProxyClient& m_proxy_client;
    ParamsBuilder& m_params_builder;

    template <typename... Params>
    void operator()(Params&&... params)
    {
        m_proxy_client.buildParams(m_params_builder, params...);
    }
};

template <typename ProxyClient, typename ResultsReader>
struct ClientRead
{
    ProxyClient& m_proxy_client;
    ResultsReader& m_results_reader;

    template <typename... Results>
    void operator()(Results&&... results)
    {
        m_proxy_client.readResults(m_results_reader, results...);
    }
};

// TODO: Maybe replace this with fold
// TODO: Use this to replace BuildParams crap
template <typename F, typename Tuple, typename... Tuples>
void CallWithTupleParams(F&& f, Tuple& tuple, Tuples&... tuples)
{
    CallWithTupleParams(BindTuple(f, tuple), tuples...);
}

template <typename F>
void CallWithTupleParams(F&& f)
{
    f();
}

// FIXME remove
void BreakPoint();

template <typename Interface, typename Class>
template <typename MethodTraits, typename GetRequest, typename ProxyClient, typename... _Params>
void ProxyClientBase<Interface, Class>::invoke(MethodTraits,
    const GetRequest& get_request,
    ProxyClient& proxy_client,
    _Params&&... params)
{
    if (!m_loop) {
        std::cerr << "ProxyServerClient call made after event loop shutdown." << std::endl;
        return;
    }
    using MethodParams = typename MethodTraits::Fields;

    const auto waiting_thread = std::this_thread::get_id();
    std::promise<void> promise;
    m_loop->sync([&]() {
        auto request = (m_client.*get_request)(nullptr);
        using Request = CapRequestTraits<decltype(request)>;
        FieldChain().handleChain(*m_loop, request, MethodParams(), typename _Params::BuildParams{&params}...);
        // FIXME: This works with gcc, gives error on argc/argv overload with clang++
        // Should either strip it out removing ClientBuild class, buildParams
        // method definitino, and whatever else, or fix/replace
        // CallWithTupleParams(Make<ClientBuild>(proxy_client, request), params.m_values...);

        if (m_loop->m_debug) {
            printf("============================================================\n");
            printf("Client Request %s %p pid %i\n%s\n",
                ::capnp::Schema::from<typename Request::Params>().getShortDisplayName().cStr(), this, getpid(),
                request.toString().flatten().cStr());
        }
        if (waiting_thread == m_loop->m_thread_id) BreakPoint();
        KJ_ASSERT(waiting_thread != m_loop->m_thread_id,
            "Error: IPC call from IPC callback handler not supported (use async)");

        m_loop->m_task_set.add(request.send().then([&](::capnp::Response<typename Request::Results>&& response) {
            if (m_loop->m_debug) {
                printf("============================================================\n");
                printf("Client Response %s %p pid %i\n%s\n",
                    ::capnp::Schema::from<typename Request::Results>().getShortDisplayName().cStr(), this, getpid(),
                    response.toString().flatten().cStr());
            }

            // FIXME: This works with gcc, gives error on argc/argv overload with clang++
            // Should either strip it out removing ClientRead class, readResults
            // method definitino, and whatever else, or fix/replace
            // CallWithTupleParams(Make<ClientRead>(proxy_client, response), params.m_values...);
            try {
                FieldChain().handleChain(*this, response, MethodParams(), typename _Params::ReadResults{&params}...);
            } catch (...) {
                promise.set_exception(std::current_exception());
                return;
            }
            promise.set_value();
        }));
    });
    promise.get_future().get();
}

extern std::atomic<int> server_reqs;


// https://stackoverflow.com/a/29350490
template <class... Args, class T, class R>
auto resolved(R (T::*m)(Args...)) -> decltype(m)
{
    return m;
}

template <typename Server, typename Context, typename Method, typename... Fields>
bool serverInvoke(Priority<1>,
    decltype(resolved<Context>(&Server::invokeMethod))* enable,
    Server& server,
    Context& context,
    const Method&,
    const Fields&...)
{
    server.invokeMethod(context);
    return true;
}

template <typename Server,
    typename Context,
    typename MethodResult,
    typename Object,
    typename... MethodParams,
    typename... Fields>
bool serverInvoke(Priority<0>,
    void*,
    Server& server,
    Context& context,
    MethodResult (Object::*const& method)(MethodParams...),
    const Fields&... fields)
{
    auto params = context.getParams();
    auto results = context.getResults();
    using Params = decltype(params);
    using Results = decltype(results);
    auto call_field = [&](
        TypeList<>, Server& server, const Params& params, Results& results, MethodParams&... method_params) {
        // FIXME: maybe call context.releaseParams() here
        return ReturnTrue([&]() { return ((*server.m_impl).*method)(static_cast<MethodParams&&>(method_params)...); });
    };
    return Call4(TypeList<MethodParams...>(), server, params, results, fields..., call_field);
}

template <typename Server, typename Context, typename Method, typename... Fields>
bool serverInvokeDebug(Server& server, Context& context, const Method& method, const Fields&... fields)
{
    auto params = context.getParams();
    auto results = context.getResults();
    using Params = decltype(params);
    using Results = decltype(results);

    int req = ++server_reqs;
    if (server.m_loop->m_debug) {
        printf("============================================================\n");
        printf("Server Request %i %s %p pid %i\n%s\n", req,
            ::capnp::Schema::from<typename Params::Reads>().getShortDisplayName().cStr(), &server, getpid(),
            params.toString().flatten().cStr());
    }

    bool done;
    try {
        if (!serverInvoke(Priority<1>(), nullptr, server, context, method, fields...)) return false;
    } catch (...) {
        std::cerr << "serverInvoke unhandled exception" << std::endl
                  << boost::current_exception_diagnostic_information();
        throw;
    }

    if (server.m_loop->m_debug) {
        printf("============================================================\n");
        printf("Server Response %i %s %p pid %i\n%s\n", req,
            "(fix)" /*::capnp::Schema::from<::capnp::BuilderFor<Results>>().getShortDisplayName().cStr()*/, &server,
            getpid(), results.toString().flatten().cStr());
    }

    return true;
}

template <typename Server, typename Context, typename Method, typename... Fields>
kj::Promise<void> serverInvokeSync(Server& server, Context& context, const Method& method, const Fields&... fields)
{
    if (!serverInvokeDebug(server, context, method, fields...)) {
        throw std::logic_error("synchronous serverInvoke call blocked");
    }
    return kj::READY_NOW;
}


template <typename Server, typename Context, typename Method, typename... Fields>
kj::Promise<void> serverInvokeAsync(Server& server, Context& context, Method method, Fields... fields)
{
    auto future = kj::newPromiseAndFulfiller<void>();
    return future.promise.attach(server.async(MakeAsyncCallable(
        kj::mvCapture(future.fulfiller, kj::mvCapture(context, [&server, method, fields...](Context context,
                                                                   kj::Own<kj::PromiseFulfiller<void>> fulfiller) {
                          if (!serverInvokeDebug(server, context, method, fields...)) return false;
                          server.m_loop->fulfillPromise(*fulfiller, []() {});
                          return true;
                      })))));
}

} // namespace capnp
} // namespace interfaces

#endif // BITCOIN_INTERFACES_CAPNP_PROXY_H
