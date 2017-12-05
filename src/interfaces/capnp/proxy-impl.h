#ifndef BITCOIN_INTERFACES_CAPNP_PROXY_IMPL_H
#define BITCOIN_INTERFACES_CAPNP_PROXY_IMPL_H

#include <interfaces/capnp/proxy.h>

#include <interfaces/capnp/proxy.capnp.h>
#include <interfaces/capnp/util.h>
#include <optional.h>
#include <serialize.h> // For CharCast
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

class EventLoop;

//! Handler for kj::TaskSet failed task events.
class LoggingErrorHandler : public kj::TaskSet::ErrorHandler
{
public:
    LoggingErrorHandler(EventLoop& loop) : m_loop(loop) {}
    void taskFailed(kj::Exception&& exception) override;
    EventLoop& m_loop;
};

//! Event loop implementation.
//!
//! Based on https://groups.google.com/d/msg/capnproto/TuQFF1eH2-M/g81sHaTAAQAJ
class EventLoop
{
public:
    //! Construct event loop object.
    //!
    //! @param[in]  thread  optional thread handle to join on destruction.
    EventLoop(const char* exe_name, std::thread&& thread = {});
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

    CleanupIt addCleanup(std::function<void()> fn)
    {
        return m_cleanup_fns.emplace(m_cleanup_fns.begin(), std::move(fn));
    }

    void removeCleanup(CleanupIt it) { m_cleanup_fns.erase(it); }

    const char* m_exe_name;
    ::interfaces::capnp::ThreadMap::Client m_thread_map;
    ::capnp::CapabilityServerSet<Thread> m_threads;
    kj::AsyncIoContext m_io_context;
    LoggingErrorHandler m_error_handler{*this};
    kj::TaskSet m_task_set{m_error_handler};
    std::thread m_thread;
    std::thread::id m_thread_id = std::this_thread::get_id();
    std::mutex m_post_mutex;
    std::function<void()> m_post_fn;
    CleanupList m_cleanup_fns;
    int m_wait_fd = -1;
    int m_post_fd = -1;
};


struct Waiter
{
    Waiter(EventLoop& loop) : m_loop(loop) {}

    ~Waiter()
    {
        if (m_result.valid()) { // true if an external thread is calling wait
            std::unique_lock<std::mutex> lock(m_mutex);
            std::future<void> result = std::move(m_result);
            assert(!m_result.valid());
            m_cv.notify_all();
            lock.unlock();
            result.wait();
        }
    }

    template <typename Fn>
    void post(Fn&& fn)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        assert(!m_fn);
        m_fn = std::move(fn);
        m_cv.notify_all();
    }

    template <class Predicate>
    void wait(std::unique_lock<std::mutex>& lock, Predicate pred)
    {
        m_cv.wait(lock, [&] {
            // Important for this to be while loop, not if statement to avoid a
            // lost-wakeup bug. A new m_fn and m_cv notification might be set
            // between the unlock & lock lines in this loop in the case where a
            // capnp response is sent and a brand new request is received here
            // before this thread relocks.
            while (m_fn) {
                auto fn = std::move(m_fn);
                m_fn = nullptr;
                lock.unlock();
                fn();
                lock.lock();
            }
            bool done = pred();
            return done;
        });
    }

    EventLoop& m_loop;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::string m_name;
    std::function<void()> m_fn;
    std::future<void> m_result;
};

template <>
struct ProxyServer<Thread> final : public Thread::Server
{
public:
    ProxyServer(Waiter& waiter) : m_waiter(waiter) {}

    kj::Promise<void> getName(GetNameContext context) override
    {
        context.getResults().setResult(m_waiter.m_name);
        return kj::READY_NOW;
    }

    Waiter& m_waiter;
};

struct ThreadContext
{
    std::unique_ptr<Waiter> waiter;
    std::map<EventLoop*, Optional<Thread::Client>> local_threads;
    std::map<EventLoop*, Optional<Thread::Client>> remote_threads;

    ~ThreadContext()
    {
        // FIXME
        for (auto& thread : remote_threads) {
            if (thread.second) {
                thread.first->sync([&] { auto client = std::move(*thread.second); });
            }
        }
    }
};

struct ClientInvokeContext : InvokeContext
{
    ThreadContext& client_thread;
    ClientInvokeContext(EventLoop& loop, ThreadContext& client_thread)
        : InvokeContext{loop}, client_thread{client_thread}
    {
    }
};

template <typename Output>
void BuildField(TypeList<>,
    Priority<1>,
    ClientInvokeContext& invoke_context,
    Output&& output,
    typename std::enable_if<std::is_same<decltype(output.init()), Context::Builder>::value>::type* enable = nullptr)
{
    Optional<Thread::Client>& local_thread = invoke_context.client_thread.local_threads[&invoke_context.loop];
    if (!local_thread) {
        local_thread =
            invoke_context.loop.m_threads.add(kj::heap<ProxyServer<Thread>>(*invoke_context.client_thread.waiter));
    }

    Optional<Thread::Client>& remote_thread = invoke_context.client_thread.remote_threads[&invoke_context.loop];
    if (!remote_thread) {
        // This code will only run if IPC client call is being made for the
        // first time on a new thread. After the first call, subsequent calls
        // will use the existing remote thread. This code will also never run at
        // all if the current thread is a remote thread created for a different
        // IPC client, because in that case PassField code (below) will have set
        // remote_thread to point to the calling thread.
        auto request = invoke_context.loop.m_thread_map.makeThreadRequest();
        request.setName(invoke_context.client_thread.waiter->m_name);
        remote_thread = request.send().getResult(); // Nonblocking due to capnp request piplineing.
    }

    auto context = output.init();
    context.setThread(*remote_thread);
    context.setCallbackThread(*local_thread);
}

extern thread_local ThreadContext g_thread_context;

