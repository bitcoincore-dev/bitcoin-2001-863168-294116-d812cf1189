#ifndef BITCOIN_INTERFACE_CAPNP_PROXY_H
#define BITCOIN_INTERFACE_CAPNP_PROXY_H

#include <interface/capnp/proxy.h>

#include <interface/capnp/util.h>
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

namespace interface {
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
    AsyncCallable(Callable&& callable) : m_callable(std::move(callable)) {}
    AsyncCallable(const AsyncCallable& other) : m_callable(std::move(other.m_callable)) {}
    ~AsyncCallable() noexcept {}
    ResultOf<Callable> operator()() { return m_callable(); }
    mutable Callable m_callable;
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

    //! Run callable in new asynchonous thread. Returns immediately.
    template <typename Callable>
    kj::Promise<void> async(Callable&& callable)
    {
        auto future = kj::newPromiseAndFulfiller<void>();
        return future.promise.attach(std::async(std::launch::async,
            MakeAsyncCallable(kj::mvCapture(future.fulfiller,
                kj::mvCapture(callable, [this](Callable callable, kj::Own<kj::PromiseFulfiller<void>> fulfiller) {
                                                fulfillPromise(*fulfiller, std::move(callable));
                                            })))));
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

//! Invoke callback returning Void if callback returns void
template <typename Callable,
    typename Result = ResultOf<Callable>,
    typename Enable = typename std::enable_if<!std::is_same<Result, void>::value>::type>
Result ReturnVoid(Callable&& callable)
{
    return callable();
}

template <typename Callable,
    typename Result = ResultOf<Callable>,
    typename Enable = typename std::enable_if<std::is_same<Result, void>::value>::type>
::capnp::Void ReturnVoid(Callable&& callable)
{
    callable();
    return {};
}

template <typename Reader, typename Accessor>
class FieldReader
{
    const Reader& m_reader;
    const Accessor& m_accessor;
    EventLoop& m_loop;

public:
    static constexpr bool can_get = !std::is_same<decltype(std::declval<Accessor>().getter), std::nullptr_t>::value;

    FieldReader(const Reader& reader, const Accessor& accessor, EventLoop& loop)
        : m_reader(reader), m_accessor(accessor), m_loop(loop)
    {
    }

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

    EventLoop& loop() const { return m_loop; }
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

template <typename Value, typename Reader, typename DestValue>
void ReadFieldUpdate(TypeList<boost::optional<Value>>, Reader&& reader, DestValue&& value)
{
    if (!reader.has()) {
        value.reset();
        return;
    }
    if (value) {
        ReadField(TypeList<Value>(), reader, *value);
    } else {
        ReadField(TypeList<Value>(), reader, Emplace<DestValue>(value));
    }
}

template <typename Value, typename Reader, typename DestValue>
void ReadFieldUpdate(TypeList<std::shared_ptr<Value>>, Reader&& reader, DestValue&& value)
{
    if (!reader.has()) {
        value.reset();
        return;
    }
    if (value) {
        ReadField(TypeList<Value>(), reader, *value);
    } else {
        ReadField(TypeList<Value>(), reader, Emplace<DestValue>(value));
    }
}

template <typename Value, typename Reader, typename DestValue>
void ReadFieldUpdate(TypeList<std::shared_ptr<const Value>>, Reader&& reader, DestValue&& value)
{

    if (!reader.has()) {
        value.reset();
        return;
    }
    ReadField(TypeList<Value>(), reader, Emplace<DestValue>(value));
}

template <typename Value, typename Reader, typename DestValue>
void ReadFieldUpdate(TypeList<std::vector<Value>>, Reader&& reader, DestValue&& value)
{
    auto data = reader.get();
    value.clear();
    value.reserve(data.size());
    for (auto&& item : data) {
        ReadField(TypeList<Value>(), MakeReader(item, reader.loop()), Emplace<DestValue>(value));
    }
}

template <typename Value, typename Reader, typename DestValue>
void ReadFieldUpdate(TypeList<std::set<Value>>, Reader&& reader, DestValue&& value)
{
    auto data = reader.get();
    value.clear();
    for (auto&& item : data) {
        ReadField(TypeList<Value>(), MakeReader(item, reader.loop()), Emplace<DestValue>(value));
    }
}

template <typename Key, typename Value, typename Reader, typename DestValue>
void ReadFieldUpdate(TypeList<std::map<Key, Value>>, Reader&& reader, DestValue&& value)
{
    auto data = reader.get();
    value.clear();
    for (auto&& item : data) {
        ReadField(TypeList<std::pair<Key, Value>>(), MakeReader(item, reader.loop()), Emplace<DestValue>(value));
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

template <typename Value, typename Reader, typename Emplace>
struct PairValueEmplace
{
    Reader& m_reader;
    Emplace& m_emplace;
    template <typename KeyTuple>
    void operator()(KeyTuple&& key_tuple)
    {
        ReadField(TypeList<Value>(), MakeReader(m_reader.get().getValue(), m_reader.loop()),
            MakeTupleEmplace(Make<Compose>(Get<1>(), Bind(m_emplace, std::piecewise_construct, key_tuple))));
    }
};

template <typename Key, typename Value, typename Reader, typename Emplace>
void ReadFieldNew(TypeList<std::pair<Key, Value>>, Reader&& reader, Emplace&& emplace)
{
    /* This could be simplified a lot with c++14 generic lambdas. All it is doing is:
    ReadField(TypeList<Key>(), MakeReader(reader.getKey(), reader.loop()), [&](auto&&... key_args) {
        ReadField(TypeList<Value>(), MakeReader(reader.getValue(), reader.loop()), [&](auto&&... value_args) {
            emplace(std::piecewise_construct, std::forward_as_tuple(key_args...),
    std::forward_as_tuple(value_args...));
        })
    });
    */
    ReadField(TypeList<Key>(), MakeReader(reader.get().getKey(), reader.loop()),
        MakeTupleEmplace(PairValueEmplace<Value, Reader, Emplace>{reader, emplace}));
}

template <typename Key, typename Value, typename Reader, typename Tuple>
void ReadFieldUpdate(TypeList<std::tuple<Key, Value>>, Reader&& reader, Tuple&& tuple)
{
    ReadField(TypeList<Key>(), MakeReader(reader.get().getKey(), reader.loop()), std::get<0>(tuple));
    ReadField(TypeList<Value>(), MakeReader(reader.get().getValue(), reader.loop()), std::get<1>(tuple));
}

template <int priority, bool enable>
using PriorityEnable = typename std::enable_if<enable, Priority<priority>>::type;

template <typename Value, typename Reader, typename Emplace>
void ReadFieldNew(TypeList<Value>,
    Reader&& reader,
    Emplace&& emplace,
    typename std::enable_if<std::is_enum<Value>::value>::type* enable = 0)
{
    emplace(static_cast<Value>(reader.get()));
}

template <typename Value, typename Reader, typename Emplace>
void ReadFieldNew(TypeList<Value>,
    Reader&& reader,
    Emplace&& emplace,
    typename std::enable_if<std::is_integral<Value>::value>::type* enable = 0)
{
    auto value = reader.get();
    if (value < std::numeric_limits<Value>::min() || value > std::numeric_limits<Value>::max()) {
        throw std::range_error("out of bound int received");
    }
    emplace(static_cast<Value>(value));
}

template <typename Value, typename Reader, typename Emplace>
void ReadFieldNew(TypeList<Value>,
    Reader&& reader,
    Emplace&& emplace,
    typename std::enable_if<std::is_floating_point<Value>::value>::type* enable = 0)
{
    auto value = reader.get();
    static_assert(std::is_same<Value, decltype(value)>::value, "floating point type mismatch");
    emplace(value);
}

template <typename Reader, typename Emplace>
void ReadFieldNew(TypeList<std::string>, Reader&& reader, Emplace&& emplace)
{
    auto data = reader.get();
    emplace(CharCast(data.begin()), data.size());
}

template <typename Impl, typename Reader, typename Emplace>
void ReadFieldNew(TypeList<std::unique_ptr<Impl>>,
    Reader&& reader,
    Emplace&& emplace,
    typename Plain<decltype(reader.get())>::Calls* enable = nullptr)
{
    using Interface = typename Plain<decltype(reader.get())>::Calls;
    emplace(MakeUnique<ProxyClient<Interface>>(std::move(reader.get()), reader.loop()));
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

template <typename FnR, typename... FnParams, typename Reader, typename Emplace>
void ReadFieldNew(TypeList<std::function<FnR(FnParams...)>>, Reader&& reader, Emplace&& emplace)
{
    using Interface = typename Plain<decltype(reader.get())>::Calls;
    auto proxy = std::make_shared<ProxyClient<Interface>>(reader.get(), reader.loop());
    emplace(Callback<decltype(proxy)>{std::move(proxy)});
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

using BuildFieldPriority = Priority<2>;

template <typename... Params, typename Reader, typename... Values>
auto ReadFieldImpl(TypeList<Params...>, Priority<5>, Reader&& reader, Values&&... values) ->
    typename std::enable_if<!Plain<Reader>::can_get>::type
{
}

template <typename Value, typename Enable = void>
struct IsEmplace : std::false_type
{
};

template <typename Value>
struct IsEmplace<Value, typename std::enable_if<Value::emplace>::type> : public std::true_type
{
};

template <typename Param, typename Reader, typename Value>
void ReadFieldUpdate(TypeList<Param>,
    Reader&& reader,
    Value&& value,
    decltype(
        ReadFieldNew(TypeList<Param>(), reader, std::declval<Emplace<decltype(std::ref(value))>>()))* enable = nullptr)
{
    auto ref = std::ref(value);
    ReadFieldNew(TypeList<Param>(), reader, Emplace<decltype(ref)>(ref));
}

template <size_t index, typename Param, typename Reader, typename Value>
void ReadOne(TypeList<Param> param,
    Reader&& reader,
    Value&& value,
    typename std::enable_if<index != ProxyType<Param>::fields>::type* enable = nullptr)
{
    using Index = std::integral_constant<size_t, index>;
    using Struct = typename ProxyType<Param>::Struct;
    auto accessor = ProxyStruct<Struct>::get(Index());
    FieldReader<typename Struct::Reader, decltype(accessor)> field_reader{reader.get(), accessor, reader.loop()};
    auto&& field_value = value.*ProxyType<Param>::get(Index());
    ReadField(TypeList<decltype(field_value)>(), field_reader, field_value);
    ReadOne<index + 1>(param, reader, value);
}

template <size_t index, typename Param, typename Reader, typename Value>
void ReadOne(TypeList<Param> param,
    Reader& reader,
    Value& value,
    typename std::enable_if<index == ProxyType<Param>::fields>::type* enable = nullptr)
{
}

template <typename Param, typename Reader, typename Value>
void ReadFieldUpdate(TypeList<Param> param,
    Reader&& reader,
    Value&& value,
    typename ProxyType<Param>::Struct* enable = nullptr)
{
    ReadOne<0>(param, reader, value);
}

template <typename Param, typename Reader, typename Value>
void ReadFieldImpl(TypeList<Param>,
    Priority<4>,
    Reader&& reader,
    Value&& value,
    typename std::enable_if<!IsEmplace<Value>::value>::type* enable = nullptr)
{
    ReadFieldUpdate(TypeList<Plain<Param>>(), reader, std::forward<Value>(value));
}

template <typename Param, typename Reader, typename Emplace>
void ReadFieldImpl(TypeList<Param>,
    Priority<3>,
    Reader&& reader,
    Emplace&& emplace,
    decltype(ReadFieldNew(TypeList<Plain<Param>>(), reader, std::forward<Emplace>(emplace)))* enable = nullptr)
{
    ReadFieldNew(TypeList<Plain<Param>>(), reader, std::forward<Emplace>(emplace));
}

template <typename Param, typename Reader, typename Emplace>
void ReadFieldImpl(TypeList<Param>,
    Priority<2>,
    Reader&& reader,
    Emplace&& emplace,
    typename std::enable_if<!std::is_const<decltype(emplace())>::value,
        decltype(ReadFieldUpdate(TypeList<Plain<Param>>(), reader, emplace()))>::type* enable = nullptr)
{
    auto&& ref = emplace();
    ReadFieldUpdate(TypeList<Plain<Param>>(), reader, ref);
}

template <typename Param, typename Reader, typename Emplace>
void ReadFieldImpl(TypeList<Param>,
    Priority<1>,
    Reader&& reader,
    Emplace&& emplace,
    decltype(ReadFieldUpdate(TypeList<Plain<Param>>(), reader, std::declval<Plain<Param>&>()))* enable = nullptr)
{
    Plain<Param> temp;
    ReadFieldUpdate(TypeList<Plain<Param>>(), reader, temp);
    emplace(std::move(temp));
}

template <typename Params, typename Reader, typename... Values>
void ReadField(Params params, Reader&& reader, Values&&... values)
{
    ReadFieldImpl(params, Priority<5>(), reader, std::forward<Values>(values)...);
}

template <typename Params, typename Reader, typename... Values>
void ClientReadField(Params params, Reader&& reader, Values&&... values)
{
    ReadField(params, reader, std::forward<Values>(values)...);
}

template <typename Param, typename Reader, typename Value>
void ClientReadField(TypeList<Param*>, Reader&& reader, Value* value)
{
    if (value) {
        ReadField(TypeList<Param>(), reader, *value);
    }
}

template <typename FUCK, typename... Param, typename Value, typename Builder>
void BuildField(TypeList<Param...>,
    TypeList<FUCK>,
    Priority<2>,
    EventLoop& loop,
    Value&& value,
    Builder&& builder,
    typename std::enable_if<!Plain<Builder>::can_set>::type* enable = nullptr)
{
    // fixme should set want if pointer and non-null
}

template <typename FUCK, typename Value, typename Builder>
void BuildField(TypeList<std::string>, TypeList<FUCK>, Priority<1>, EventLoop& loop, Value&& value, Builder&& builder)
{
    auto result = builder.set(value.size());
    memcpy(result.begin(), value.data(), value.size());
}

template <typename Interface, typename Impl>
kj::Own<ProxyServer<Interface>> MakeServer(Impl impl, EventLoop& loop)
{
    return kj::heap<ProxyServer<Interface>>(std::move(impl), loop);
}

template <typename FUCK, typename Value, typename FnR, typename... FnParams, typename Builder>
void BuildField(TypeList<std::function<FnR(FnParams...)>>,
    TypeList<FUCK>,
    Priority<1>,
    EventLoop& loop,
    Value&& value,
    Builder&& builder)
{
    using Interface = typename Plain<Builder>::Type::Calls;
    // fixme: probably std::forward instead of move
    builder.set(kj::heap<ProxyServer<Interface>>(MakeProxyCallback(std::move(value)), loop));
}

template <typename FUCK, typename Param, typename Value, typename Builder>
void BuildField(TypeList<Param*>, TypeList<FUCK>, Priority<1>, EventLoop& loop, Value&& value, Builder&& builder)
{
    if (value) {
        // FIXME std::move probably wrong
        BuildField(TypeList<Param>(), TypeList<FUCK>(), Priority<2>(), loop, std::move(*value), builder);
    }
}

template <typename FUCK, typename Param, typename Value, typename Builder>
void BuildField(TypeList<std::shared_ptr<Param>>,
    TypeList<FUCK>,
    Priority<1>,
    EventLoop& loop,
    Value&& value,
    Builder&& builder)
{
    if (value) {
        BuildField(TypeList<Param>(), TypeList<FUCK>(), Priority<2>(), loop, *value, builder);
    }
}

template <typename FUCK, typename Param, typename Value, typename Builder>
void BuildField(TypeList<std::vector<Param>>,
    TypeList<FUCK>,
    Priority<1>,
    EventLoop& loop,
    Value&& value,
    Builder&& builder)
{
    // FIXME dedup with set handler below
    auto list = builder.set(value.size());
    size_t i = 0;
    for (auto& elem : value) {
        BuildField(TypeList<Param>(), TypeList<FUCK>(), Priority<2>(), loop, std::move(elem),
            ListElemBuilder<typename decltype(list)::Builds>(list, i));
        ++i;
    }
}

template <typename FUCK, typename Param, typename Value, typename Builder>
void BuildField(TypeList<std::set<Param>>,
    TypeList<FUCK>,
    Priority<1>,
    EventLoop& loop,
    Value&& value,
    Builder&& builder)
{
    // FIXME dededup with vector handler above
    auto list = builder.set(value.size());
    size_t i = 0;
    for (const auto& elem : value) {
        BuildField(TypeList<Param>(), TypeList<FUCK>(), Priority<2>(), loop, elem,
            ListElemBuilder<typename decltype(list)::Builds>(list, i));
        ++i;
    }
}

template <typename FUCK, typename K, typename V, typename Value, typename Builder>
void BuildField(TypeList<std::map<K, V>>,
    TypeList<FUCK>,
    Priority<1>,
    EventLoop& loop,
    Value&& value,
    Builder&& builder)
{
    // FIXME dededup with vector handler above
    auto list = builder.set(value.size());
    size_t i = 0;
    for (const auto& elem : value) {
        BuildField(TypeList<std::pair<K, V>>(), TypeList<FUCK>(), Priority<2>(), loop, elem,
            ListElemBuilder<typename decltype(list)::Builds>(list, i));
        ++i;
    }
}

template <typename Value>
::capnp::Void BuildPrimitive(EventLoop& loop, Value&&, TypeList<::capnp::Void>)
{
    return {};
}

template <typename Param, typename Value>
Param BuildPrimitive(EventLoop& loop,
    const Value& value,
    TypeList<Param>,
    typename std::enable_if<std::is_enum<Value>::value>::type* enable = nullptr)
{
    return static_cast<Param>(value);
}

template <typename Param, typename Value>
Param BuildPrimitive(EventLoop& loop,
    const Value& value,
    TypeList<Param>,
    typename std::enable_if<std::is_integral<Value>::value, int>::type* enable = nullptr)
{
    static_assert(
        std::numeric_limits<Param>::lowest() <= std::numeric_limits<Value>::lowest(), "mismatched integral types");
    static_assert(std::numeric_limits<Param>::max() >= std::numeric_limits<Value>::max(), "mismatched integral types");
    return value;
}

template <typename Param, typename Value>
Param BuildPrimitive(EventLoop& loop,
    const Value& value,
    TypeList<Param>,
    typename std::enable_if<std::is_floating_point<Value>::value>::type* enable = nullptr)
{
    static_assert(std::is_same<Value, Param>::value,
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

template <typename FUCK, typename Param, typename Value, typename Builder>
void BuildField(TypeList<boost::optional<Param>>,
    TypeList<FUCK>,
    Priority<1>,
    EventLoop& loop,
    Value&& value,
    Builder&& builder)
{
    if (value) {
        // FIXME: should std::move value if destvalue is rref?
        BuildField(TypeList<Param>(), TypeList<FUCK>(), Priority<2>(), loop, *value, builder);
    }
}

template <typename Builder>
struct KeyBuilder
{
    Builder& m_builder;
    template <typename... Params>
    auto set(Params&&... params) -> AUTO_RETURN(this->m_builder.initKey(std::forward<Params>(params)...))
};

template <typename Builder, typename Enable = void>
struct ValueBuilder
{
    Builder& m_builder;

    template <typename Param>
    void set(Param&& param)
    {
        m_builder.setValue(std::forward<Param>(param));
    }
    using Type = typename SetterTraits<decltype(&Builder::setValue)>::Type;
};

template <typename Builder>
struct ValueBuilder<Builder, Void<decltype(std::declval<Builder>().initValue())>>
{
    Builder& m_builder;
    template <typename... Params>
    auto set(Params&&... params) -> AUTO_RETURN(this->m_builder.initValue(std::forward<Params>(params)...))
};

template <typename FUCK, typename K, typename V, typename Value, typename Builder>
void BuildField(TypeList<std::pair<K, V>>,
    TypeList<FUCK>,
    Priority<1>,
    EventLoop& loop,
    Value&& value,
    Builder&& builder)
{
    auto kv = builder.set();
    BuildField(TypeList<K>(), TypeList<FUCK>(), Priority<2>(), loop, value.first, KeyBuilder<decltype(kv)>{kv});
    BuildField(TypeList<V>(), TypeList<FUCK>(), Priority<2>(), loop, value.second, ValueBuilder<decltype(kv)>{kv});
}

template <typename FUCK, typename K, typename V, typename Value, typename Builder>
void BuildField(TypeList<std::tuple<K, V>>,
    TypeList<FUCK>,
    Priority<1>,
    EventLoop& loop,
    Value&& value,
    Builder&& builder)
{
    auto kv = builder.set();
    BuildField(TypeList<K>(), TypeList<FUCK>(), Priority<2>(), loop, std::get<0>(value), KeyBuilder<decltype(kv)>{kv});
    BuildField(
        TypeList<V>(), TypeList<FUCK>(), Priority<2>(), loop, std::get<1>(value), ValueBuilder<decltype(kv)>{kv});
}

template <typename FUCK, typename Param, typename Builder, typename Value>
void BuildField(TypeList<const Param>, TypeList<FUCK>, Priority<0>, EventLoop& loop, Value&& value, Builder&& builder)
{
    BuildField(TypeList<Param>(), TypeList<FUCK>(), Priority<2>(), loop, std::forward<Value>(value), builder);
}

template <typename FUCK, typename Param, typename Builder, typename Value>
void BuildField(TypeList<Param&>,
    TypeList<FUCK>,
    Priority<0>,
    EventLoop& loop,
    Value&& value,
    Builder&& builder,
    void* enable = nullptr)
{
    BuildField(TypeList<Param>(), TypeList<FUCK>(), Priority<2>(), loop, std::forward<Value>(value), builder);
}

template <typename FUCK, typename Param, typename Builder, typename Value>
void BuildField(TypeList<Param&&>, TypeList<FUCK>, Priority<0>, EventLoop& loop, Value&& value, Builder&& builder)
{
    BuildField(TypeList<Param>(), TypeList<FUCK>(), Priority<2>(), loop, std::forward<Value>(value), builder);
}

template <typename FUCK, typename Param, typename Builder, typename Value>
void BuildField(TypeList<Param>, TypeList<FUCK>, Priority<0>, EventLoop& loop, Value&& value, Builder&& builder)
{
    builder.set(BuildPrimitive(loop, std::forward<Value>(value), TypeList<typename Plain<Builder>::Type>()));
}

template <size_t index, typename Param, typename Builder, typename Value>
void BuildOne(TypeList<Param> param,
    EventLoop& loop,
    Value&& value,
    Builder&& builder,
    typename std::enable_if < index<ProxyType<Param>::fields>::type * enable = nullptr)
{
    using Index = std::integral_constant<size_t, index>;
    using Struct = typename ProxyType<Param>::Struct;
    auto accessor = ProxyStruct<Struct>::get(Index());
    FieldBuilder<typename Struct::Builder, decltype(accessor)> field_builder{builder, accessor};
    auto&& field_value = value.*ProxyType<Param>::get(Index());
    using FUCK = int;
    BuildField(TypeList<decltype(field_value)>(), TypeList<FUCK>(), Priority<2>(), loop, field_value, field_builder);
    BuildOne<index + 1>(param, loop, value, builder);
}

template <size_t index, typename Param, typename Builder, typename Value>
void BuildOne(TypeList<Param> param,
    EventLoop& loop,
    Value& value,
    Builder& builder,
    typename std::enable_if<index == ProxyType<Param>::fields>::type* enable = nullptr)
{
}

template <typename FUCK, typename Param, typename Value, typename Builder>
void BuildField(TypeList<Param> param,
    TypeList<FUCK>,
    Priority<1>,
    EventLoop& loop,
    Value&& value,
    Builder&& builder,
    typename ProxyType<Param>::Struct* enable = nullptr)
{
    BuildOne<0>(param, loop, value, builder.set());
}

template <typename FUCK, typename Value, typename Reader, typename Builder, typename Fn, typename... FnParams>
void PassField(TypeList<FUCK>, TypeList<Value*>, Reader&& reader, Builder&& builder, Fn&& fn, FnParams&&... fn_params)
{
    boost::optional<Plain<Value>> param;
    bool want = reader.want();
    if (want) ReadField(TypeList<Value>(), reader, Emplace<decltype(param)>(param));
    fn(std::forward<FnParams>(fn_params)..., param ? &*param : nullptr);
    if (want) BuildField(TypeList<Value>(), TypeList<FUCK>(), Priority<2>(), reader.loop(), *param, builder);
}

template <typename FUCK, typename Value, typename Reader, typename Builder, typename Fn, typename... FnParams>
void PassField(TypeList<FUCK>, TypeList<Value>, Reader&& reader, Builder&& builder, Fn&& fn, FnParams&&... fn_params)
{
    boost::optional<Plain<Value>> param;
    ReadField(TypeList<Value>(), reader, Emplace<decltype(param)>(param));
    fn(std::forward<FnParams>(fn_params)..., *param);
    BuildField(TypeList<Value>(), TypeList<FUCK>(), Priority<2>(), reader.loop(), *param, builder);
}

template <int argn, typename Accessor, typename... Types>
struct ClientParam
{
    ClientParam(Accessor accessor, Types&... values) : m_accessor(accessor), m_values(values...) {}

    struct BuildParams
    {
        template <typename MethodParams, typename Params, typename Fn, typename... FwdParams>
        void operator()(MethodParams method_params,
            EventLoop& loop,
            Params& params,
            Fn&& fn,
            FwdParams&&... fwd_params)
        {
            (*this)(method_params, loop, params);
            fn(typename Split<sizeof...(Types), MethodParams>::Second(), loop, params,
                std::forward<FwdParams>(fwd_params)...);
        }

        template <typename MethodParams, typename Params>
        void operator()(MethodParams, EventLoop& loop, Params& params)
        {
            using FieldParams = typename Split<sizeof...(Types), MethodParams>::First;
            CallBuild<0>(FieldParams(), loop, params, Priority<1>());
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
            using Builder = FieldBuilder<Params, Accessor>;

            BuildField(field_params, TypeList<typename Builder::Type>(), BuildFieldPriority(), loop,
                std::forward<Values>(values)..., Builder(params, m_client_param->m_accessor));
        }

        ClientParam* m_client_param;
    };

    struct ReadResults
    {
        template <typename MethodParams, typename Results, typename Fn, typename... FwdParams>
        void operator()(MethodParams method_params,
            EventLoop& loop,
            const Results& results,
            Fn&& fn,
            FwdParams&&... fwd_params)
        {
            (*this)(method_params, loop, results);
            fn(typename Split<sizeof...(Types), MethodParams>::Second(), loop, results,
                std::forward<FwdParams>(fwd_params)...);
        }

        template <typename MethodParams, typename Results>
        void operator()(MethodParams method_params, EventLoop& loop, Results& results)
        {
            FieldReader<Results, Accessor> reader(results, m_client_param->m_accessor, loop);
            readTuple<0>(method_params, reader);
        }

        static constexpr size_t argc = sizeof...(Types);

        template <int n, typename... Args>
        typename std::enable_if<(n < argc)>::type readTuple(Args&&... args)
        {
            readTuple<n + 1>(std::forward<Args>(args)..., std::get<n>(m_client_param->m_values));
        }

        template <int n, typename MethodParams, typename Reader, typename... Args>
        typename std::enable_if<n == argc && 0 < argc>::type readTuple(MethodParams, Reader&& reader, Args&&... args)
        {
            using FieldParams = typename Split<sizeof...(Types), MethodParams>::First;
            ClientReadField(FieldParams(), reader, std::forward<Args>(args)...);
        }

        template <int n, typename MethodParams, typename Reader>
        typename std::enable_if<n == 0 && argc == 0>::type readTuple(MethodParams, Reader&&)
        {
            //! Special case for argn == 0, no-param fields
        }

        ClientParam* m_client_param;
    };

    Accessor m_accessor;
    std::tuple<Types&...> m_values;
    ClientParam* m_client_param;
};

template <int argn, typename Accessor, typename... Types>
ClientParam<argn, Accessor, Types...> MakeClientParam(Accessor accessor, Types&... values)
{
    return {std::move(accessor), values...};
}

template <typename A, typename B, typename C, typename Fn, typename... FnParams>
void Call3(A&& a, B&& b, C&& c, Fn&& fn, FnParams&&... fn_params)
{
    fn(std::forward<A>(a), std::forward<B>(b), std::forward<C>(c), std::forward<FnParams>(fn_params)...);
}

template <typename A, typename B, typename C>
void Call3(A&&, B&&, C&&)
{
}

template <typename A, typename B, typename C, typename D, typename Fn, typename... FnParams>
void Call4(A&& a, B&& b, C&& c, D&& d, Fn&& fn, FnParams&&... fn_params)
{
    fn(std::forward<A>(a), std::forward<B>(b), std::forward<C>(c), std::forward<D>(d),
        std::forward<FnParams>(fn_params)...);
}

template <typename A, typename B, typename C, typename D>
void Call4(A&&, B&&, C&&, D&&)
{
}

//! Safely convert char pointer to kj pointer.
static inline kj::byte* ByteCast(char* c) { return reinterpret_cast<kj::byte*>(c); }
static inline const kj::byte* ByteCast(const char* c) { return reinterpret_cast<const kj::byte*>(c); }

template <int argc, typename Accessor>
struct ServerField
{
    template <typename MethodParams, typename Params, typename Results, typename Fn, typename... FwdParams>
    void operator()(MethodParams,
        EventLoop& loop,
        const Params& params,
        Results& results,
        Fn&& fn,
        FwdParams&&... fwd_params) const
    {
        using SplitParams = Split<argc, MethodParams>;
        using Builder = FieldBuilder<Results, Accessor>;

        // FIXME Weird rvalue issue std::move below
        PassField(TypeList<typename Builder::Type>(), typename SplitParams::First(),
            FieldReader<Params, Accessor>(params, m_accessor, loop), Builder(results, m_accessor), fn,
            typename SplitParams::Second(), loop, params, results, std::forward<FwdParams>(fwd_params)...);
    }

    Accessor m_accessor;
};

template <typename Accessor>
struct ServerFieldRet
{
    template <typename Params, typename Results, typename Fn, typename... FwdParams>
    void operator()(TypeList<> param_types,
        EventLoop& loop,
        const Params& params,
        Results& results,
        Fn&& fn,
        FwdParams&&... fwd_params) const
    {
        using Builder = FieldBuilder<Results, Accessor>;

        // FIXME Weird rvalue issue static_cast below
        auto&& result = fn(param_types, loop, params, results, static_cast<FwdParams&>(fwd_params)...);
        using FieldType = TypeList<decltype(result)>;
        BuildField(FieldType(), TypeList<typename Builder::Type>(), BuildFieldPriority(), loop, std::move(result),
            Builder(results, m_accessor));
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

template <typename Request>
struct RequestTraits;

template <typename _Params, typename _Results>
struct RequestTraits<::capnp::Request<_Params, _Results>>
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
        using Request = RequestTraits<decltype(request)>;
        Call3(MethodParams(), *m_loop, request, typename _Params::BuildParams{&params}...);
        CallWithTupleParams(Make<ClientBuild>(proxy_client, request), params.m_values...);

        if (m_loop->m_debug) {
            printf("============================================================\n");
            printf("Request %s %p pid %i\n%s\n",
                ::capnp::Schema::from<typename Request::Params>().getShortDisplayName().cStr(), this, getpid(),
                request.toString().flatten().cStr());
        }
        if (waiting_thread == m_loop->m_thread_id) BreakPoint();
        KJ_ASSERT(waiting_thread != m_loop->m_thread_id,
            "Error: IPC call from IPC callback handler not supported (use async)");

        m_loop->m_task_set.add(request.send().then([&](::capnp::Response<typename Request::Results>&& response) {
            if (m_loop->m_debug) {
                printf("============================================================\n");
                printf("Response %s %p pid %i\n%s\n",
                    ::capnp::Schema::from<typename Request::Results>().getShortDisplayName().cStr(), this, getpid(),
                    response.toString().flatten().cStr());
            }
            Call3(MethodParams(), *m_loop, response, typename _Params::ReadResults{&params}...);
            CallWithTupleParams(Make<ClientRead>(proxy_client, response), params.m_values...);
            promise.set_value();
        }));
    });
    promise.get_future().get();
}

template <typename Context,
    typename Object,
    typename Object2,
    typename MethodResult,
    typename... MethodParams,
    typename... Fields>
kj::Promise<void> serverInvoke(Context& context,
    EventLoop& loop,
    Object2& object,
    MethodResult (Object::*method)(MethodParams...),
    const Fields&... fields)
{
    auto params = context.getParams();
    auto results = context.getResults();
    using Params = decltype(params);
    using Results = decltype(results);
    auto fn = [&](
        TypeList<>, EventLoop& loop, const Params& params, Results& results, MethodParams&... method_params) {
        // FIXME: maybe call context.releaseParams() here
        return ReturnVoid([&]() { return (object.*method)(static_cast<MethodParams&&>(method_params)...); });
    };
    try {
        Call4(TypeList<MethodParams...>(), loop, params, results, fields..., fn);
    } catch (...) {
        std::cerr << "ProxyServerBase unhandled exception" << std::endl
                  << boost::current_exception_diagnostic_information();
        throw;
    }
    return kj::READY_NOW;
}

template <typename Interface, typename Class>
template <typename Context, typename Method, typename... Fields>
kj::Promise<void> ProxyServerBase<Interface, Class>::invokeMethod(Context& context,
    const Method& method,
    const Fields&... fields)
{
    return serverInvoke(context, m_loop, *m_impl, method, fields...);
}

template <typename Interface, typename Class>
template <typename Context, typename Method, typename... Fields>
kj::Promise<void> ProxyServerBase<Interface, Class>::invokeMethodAsync(Context& context,
    const Method& method,
    const Fields&... fields)
{
    return m_loop.async(kj::mvCapture(
        context, [this, method, fields...](Context context) { this->invokeMethod(context, method, fields...); }));
}

} // namespace capnp
} // namespace interface

#endif // BITCOIN_INTERFACE_CAPNP_PROXY_H