template <typename Accessor, typename ServerContext, typename Fn, typename... Args>
auto PassField(TypeList<>, ServerContext& server_context, const Fn& fn, const Args&... args) ->
    typename std::enable_if<
        std::is_same<decltype(Accessor::get(server_context.call_context.getParams())), Context::Reader>::value,
        kj::Promise<typename ServerContext::CallContext>>::type
{
    const auto& params = server_context.call_context.getParams();
    Context::Reader context_arg = Accessor::get(params);
    auto future = kj::newPromiseAndFulfiller<typename ServerContext::CallContext>();
    auto& server = server_context.proxy_server;
    int req = server_context.req;
    auto invoke = MakeAsyncCallable(kj::mvCapture(future.fulfiller,
        kj::mvCapture(server_context.call_context,
            [&server, req, fn, args...](typename ServerContext::CallContext call_context,
                kj::Own<kj::PromiseFulfiller<typename ServerContext::CallContext>> fulfiller) {
                const auto& params = call_context.getParams();
                Context::Reader context_arg = Accessor::get(params);
                ServerContext server_context{server, call_context, req};
                {
                    Optional<Thread::Client>& thread_client = g_thread_context.remote_threads[server.m_loop];
                    TempSetter<Optional<Thread::Client>> temp_setter(thread_client, context_arg.getCallbackThread());
                    fn.invoke(server_context, args...);
                }
                KJ_IF_MAYBE(exception, kj::runCatchingExceptions([&]() {
                    server.m_loop->sync([&] {
                        auto fulfiller_dispose = kj::mv(fulfiller);
                        fulfiller_dispose->fulfill(kj::mv(call_context));
                    });
                }))
                {
                    server.m_loop->sync([&]() {
                        auto fulfiller_dispose = kj::mv(fulfiller);
                        fulfiller_dispose->reject(kj::mv(*exception));
                    });
                }
            })));

    auto thread_client = context_arg.getThread();
    return JoinPromises(
        server.m_loop->m_threads.getLocalServer(thread_client)
            .then([&server, invoke, req](kj::Maybe<Thread::Server&> perhaps) {
                KJ_IF_MAYBE(thread_server, perhaps)
                {
                    const auto& thread = static_cast<ProxyServer<Thread>&>(*thread_server);
                    LogIpc(*server.m_loop, "IPC server post request #%i {%s}\n", req, thread.m_waiter.m_name);
                    thread.m_waiter.post(std::move(invoke));
                }
                else
                {
                    LogIpc(*server.m_loop, "IPC server error request #%i {%s}, missing thread to execute request\n");
                    throw std::runtime_error("invalid thread handle");
                }
            }),
        kj::mv(future.promise));
}

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

template <typename Interface>
void destroyClient(ProxyClient<Interface>& proxy, bool remote)
{
    typename Interface::Client(std::move(proxy.m_client));
    proxy.Base::close(remote);
    proxy.m_loop = nullptr;
}

// two shutdown sequences need to be supported, one where event loop thread exits before class is destroyed, one where
// class being destroyed shuts down event loop.
//
// event loop thread exits
//  Base::close callback called
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
template <typename Interface, typename Impl>
ProxyClientBase<Interface, Impl>::ProxyClientBase(typename Interface::Client client, EventLoop& loop)
    : m_client(std::move(client)), m_loop(&loop)
{
    m_cleanup = loop.addCleanup([this]() { destroyClient(self(), true); });
    self().construct();
}

template <typename Interface, typename Impl>
ProxyClientBase<Interface, Impl>::~ProxyClientBase() noexcept
{
    self().destroy();
    if (m_loop) {
        m_loop->sync([&]() {
            m_loop->removeCleanup(m_cleanup);
            destroyClient(self(), false /* remote */);
        });
    }
}

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

template <typename LocalType, typename Input, typename DestValue>
void ReadFieldUpdate(TypeList<boost::optional<LocalType>>,
    InvokeContext& invoke_context,
    Input&& input,
    DestValue&& value)
{
    if (!input.has()) {
        value.reset();
        return;
    }
    if (value) {
        ReadFieldUpdate(TypeList<LocalType>(), invoke_context, input, *value);
    } else {
        ReadField(TypeList<LocalType>(), invoke_context, input, Emplace<DestValue>(value));
    }
}

template <typename LocalType, typename Input, typename DestValue>
void ReadFieldUpdate(TypeList<std::shared_ptr<LocalType>>,
    InvokeContext& invoke_context,
    Input&& input,
    DestValue&& value)
{
    if (!input.has()) {
        value.reset();
        return;
    }
    if (value) {
        ReadFieldUpdate(TypeList<LocalType>(), invoke_context, input, *value);
    } else {
        ReadField(TypeList<LocalType>(), invoke_context, input, Emplace<DestValue>(value));
    }
}

template <typename LocalType, typename Input, typename DestValue>
void ReadFieldUpdate(TypeList<LocalType*>, InvokeContext& invoke_context, Input&& input, DestValue&& value)
{
    if (value) {
        ReadFieldUpdate(TypeList<LocalType>(), invoke_context, std::forward<Input>(input), *value);
    }
}

template <typename LocalType, typename Input, typename DestValue>
void ReadFieldUpdate(TypeList<std::shared_ptr<const LocalType>>,
    InvokeContext& invoke_context,
    Input&& input,
    DestValue&& value)
{
    if (!input.has()) {
        value.reset();
        return;
    }
    ReadField(TypeList<LocalType>(), invoke_context, std::forward<Input>(input), Emplace<DestValue>(value));
}

template <typename LocalType, typename Input, typename DestValue>
void ReadFieldUpdate(TypeList<std::vector<LocalType>>, InvokeContext& invoke_context, Input&& input, DestValue&& value)
{
    auto data = input.get();
    value.clear();
    value.reserve(data.size());
    for (auto item : data) {
        ReadField(TypeList<LocalType>(), invoke_context, Make<ValueField>(item), Emplace<DestValue>(value));
    }
}

template <typename LocalType, typename Input, typename DestValue>
void ReadFieldUpdate(TypeList<std::set<LocalType>>, InvokeContext& invoke_context, Input&& input, DestValue&& value)
{
    auto data = input.get();
    value.clear();
    for (auto item : data) {
        ReadField(TypeList<LocalType>(), invoke_context, Make<ValueField>(item), Emplace<DestValue>(value));
    }
}

template <typename KeyLocalType, typename ValueLocalType, typename Input, typename DestValue>
void ReadFieldUpdate(TypeList<std::map<KeyLocalType, ValueLocalType>>,
    InvokeContext& invoke_context,
    Input&& input,
    DestValue&& value)
{
    auto data = input.get();
    value.clear();
    for (auto item : data) {
        ReadField(TypeList<std::pair<KeyLocalType, ValueLocalType>>(), invoke_context, Make<ValueField>(item),
            Emplace<DestValue>(value));
    }
}

// FIXME: Misnamed. Really just forward_as_tuple function object.
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

// Emplace function that when called with tuple of key constructor arguments
// reads value from pair and calls piecewise construct.
template <typename ValueLocalType, typename Input, typename Emplace>
struct PairValueEmplace
{
    InvokeContext& m_context;
    Input& m_input;
    Emplace& m_emplace;
    template <typename KeyTuple>

    // FIXME Should really return reference to emplaced key object.
    void operator()(KeyTuple&& key_tuple)
    {
        const auto& pair = m_input.get();
        using ValueAccessor = typename ProxyStruct<typename Decay<decltype(pair)>::Reads>::ValueAccessor;
        ReadField(TypeList<ValueLocalType>(), m_context, Make<StructField, ValueAccessor>(pair),
            MakeTupleEmplace(Make<Compose>(Get<1>(), Bind(m_emplace, std::piecewise_construct, key_tuple))));
    }
};

template <typename KeyLocalType, typename ValueLocalType, typename Input, typename Emplace>
void ReadFieldNew(TypeList<std::pair<KeyLocalType, ValueLocalType>>,
    InvokeContext& invoke_context,
    Input&& input,
    Emplace&& emplace)
{
    /* This could be simplified a lot with c++14 generic lambdas. All it is doing is:
    ReadField(TypeList<KeyLocalType>(), invoke_context, Make<ValueField>(input.get().getKey()), [&](auto&&... key_args) {
        ReadField(TypeList<ValueLocalType>(), invoke_context, Make<ValueField>(input.get().getValue()), [&](auto&&...
    value_args)
    {
            emplace(std::piecewise_construct, std::forward_as_tuple(key_args...),
    std::forward_as_tuple(value_args...));
        })
    });
    */
    const auto& pair = input.get();
    using KeyAccessor = typename ProxyStruct<typename Decay<decltype(pair)>::Reads>::KeyAccessor;
    ReadField(TypeList<KeyLocalType>(), invoke_context, Make<StructField, KeyAccessor>(pair),
        MakeTupleEmplace(PairValueEmplace<ValueLocalType, Input, Emplace>{invoke_context, input, emplace}));
}

template <typename KeyLocalType, typename ValueLocalType, typename Input, typename Tuple>
void ReadFieldUpdate(TypeList<std::tuple<KeyLocalType, ValueLocalType>>,
    InvokeContext& invoke_context,
    Input&& input,
    Tuple&& tuple)
{
    const auto& pair = input.get();
    using Struct = ProxyStruct<typename Decay<decltype(pair)>::Reads>;
    ReadFieldUpdate(TypeList<KeyLocalType>(), invoke_context, Make<StructField, typename Struct::KeyAccessor>(pair),
        std::get<0>(tuple));
    ReadFieldUpdate(TypeList<ValueLocalType>(), invoke_context,
        Make<StructField, typename Struct::ValueAccessor>(pair), std::get<1>(tuple));
}

template <typename LocalType, typename Input, typename Emplace>
void ReadFieldNew(TypeList<LocalType>,
    InvokeContext& invoke_context,
    Input&& input,
    Emplace&& emplace,
    typename std::enable_if<std::is_enum<LocalType>::value>::type* enable = 0)
{
    emplace(static_cast<LocalType>(input.get()));
}

template <typename LocalType, typename Input, typename Emplace>
void ReadFieldNew(TypeList<LocalType>,
    InvokeContext& invoke_context,
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

template <typename LocalType, typename Input, typename Emplace>
void ReadFieldNew(TypeList<LocalType>,
    InvokeContext& invoke_context,
    Input&& input,
    Emplace&& emplace,
    typename std::enable_if<std::is_floating_point<LocalType>::value>::type* enable = 0)
{
    auto value = input.get();
    static_assert(std::is_same<LocalType, decltype(value)>::value, "floating point type mismatch");
    emplace(value);
}

template <typename Input, typename Emplace>
void ReadFieldNew(TypeList<std::string>, InvokeContext& invoke_context, Input&& input, Emplace&& emplace)
{
    auto data = input.get();
    emplace(CharCast(data.begin()), data.size());
}

template <typename Input, typename Emplace>
void ReadFieldNew(TypeList<std::exception>, InvokeContext& invoke_context, Input&& input, Emplace&& emplace)
{
    auto data = input.get();
    emplace(std::string(CharCast(data.begin()), data.size()));
}

template <typename LocalType, typename Input, typename Emplace>
void ReadFieldNew(TypeList<std::unique_ptr<LocalType>>,
    InvokeContext& invoke_context,
    Input&& input,
    Emplace&& emplace,
    typename Decay<decltype(input.get())>::Calls* enable = nullptr)
{
    using Interface = typename Decay<decltype(input.get())>::Calls;
    if (input.has()) {
        emplace(MakeUnique<ProxyClient<Interface>>(std::move(input.get()), invoke_context.loop));
    }
}

// Callback class is needed because c++11 doesn't support auto lambda parameters.
// It's equivalent c++14: [invoke_context](auto&& params) {
// invoke_context->call(std::forward<decltype(params)>(params)...)
template <typename InvokeContext>
struct Callback
{
    InvokeContext m_proxy;

    template <typename... CallParams>
    auto operator()(CallParams&&... params) -> AUTO_RETURN(this->m_proxy->call(std::forward<CallParams>(params)...))
};

template <typename FnR, typename... FnParams, typename Input, typename Emplace>
void ReadFieldNew(TypeList<std::function<FnR(FnParams...)>>,
    InvokeContext& invoke_context,
    Input&& input,
    Emplace&& emplace)
{
    if (input.has()) {
        using Interface = typename Decay<decltype(input.get())>::Calls;
        auto client = std::make_shared<ProxyClient<Interface>>(input.get(), invoke_context.loop);
        emplace(Callback<decltype(client)>{std::move(client)});
    }
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

template <typename Param, typename Enable = void>
struct IsEmplace : std::false_type
{
};

template <typename Param>
struct IsEmplace<Param, typename std::enable_if<Param::emplace>::type> : public std::true_type
{
};

template <typename LocalType, typename Input, typename Value>
void ReadFieldUpdate(TypeList<LocalType>,
    InvokeContext& invoke_context,
    Input&& input,
    Value&& value,
    decltype(ReadFieldNew(TypeList<Decay<LocalType>>(),
        invoke_context,
        std::forward<Input>(input),
        std::declval<Emplace<decltype(std::ref(value))>>()))* enable = nullptr)
{
    auto ref = std::ref(value);
    ReadFieldNew(TypeList<Decay<LocalType>>(), invoke_context, input, Emplace<decltype(ref)>(ref));
}

template <size_t index, typename LocalType, typename Input, typename Value>
void ReadOne(TypeList<LocalType> param,
    InvokeContext& invoke_context,
    Input&& input,
    Value&& value,
    typename std::enable_if<index != ProxyType<LocalType>::fields>::type* enable = nullptr)
{
    using Index = std::integral_constant<size_t, index>;
    using Struct = typename ProxyType<LocalType>::Struct;
    using Accessor = typename std::tuple_element<index, typename ProxyStruct<Struct>::Accessors>::type;
    const auto& struc = input.get();
    auto&& field_value = value.*ProxyType<LocalType>::get(Index());
    ReadFieldUpdate(
        TypeList<Decay<decltype(field_value)>>(), invoke_context, Make<StructField, Accessor>(struc), field_value);
    ReadOne<index + 1>(param, invoke_context, input, value);
}

template <size_t index, typename LocalType, typename Input, typename Value>
void ReadOne(TypeList<LocalType> param,
    InvokeContext& invoke_context,
    Input& input,
    Value& value,
    typename std::enable_if<index == ProxyType<LocalType>::fields>::type* enable = nullptr)
{
}

template <typename LocalType, typename Input, typename Value>
void ReadFieldUpdate(TypeList<LocalType> param,
    InvokeContext& invoke_context,
    Input&& input,
    Value&& value,
    typename ProxyType<LocalType>::Struct* enable = nullptr)
{
    ReadOne<0>(param, invoke_context, input, value);
}

// ReadField calling Emplace when ReadFieldNew is avaiable.
template <typename LocalType, typename Input, typename Emplace>
void ReadFieldImpl(TypeList<LocalType>,
    Priority<2>,
    InvokeContext& invoke_context,
    Input&& input,
    Emplace&& emplace,
    decltype(
        ReadFieldNew(TypeList<Decay<LocalType>>(), invoke_context, input, std::forward<Emplace>(emplace)))* enable =
        nullptr)
{
    ReadFieldNew(TypeList<Decay<LocalType>>(), invoke_context, input, std::forward<Emplace>(emplace));
}

// ReadField calling Emplace when ReadFieldNew is not available, and emplace creates non-const object.
// Call emplace first to create empty value, then ReadFieldUpdate into the new object.
template <typename LocalType, typename Input, typename Emplace>
void ReadFieldImpl(TypeList<LocalType>,
    Priority<1>,
    InvokeContext& invoke_context,
    Input&& input,
    Emplace&& emplace,
    typename std::enable_if<!std::is_void<decltype(emplace())>::value &&
                            !std::is_const<typename std::remove_reference<decltype(emplace())>::type>::value>::type*
        enable = nullptr)
{
    auto&& ref = emplace();
    ReadFieldUpdate(TypeList<Decay<LocalType>>(), invoke_context, input, ref);
}

// ReadField calling Emplace when ReadFieldNew is not available, and emplace creates const object.
// Initialize temporary with ReadFieldUpdate then std::move into emplace.
template <typename LocalType, typename Input, typename Emplace>
void ReadFieldImpl(TypeList<LocalType>, Priority<0>, InvokeContext& invoke_context, Input&& input, Emplace&& emplace)
{
    Decay<LocalType> temp;
    ReadFieldUpdate(TypeList<Decay<LocalType>>(), invoke_context, input, temp);
    emplace(std::move(temp));
}

template <typename LocalTypes, typename Input, typename... Values>
void ReadField(LocalTypes, InvokeContext& invoke_context, Input&& input, Values&&... values)
{
    ReadFieldImpl(LocalTypes(), Priority<2>(), invoke_context, input, std::forward<Values>(values)...);
}

template <typename LocalType, typename Output>
void BuildField(TypeList<LocalType>, Priority<1>, InvokeContext& invoke_context, ::capnp::Void, Output&& output)
{
}

template <typename Value, typename Output>
void BuildField(TypeList<std::string>, Priority<1>, InvokeContext& invoke_context, Value&& value, Output&& output)
{
    auto result = output.init(value.size());
    memcpy(result.begin(), value.data(), value.size());
}

//! Adapter to convert ProxyCallback object call to function object call.
template <typename Result, typename... Args>
class ProxyCallbackImpl : public ProxyCallback<std::function<Result(Args...)>>
{
    using Fn = std::function<Result(Args...)>;
    Fn m_fn;

 public:
 ProxyCallbackImpl(Fn fn) : m_fn(std::move(fn)) {}
    Result call(Args&&... args) override { return m_fn(std::forward<Args>(args)...); }
};

template <typename Value, typename FnR, typename... FnParams, typename Output>
void BuildField(TypeList<std::function<FnR(FnParams...)>>,
    Priority<1>,
    InvokeContext& invoke_context,
    Value&& value,
    Output&& output)
{
    if (value) {
        using Interface = typename decltype(output.get())::Calls;
        using Callback = ProxyCallbackImpl<FnR, FnParams...>;
        output.set(kj::heap<ProxyServer<Interface>>(new Callback(std::forward<Value>(value)), true /* owned */, invoke_context.loop));
    }
}

template <typename Impl, typename Value, typename Output>
void BuildField(TypeList<std::unique_ptr<Impl>>, Priority<1>, InvokeContext& invoke_context, Value&& value, Output&& output,
                typename Decay<decltype(output.get())>::Calls* enable = nullptr)
{
    if (value) {
        using Interface = typename decltype(output.get())::Calls;
        output.set(kj::heap<ProxyServer<Interface>>(value.release(), true /* owned */, invoke_context.loop));
    }
}

template <typename LocalType, typename Value, typename Output>
void BuildField(TypeList<LocalType*>, Priority<3>, InvokeContext& invoke_context, Value&& value, Output&& output)
{
    if (value) {
        // FIXME std::move probably wrong
        BuildField(TypeList<LocalType>(), BuildFieldPriority(), invoke_context, std::move(*value), output);
    }
}

template <typename LocalType, typename Value, typename Output>
void BuildField(TypeList<std::shared_ptr<LocalType>>,
    Priority<1>,
    InvokeContext& invoke_context,
    Value&& value,
    Output&& output)
{
    if (value) {
        BuildField(TypeList<LocalType>(), BuildFieldPriority(), invoke_context, *value, output);
    }
}

template <typename LocalType, typename Value, typename Output>
void BuildField(TypeList<std::vector<LocalType>>,
    Priority<1>,
    InvokeContext& invoke_context,
    Value&& value,
    Output&& output)
{
    // FIXME dedup with set handler below
    auto list = output.init(value.size());
    size_t i = 0;
    for (auto& elem : value) {
        BuildField(TypeList<LocalType>(), BuildFieldPriority(), invoke_context, std::move(elem),
            ListOutput<typename decltype(list)::Builds>(list, i));
        ++i;
    }
}

template <typename LocalType, typename Value, typename Output>
void BuildField(TypeList<std::set<LocalType>>,
    Priority<1>,
    InvokeContext& invoke_context,
    Value&& value,
    Output&& output)
{
    // FIXME dededup with vector handler above
    auto list = output.init(value.size());
    size_t i = 0;
    for (const auto& elem : value) {
        BuildField(TypeList<LocalType>(), BuildFieldPriority(), invoke_context, elem,
            ListOutput<typename decltype(list)::Builds>(list, i));
        ++i;
    }
}

template <typename KeyLocalType, typename ValueLocalType, typename Value, typename Output>
void BuildField(TypeList<std::map<KeyLocalType, ValueLocalType>>,
    Priority<1>,
    InvokeContext& invoke_context,
    Value&& value,
    Output&& output)
{
    // FIXME dededup with vector handler above
    auto list = output.init(value.size());
    size_t i = 0;
    for (const auto& elem : value) {
        BuildField(TypeList<std::pair<KeyLocalType, ValueLocalType>>(), BuildFieldPriority(), invoke_context, elem,
            ListOutput<typename decltype(list)::Builds>(list, i));
        ++i;
    }
}
template <typename Value>
::capnp::Void BuildPrimitive(InvokeContext& invoke_context, Value&&, TypeList<::capnp::Void>)
{
    return {};
}

template <typename LocalType, typename Value>
LocalType BuildPrimitive(InvokeContext& invoke_context,
    const Value& value,
    TypeList<LocalType>,
    typename std::enable_if<std::is_enum<Value>::value>::type* enable = nullptr)
{
    return static_cast<LocalType>(value);
}

template <typename LocalType, typename Value>
LocalType BuildPrimitive(InvokeContext& invoke_context,
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
LocalType BuildPrimitive(InvokeContext& invoke_context,
    const Value& value,
    TypeList<LocalType>,
    typename std::enable_if<std::is_floating_point<Value>::value>::type* enable = nullptr)
{
    static_assert(std::is_same<Value, LocalType>::value,
        "mismatched floating point types. please fix message.capnp type declaration to match wrapped interface");
    return value;
}

template <typename LocalType, typename Output>
void BuildField(TypeList<LocalType&>,
    Priority<1>,
    InvokeContext& invoke_context,
    LocalType&& value,
    Output&& output,
    typename decltype(output.get())::Calls* enable = nullptr)
{
    output.set(
        kj::heap<ProxyServer<typename decltype(output.get())::Calls>>(&value, false /* owned */, invoke_context.loop));
}

template <typename LocalType, typename Value, typename Output>
void BuildField(TypeList<boost::optional<LocalType>>,
    Priority<1>,
    InvokeContext& invoke_context,
    Value&& value,
    Output&& output)
{
    if (value) {
        output.setHas();
        // FIXME: should std::move value if destvalue is rref?
        BuildField(TypeList<LocalType>(), BuildFieldPriority(), invoke_context, *value, output);
    }
}

template <typename Output>
void BuildField(TypeList<std::exception>,
    Priority<1>,
    InvokeContext& invoke_context,
    const std::exception& value,
    Output&& output)
{
    BuildField(TypeList<std::string>(), BuildFieldPriority(), invoke_context, std::string(value.what()), output);
}

// FIXME: Overload on output type instead of value type and switch to std::get and merge with next overload
template <typename KeyLocalType, typename ValueLocalType, typename Value, typename Output>
void BuildField(TypeList<std::pair<KeyLocalType, ValueLocalType>>,
    Priority<1>,
    InvokeContext& invoke_context,
    Value&& value,
    Output&& output)
{
    auto pair = output.init();
    using KeyAccessor = typename ProxyStruct<typename decltype(pair)::Builds>::KeyAccessor;
    using ValueAccessor = typename ProxyStruct<typename decltype(pair)::Builds>::ValueAccessor;
    BuildField(TypeList<KeyLocalType>(), BuildFieldPriority(), invoke_context, value.first,
        Make<StructField, KeyAccessor>(pair));
    BuildField(TypeList<ValueLocalType>(), BuildFieldPriority(), invoke_context, value.second,
        Make<StructField, ValueAccessor>(pair));
}

template <typename KeyLocalType, typename ValueLocalType, typename Value, typename Output>
void BuildField(TypeList<std::tuple<KeyLocalType, ValueLocalType>>,
    Priority<1>,
    InvokeContext& invoke_context,
    Value&& value,
    Output&& output)
{
    auto pair = output.init();
    using KeyAccessor = typename ProxyStruct<typename decltype(pair)::Builds>::KeyAccessor;
    using ValueAccessor = typename ProxyStruct<typename decltype(pair)::Builds>::ValueAccessor;
    BuildField(TypeList<KeyLocalType>(), BuildFieldPriority(), invoke_context, std::get<0>(value),
        Make<StructField, KeyAccessor>(pair));
    BuildField(TypeList<ValueLocalType>(), BuildFieldPriority(), invoke_context, std::get<1>(value),
        Make<StructField, ValueAccessor>(pair));
}

template <typename LocalType, typename Value, typename Output>
void BuildField(TypeList<const LocalType>, Priority<0>, InvokeContext& invoke_context, Value&& value, Output&& output)
{
    BuildField(TypeList<LocalType>(), BuildFieldPriority(), invoke_context, std::forward<Value>(value), output);
}

template <typename LocalType, typename Value, typename Output>
void BuildField(TypeList<LocalType&>,
    Priority<0>,
    InvokeContext& invoke_context,
    Value&& value,
    Output&& output,
    void* enable = nullptr)
{
    BuildField(TypeList<LocalType>(), BuildFieldPriority(), invoke_context, std::forward<Value>(value), output);
}

template <typename LocalType, typename Value, typename Output>
void BuildField(TypeList<LocalType&&>, Priority<0>, InvokeContext& invoke_context, Value&& value, Output&& output)
{
    BuildField(TypeList<LocalType>(), BuildFieldPriority(), invoke_context, std::forward<Value>(value), output);
}

template <typename LocalType, typename Value, typename Output>
void BuildField(TypeList<LocalType>, Priority<0>, InvokeContext& invoke_context, Value&& value, Output&& output)
{
    output.set(BuildPrimitive(invoke_context, std::forward<Value>(value), TypeList<decltype(output.get())>()));
}

template <size_t index, typename LocalType, typename Value, typename Output>
void BuildOne(TypeList<LocalType> param,
    InvokeContext& invoke_context,
    Value&& value,
    Output&& output,
    typename std::enable_if < index<ProxyType<LocalType>::fields>::type * enable = nullptr)
{
    using Index = std::integral_constant<size_t, index>;
    using Struct = typename ProxyType<LocalType>::Struct;
    using Accessor = typename std::tuple_element<index, typename ProxyStruct<Struct>::Accessors>::type;
    auto&& field_output = Make<StructField, Accessor>(output);
    auto&& field_value = value.*ProxyType<LocalType>::get(Index());
    BuildField(TypeList<decltype(field_value)>(), BuildFieldPriority(), invoke_context, field_value, field_output);
    BuildOne<index + 1>(param, invoke_context, value, output);
}

template <size_t index, typename LocalType, typename Value, typename Output>
void BuildOne(TypeList<LocalType> param,
    InvokeContext& invoke_context,
    Value& value,
    Output& output,
    typename std::enable_if<index == ProxyType<LocalType>::fields>::type* enable = nullptr)
{
}

template <typename LocalType, typename Value, typename Output>
void BuildField(TypeList<LocalType> local_type,
    Priority<1>,
    InvokeContext& invoke_context,
    Value&& value,
    Output&& output,
    typename ProxyType<LocalType>::Struct* enable = nullptr)
{
    BuildOne<0>(local_type, invoke_context, value, output.init());
}

template <typename Accessor, typename LocalType, typename ServerContext, typename Fn, typename... Args>
void PassField(TypeList<LocalType*>, ServerContext& server_context, const Fn& fn, Args&&... args)
{
    InvokeContext& invoke_context = server_context;
    boost::optional<Decay<LocalType>> param;
    const auto& params = server_context.call_context.getParams();
    const auto& input = Make<StructField, Accessor>(params);
    bool want = input.want();
    if (want) {
        MaybeReadField(std::integral_constant<bool, Accessor::in>(), TypeList<LocalType>(), invoke_context, input,
            Emplace<decltype(param)>(param));
        if (!param) param.emplace();
    }
    fn.invoke(server_context, std::forward<Args>(args)..., param ? &*param : nullptr);
    auto&& results = server_context.call_context.getResults();
    if (want) {
        MaybeBuildField(std::integral_constant<bool, Accessor::out>(), TypeList<LocalType>(), BuildFieldPriority(),
            invoke_context, *param, Make<StructField, Accessor>(results));
    }
}

template <typename Accessor, typename LocalType, typename ServerContext, typename Fn, typename... Args>
auto PassField(TypeList<LocalType&>, ServerContext& server_context, Fn&& fn, Args&&... args)
    -> Require<typename decltype(Accessor::get(server_context.call_context.getParams()))::Calls>
{
    // Just drop argument if it is a reference to an interface client, because
    // it would be unclear when the the client should be released. Server will
    // need to provide a custom invokeMethod overload in order to access the
    // client, and can arrange for it to be disposed at the appropriate time.
    fn.invoke(server_context, std::forward<Args>(args)...);
}

template <typename... Args>
void MaybeBuildField(std::true_type, Args&&... args)
{
    BuildField(std::forward<Args>(args)...);
}
template <typename... Args>
void MaybeBuildField(std::false_type, Args&&...)
{
}
template <typename... Args>
void MaybeReadField(std::true_type, Args&&... args)
{
    ReadField(std::forward<Args>(args)...);
}
template <typename... Args>
void MaybeReadField(std::false_type, Args&&...)
{
}
template <typename... Args>
void MaybeReadFieldUpdate(std::true_type, Args&&... args)
{
    ReadFieldUpdate(std::forward<Args>(args)...);
}
template <typename... Args>
void MaybeReadFieldUpdate(std::false_type, Args&&...)
{
}

template <typename LocalType, typename Value, typename Output>
void MaybeSetWant(TypeList<LocalType*>, Priority<1>, Value&& value, Output&& output)
{
    if (value) {
        output.setWant();
    }
}

template <typename LocalTypes, typename... Args>
void MaybeSetWant(LocalTypes, Priority<0>, Args&&...)
{
}

template <typename Accessor, typename LocalType, typename ServerContext, typename Fn, typename... Args>
void DefaultPassField(TypeList<LocalType>, ServerContext& server_context, Fn&& fn, Args&&... args)
{
    InvokeContext& invoke_context = server_context;
    boost::optional<Decay<LocalType>> param;
    const auto& params = server_context.call_context.getParams();
    MaybeReadField(std::integral_constant<bool, Accessor::in>(), TypeList<LocalType>(), invoke_context,
        Make<StructField, Accessor>(params), Emplace<decltype(param)>(param));
    if (!param) param.emplace();
    fn.invoke(server_context, std::forward<Args>(args)..., static_cast<LocalType&&>(*param));
    auto&& results = server_context.call_context.getResults();
    MaybeBuildField(std::integral_constant<bool, Accessor::out>(), TypeList<LocalType>(), BuildFieldPriority(),
        invoke_context, *param, Make<StructField, Accessor>(results));
}

template <typename Output>
void BuildField(TypeList<>,
    Priority<1>,
    InvokeContext& invoke_context,
    Output&& output,
    typename std::enable_if<std::is_same<decltype(output.get()), ThreadMap::Client>::value>::type* enable = nullptr)
{
    output.set(kj::heap<ProxyServer<ThreadMap>>(invoke_context.loop));
}

template <typename Input>
void ReadFieldUpdate(TypeList<>,
    InvokeContext& invoke_context,
    Input&& input,
    typename std::enable_if<std::is_same<decltype(input.get()), ThreadMap::Client>::value>::type* enable = nullptr)
{
    invoke_context.loop.m_thread_map = input.get();
}

template <typename Accessor, typename ServerContext, typename Fn, typename... Args>
auto PassField(TypeList<>, ServerContext& server_context, const Fn& fn, Args&&... args) -> typename std::enable_if<
    std::is_same<decltype(Accessor::get(server_context.call_context.getParams())), ThreadMap::Client>::value>::type
{
    const auto& params = server_context.call_context.getParams();
    const auto& input = Make<StructField, Accessor>(params);
    ReadFieldUpdate(TypeList<>(), server_context, input);
    fn.invoke(server_context, std::forward<Args>(args)...);
    auto&& results = server_context.call_context.getResults();
    BuildField(TypeList<>(), BuildFieldPriority(), server_context, Make<StructField, Accessor>(results));
}

template <typename Derived, size_t N = 0>
struct IterateFieldsHelper
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

struct IterateFields : IterateFieldsHelper<IterateFields, 0>
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
    struct BuildParams : IterateFieldsHelper<BuildParams, 0>
    {
        template <typename Params, typename ParamList>
        void handleField(InvokeContext& invoke_context, Params& params, ParamList)
        {
        }

        BuildParams(ClientException* client_exception) : m_client_exception(client_exception) {}
        ClientException* m_client_exception;
    };

    struct ReadResults : IterateFieldsHelper<ReadResults, 0>
    {
        template <typename Results, typename ParamList>
        void handleField(InvokeContext& invoke_context, Results& results, ParamList)
        {
            StructField<Accessor, Results> input(results);
            if (input.has()) {
                ReadField(TypeList<Exception>(), invoke_context, input, ThrowEmplace<Exception>());
            }
        }

        ReadResults(ClientException* client_exception) : m_client_exception(client_exception) {}
        ClientException* m_client_exception;
    };
};

template <typename Accessor, typename... Types>
struct ClientParam
{
    ClientParam(Types&... values) : m_values(values...) {}

    struct BuildParams : IterateFieldsHelper<BuildParams, sizeof...(Types)>
    {
        template <typename... Args>
        void handleField(Args&&... args)
        {
            callBuild<0>(std::forward<Args>(args)...);
        }

        template <size_t I, typename... Args>
        auto callBuild(Args&&... args) -> typename std::enable_if<(I < sizeof...(Types))>::type
        {
            // FIXME: replace std::move below with std::forward
            callBuild<I + 1>(std::forward<Args>(args)..., std::move(std::get<I>(m_client_param->m_values)));
        }

        template <size_t I, typename Params, typename ParamList, typename... Values>
        auto callBuild(ClientInvokeContext& invoke_context, Params& params, ParamList, Values&&... values) ->
            typename std::enable_if<(I == sizeof...(Types))>::type
        {
            MaybeBuildField(std::integral_constant<bool, Accessor::in>(), ParamList(), BuildFieldPriority(),
                invoke_context, std::forward<Values>(values)..., Make<StructField, Accessor>(params));
            MaybeSetWant(
                ParamList(), Priority<1>(), std::forward<Values>(values)..., Make<StructField, Accessor>(params));
        }

        BuildParams(ClientParam* client_param) : m_client_param(client_param) {}
        ClientParam* m_client_param;
    };

    struct ReadResults : IterateFieldsHelper<ReadResults, sizeof...(Types)>
    {
        template <typename... Args>
        void handleField(Args&&... args)
        {
            callRead<0>(std::forward<Args>(args)...);
        }

        template <int I, typename... Args>
        auto callRead(Args&&... args) -> typename std::enable_if<(I < sizeof...(Types))>::type
        {
            callRead<I + 1>(std::forward<Args>(args)..., std::get<I>(m_client_param->m_values));
        }

        template <int I, typename Results, typename... Params, typename... Values>
        auto callRead(ClientInvokeContext& invoke_context, Results& results, TypeList<Params...>, Values&&... values)
            -> typename std::enable_if<I == sizeof...(Types)>::type
        {
            MaybeReadFieldUpdate(std::integral_constant<bool, Accessor::out>(), TypeList<Decay<Params>...>(),
                invoke_context, Make<StructField, Accessor>(results), std::forward<Values>(values)...);
        }

        ReadResults(ClientParam* client_param) : m_client_param(client_param) {}
        ClientParam* m_client_param;
    };

    // FIXME: should be rvalue reference to fix callBuild above
    std::tuple<Types&...> m_values;
};

template <typename Accessor, typename... Types>
ClientParam<Accessor, Types...> MakeClientParam(Types&... values)
{
    return {values...};
}

//! Safely convert char pointer to kj pointer.
static inline kj::byte* ByteCast(char* c) { return reinterpret_cast<kj::byte*>(c); }
static inline const kj::byte* ByteCast(const char* c) { return reinterpret_cast<const kj::byte*>(c); }

#if 0
template<typename T, typename... A>
void shit(A&&...a) {
T::invoke(std::forward<A>(a)...);
}
#endif

struct ServerCall
{
    // FIXME: maybe call call_context.releaseParams()
    template <typename ServerContext, typename... Args>
    auto invoke(ServerContext& server_context, TypeList<>, Args&&... args) const -> AUTO_RETURN(
        ProxyServerMethodTraits<typename decltype(server_context.call_context.getParams())::Reads>::invoke(
            server_context,
            std::forward<Args>(args)...))
};

struct ServerDestroy
{
    template <typename ServerContext, typename... Args>
    void invoke(ServerContext& server_context, TypeList<>, Args&&... args) const
    {
        server_context.proxy_server.invokeDestroy(true /* remote */, std::forward<Args>(args)...);
    }
};

template <typename Accessor, typename Parent>
struct ServerRet : Parent
{
    ServerRet(Parent parent) : Parent(parent) {}

    template <typename ServerContext, typename... Args>
    void invoke(ServerContext& server_context, TypeList<>, Args&&... args) const
    {
        auto&& result = Parent::invoke(server_context, TypeList<>(), std::forward<Args>(args)...);
        auto&& results = server_context.call_context.getResults();
        InvokeContext& invoke_context = server_context;
        BuildField(TypeList<decltype(result)>(), BuildFieldPriority(), invoke_context, std::move(result),
            Make<StructField, Accessor>(results));
    }
};

template <typename Exception, typename Accessor, typename Parent>
struct ServerExcept : Parent
{
    ServerExcept(Parent parent) : Parent(parent) {}

    template <typename ServerContext, typename... Args>
    void invoke(ServerContext& server_context, TypeList<>, Args&&... args) const
    {
        try {
            return Parent::invoke(server_context, TypeList<>(), std::forward<Args>(args)...);
        } catch (const Exception& e) {
            auto&& results = server_context.call_context.getResults();
            BuildField(
                TypeList<Exception>(), BuildFieldPriority(), server_context, e, Make<StructField, Accessor>(results));
        }
    }
};

template <typename Accessor, typename... Args>
auto CallPassField(Priority<1>, Args&&... args)
    -> AUTO_RETURN(PassField<Accessor>(std::forward<Args>(args)...)) template <typename Accessor, typename... Args>
    auto CallPassField(Priority<0>, Args&&... args)
        -> AUTO_RETURN(DefaultPassField<Accessor>(std::forward<Args>(args)...))

            template <int argc, typename Accessor, typename Parent>
            struct ServerField : Parent
{
    ServerField(Parent parent) : Parent(parent) {}

    const Parent& parent() const { return *this; }

    template <typename ServerContext, typename ArgTypes, typename... Args>
    auto invoke(ServerContext& server_context, ArgTypes, Args&&... args) const
        -> AUTO_RETURN(CallPassField<Accessor>(Priority<1>(),
            typename Split<argc, ArgTypes>::First(),
            server_context,
            this->parent(),
            typename Split<argc, ArgTypes>::Second(),
            std::forward<Args>(args)...))
};

template <int argc, typename Accessor, typename Parent>
ServerField<argc, Accessor, Parent> MakeServerField(Parent parent)
{
    return {parent};
}

template <typename Request>
struct CapRequestTraits;

template <typename _Params, typename _Results>
struct CapRequestTraits<::capnp::Request<_Params, _Results>>
{
    using Params = _Params;
    using Results = _Results;
};

template <typename Client>
void clientDestroy(Client& client)
{
    LogIpc(*client.m_loop, "IPC client destroy %s\n", typeid(client).name());
}

template <typename Server>
void serverDestroy(Server& server)
{
    LogIpc(*server.m_loop, "IPC server destroy %s\n", typeid(server).name());
}

template <typename ProxyClient, typename GetRequest, typename... FieldObjs>
void clientInvoke(ProxyClient& proxy_client, const GetRequest& get_request, FieldObjs&&... fields)
{
    if (!proxy_client.m_loop) {
        LogPrintf("IPC error: clientInvoke call made after event loop shutdown");
        throw std::logic_error("clientInvoke call made after event loop shutdown");
    }
    if (!g_thread_context.waiter) {
        g_thread_context.waiter = MakeUnique<Waiter>(*proxy_client.m_loop);
        g_thread_context.waiter->m_name = ThreadName(proxy_client.m_loop->m_exe_name);
        LogPrint(::BCLog::IPC, "{%s} IPC client first request from current thread, constructing waiter\n",
            g_thread_context.waiter->m_name);
    }
    ClientInvokeContext invoke_context{*proxy_client.m_loop, g_thread_context};
    std::exception_ptr exception;
    bool done = false;
    proxy_client.m_loop->sync([&]() {
        auto request = (proxy_client.m_client.*get_request)(nullptr);
        using Request = CapRequestTraits<decltype(request)>;
        using FieldList = typename ProxyClientMethodTraits<typename Request::Params>::Fields;
        IterateFields().handleChain(invoke_context, request, FieldList(), typename FieldObjs::BuildParams{&fields}...);
        LogPrint(::BCLog::IPC, "{%s} IPC client send %s %s\n", invoke_context.client_thread.waiter->m_name,
            TypeName<typename Request::Params>(), request.toString().flatten().cStr());

        proxy_client.m_loop->m_task_set.add(
            request.send().then([&](::capnp::Response<typename Request::Results>&& response) {
                LogPrint(::BCLog::IPC, "{%s} IPC client recv %s %s\n", invoke_context.client_thread.waiter->m_name,
                    TypeName<typename Request::Results>(), response.toString().flatten().cStr());

                try {
                    IterateFields().handleChain(
                        invoke_context, response, FieldList(), typename FieldObjs::ReadResults{&fields}...);
                } catch (...) {
                    exception = std::current_exception();
                }
                std::unique_lock<std::mutex> lock(invoke_context.client_thread.waiter->m_mutex);
                done = true;
                invoke_context.client_thread.waiter->m_cv.notify_all();
            }));
    });

    std::unique_lock<std::mutex> lock(invoke_context.client_thread.waiter->m_mutex);
    invoke_context.client_thread.waiter->wait(lock, [&done]() { return done; });
    if (exception) std::rethrow_exception(exception);
}

extern std::atomic<int> server_reqs;

template <typename Server, typename CallContext, typename Fn>
kj::Promise<void> serverInvoke(Server& server, CallContext& call_context, Fn fn)
{
    auto params = call_context.getParams();
    using Params = decltype(params);
    using Results = typename decltype(call_context.getResults())::Builds;

    int req = ++server_reqs;
    LogIpc(*server.m_loop, "IPC server recv request #%i %s %s\n", req, TypeName<typename Params::Reads>(),
        params.toString().flatten().cStr());

    try {
        using ServerContext = ServerInvokeContext<Server, CallContext>;
        using ArgList = typename ProxyClientMethodTraits<typename Params::Reads>::Params;
        ServerContext server_context{server, call_context, req};
        return ReplaceVoid([&]() { return fn.invoke(server_context, ArgList()); },
            [&]() { return kj::Promise<CallContext>(kj::mv(call_context)); })
            .then([&server, req](CallContext call_context) {
                LogIpc(*server.m_loop, "IPC server send request #%i %s %s\n", req, TypeName<Results>(),
                    call_context.getResults().toString().flatten().cStr());
            });
    } catch (...) {
        LogIpc(
            *server.m_loop, "IPC server unhandled exception %s\n", boost::current_exception_diagnostic_information());
        throw;
    }
}

} // namespace capnp
} // namespace interfaces

#endif // BITCOIN_INTERFACES_CAPNP_PROXY_IMPL_H
