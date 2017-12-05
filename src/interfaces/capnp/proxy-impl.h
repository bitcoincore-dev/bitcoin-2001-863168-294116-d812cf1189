#ifndef BITCOIN_INTERFACES_CAPNP_PROXY_H
#define BITCOIN_INTERFACES_CAPNP_PROXY_H

#include <interfaces/capnp/proxy.h>

#include <interfaces/capnp/proxy.capnp.h>
#include <interfaces/capnp/util.h>
#include <optional.h>
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
    ::interfaces::capnp::Init::Client* m_init = nullptr;
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
        LogIpc(m_loop, "Waiter::wait -- enter\n");
        m_cv.wait(lock, [&] {
            LogIpc(m_loop, "Waiter::wait -- wake\n");
            // Important for this to be while loop, not if statement to avoid a
            // lost-wakeup bug. A new m_fn and m_cv notification might be set
            // between the unlock&lock lines in this loop. In the case where a
            // capnp response is sent and a brand new request is received here
            // before this relocks.
            while (m_fn) {
                LogIpc(m_loop, "Waiter::wait -- calling fn\n");
                auto fn = std::move(m_fn);
                assert(!m_fn);
                lock.unlock();
                fn();
                LogIpc(m_loop, "Waiter::wait -- called fn fn\n");
                lock.lock();
            }
            bool done = pred();
            LogIpc(m_loop, "Waiter::wait -- %s\n", done ? "done" : "sleeping");
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
class ProxyServer<Thread> : public Thread::Server
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
    Optional<Thread::Client> local_thread;
    std::map<EventLoop*, Optional<Thread::Client>> remote_threads;

    ~ThreadContext()
    {
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
    typename std::enable_if<std::is_same<typename Decay<Output>::CapType, Context>::value>::type* enable = nullptr)
{
    if (!invoke_context.client_thread.local_thread) {
        invoke_context.client_thread.local_thread =
            invoke_context.loop.m_threads.add(kj::heap<ProxyServer<Thread>>(*invoke_context.client_thread.waiter));
    }

    Optional<Thread::Client>& remote_thread = invoke_context.client_thread.remote_threads[&invoke_context.loop];
    if (!remote_thread) {
        // This will only trigger if IPC client call is being made for the first time on a new thread. After this is
        // initialized subsequent calls will use the previous value. And for the first IPC client call being made from
        // a remote thread, this code ever won't run because Passfield below will set remote_thread to the
        // callbackThread from the remote call context.
        auto request = invoke_context.loop.m_init->makeThreadRequest();
        request.setName(invoke_context.client_thread.waiter->m_name);
        remote_thread = request.send().getResult(); // Should be nonblocking due to piplineing.
    }

    auto context = output.set();
    context.setThread(*remote_thread);
    context.setCallbackThread(*invoke_context.client_thread.local_thread);
}

template <typename ServerContext, typename Accessor, typename Fn, typename... Args>
typename kj::Promise<typename ServerContext::MethodContext> PassField(TypeList<Context>,
    TypeList<>,
    ServerContext& server_context,
    const Accessor& accessor,
    const Fn& fn,
    const Args&... args)
{
    Context::Reader context_arg = Make<FieldInput>(server_context.method_context.getParams(), accessor).get();
    auto future = kj::newPromiseAndFulfiller<typename ServerContext::MethodContext>();
    auto& server = server_context.proxy_server;
    int req = server_context.req;
    auto invoke = MakeAsyncCallable(kj::mvCapture(future.fulfiller,
        kj::mvCapture(server_context.method_context,
            [&server, req, accessor, fn, args...](typename ServerContext::MethodContext method_context,
                kj::Own<kj::PromiseFulfiller<typename ServerContext::MethodContext>> fulfiller) {
                Context::Reader context_arg = Make<FieldInput>(method_context.getParams(), accessor).get();
                ServerContext server_context{server, method_context, req};
                LogIpc(*server.m_loop, "PassField invoke -- calling fn\n");
                {
                    Optional<Thread::Client>& thread_client = g_thread_context.remote_threads[server.m_loop];
                    TempSetter<Optional<Thread::Client>> temp_setter(thread_client, context_arg.getCallbackThread());
                    fn.invoke(server_context, args...);
                }
                LogIpc(*server.m_loop, "PassField invoke -- called fn\n");
                KJ_IF_MAYBE(exception, kj::runCatchingExceptions([&]() {
                    server.m_loop->sync([&] {
                        LogIpc(*server.m_loop, "PassField invoke -- fullfilling promise\n");
                        auto fulfiller_dispose = kj::mv(fulfiller);
                        fulfiller_dispose->fulfill(kj::mv(method_context));
                        LogIpc(*server.m_loop, "PassField invoke -- done fullfilling promise\n");
                    });
                }))
                {
                    server.m_loop->sync([&]() {
                        LogIpc(*server.m_loop, "PassField invoke -- rejecting promise\n");
                        auto fulfiller_dispose = kj::mv(fulfiller);
                        fulfiller_dispose->reject(kj::mv(*exception));
                    });
                    LogIpc(*server.m_loop, "PassField invoke -- done promise\n");
                }
                LogIpc(*server.m_loop, "PassField invoke -- done, returning\n");
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
                    LogPrintf("IPC error: invalid thread handle");
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
    LogIpc(*m_loop, "client base destroy loop %p %s\n", m_loop, typeid(*this).name());
    if (m_loop) {
        m_loop->sync([&]() {
            LogIpc(*m_loop, "client base destroy synced %s\n", typeid(*this).name());
            m_loop->removeCleanup(m_cleanup);
            cleanup(false /* remote */);
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
        ReadField(TypeList<LocalType>(), invoke_context, input, *value);
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
        ReadField(TypeList<LocalType>(), invoke_context, input, *value);
    } else {
        ReadField(TypeList<LocalType>(), invoke_context, input, Emplace<DestValue>(value));
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
    ReadField(TypeList<LocalType>(), invoke_context, input, Emplace<DestValue>(value));
}

template <typename LocalType, typename Input, typename DestValue>
void ReadFieldUpdate(TypeList<std::vector<LocalType>>, InvokeContext& invoke_context, Input&& input, DestValue&& value)
{
    auto data = input.get();
    value.clear();
    value.reserve(data.size());
    for (auto&& item : data) {
        ReadField(TypeList<LocalType>(), invoke_context, MakeValueInput(item), Emplace<DestValue>(value));
    }
}

template <typename LocalType, typename Input, typename DestValue>
void ReadFieldUpdate(TypeList<std::set<LocalType>>, InvokeContext& invoke_context, Input&& input, DestValue&& value)
{
    auto data = input.get();
    value.clear();
    for (auto&& item : data) {
        ReadField(TypeList<LocalType>(), invoke_context, MakeValueInput(item), Emplace<DestValue>(value));
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
    for (auto&& item : data) {
        ReadField(TypeList<std::pair<KeyLocalType, ValueLocalType>>(), invoke_context, MakeValueInput(item),
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

template <typename ValueLocalType, typename Input, typename Emplace>
struct PairValueEmplace
{
    InvokeContext& m_context;
    Input& m_input;
    Emplace& m_emplace;
    template <typename KeyTuple>
    void operator()(KeyTuple&& key_tuple)
    {
        auto value_accessor = ProxyStruct<typename Decay<Input>::CapType>::get(std::integral_constant<size_t, 1>());
        FieldInput<Decay<decltype(m_input.get())>, decltype(value_accessor)> value_input{
            m_input.get(), value_accessor};
        ReadField(TypeList<ValueLocalType>(), m_context, value_input,
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
    ReadField(TypeList<KeyLocalType>(), invoke_context, MakeValueInput(input.get().getKey()), [&](auto&&... key_args) {
        ReadField(TypeList<ValueLocalType>(), invoke_context, MakeValueInput(input.get().getValue()), [&](auto&&...
    value_args)
    {
            emplace(std::piecewise_construct, std::forward_as_tuple(key_args...),
    std::forward_as_tuple(value_args...));
        })
    });
    */
    auto key_accessor = ProxyStruct<typename Decay<Input>::CapType>::get(std::integral_constant<size_t, 0>());
    FieldInput<Decay<decltype(input.get())>, decltype(key_accessor)> key_input{input.get(), key_accessor};
    ReadField(TypeList<KeyLocalType>(), invoke_context, key_input,
        MakeTupleEmplace(PairValueEmplace<ValueLocalType, Input, Emplace>{invoke_context, input, emplace}));
}

template <typename KeyLocalType, typename ValueLocalType, typename Input, typename Tuple>
void ReadFieldUpdate(TypeList<std::tuple<KeyLocalType, ValueLocalType>>,
    InvokeContext& invoke_context,
    Input&& input,
    Tuple&& tuple)
{
    auto key_accessor = ProxyStruct<typename Decay<Input>::CapType>::get(std::integral_constant<size_t, 0>());
    FieldInput<Decay<decltype(input.get())>, decltype(key_accessor)> key_input{input.get(), key_accessor};

    auto value_accessor = ProxyStruct<typename Decay<Input>::CapType>::get(std::integral_constant<size_t, 1>());
    FieldInput<Decay<decltype(input.get())>, decltype(value_accessor)> value_input{input.get(), value_accessor};

    ReadField(TypeList<KeyLocalType>(), invoke_context, key_input, std::get<0>(tuple));
    ReadField(TypeList<ValueLocalType>(), invoke_context, value_input, std::get<1>(tuple));
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
    emplace(MakeUnique<ProxyClient<Interface>>(std::move(input.get()), invoke_context.loop));
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
    using Interface = typename Decay<decltype(input.get())>::Calls;
    auto client = std::make_shared<ProxyClient<Interface>>(input.get(), invoke_context.loop);
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

template <typename... LocalTypes, typename Input, typename... Values>
auto ReadFieldImpl(TypeList<LocalTypes...>,
    Priority<5>,
    InvokeContext& invoke_context,
    Input&& input,
    Values&&... values) -> typename std::enable_if<!Decay<Input>::can_get>::type
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

template <typename LocalType, typename Input, typename Value>
void ReadFieldUpdate(TypeList<LocalType>,
    InvokeContext& invoke_context,
    Input&& input,
    Value&& value,
    decltype(ReadFieldNew(TypeList<LocalType>(),
        invoke_context,
        input,
        std::declval<Emplace<decltype(std::ref(value))>>()))* enable = nullptr)
{
    auto ref = std::ref(value);
    ReadFieldNew(TypeList<LocalType>(), invoke_context, input, Emplace<decltype(ref)>(ref));
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
    auto accessor = ProxyStruct<Struct>::get(Index());
    FieldInput<typename Struct::Reader, decltype(accessor)> field_reader{input.get(), accessor};
    auto&& field_value = value.*ProxyType<LocalType>::get(Index());
    ReadField(TypeList<decltype(field_value)>(), invoke_context, field_reader, field_value);
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

template <typename LocalType, typename Input, typename Value>
void ReadFieldImpl(TypeList<LocalType>,
    Priority<4>,
    InvokeContext& invoke_context,
    Input&& input,
    Value&& value,
    typename std::enable_if<!IsEmplace<Value>::value>::type* enable = nullptr)
{
    ReadFieldUpdate(TypeList<Decay<LocalType>>(), invoke_context, input, std::forward<Value>(value));
}

template <typename LocalType, typename Input, typename Emplace>
void ReadFieldImpl(TypeList<LocalType>,
    Priority<3>,
    InvokeContext& invoke_context,
    Input&& input,
    Emplace&& emplace,
    decltype(
        ReadFieldNew(TypeList<Decay<LocalType>>(), invoke_context, input, std::forward<Emplace>(emplace)))* enable =
        nullptr)
{
    ReadFieldNew(TypeList<Decay<LocalType>>(), invoke_context, input, std::forward<Emplace>(emplace));
}

template <typename LocalType, typename Input, typename Emplace>
void ReadFieldImpl(TypeList<LocalType>,
    Priority<2>,
    InvokeContext& invoke_context,
    Input&& input,
    Emplace&& emplace,
    typename std::enable_if<!std::is_const<decltype(emplace())>::value,
        decltype(ReadFieldUpdate(TypeList<Decay<LocalType>>(), invoke_context, input, emplace()))>::type* enable =
        nullptr)
{
    auto&& ref = emplace();
    ReadFieldUpdate(TypeList<Decay<LocalType>>(), invoke_context, input, ref);
}

template <typename LocalType, typename Input, typename Emplace>
void ReadFieldImpl(TypeList<LocalType>,
    Priority<1>,
    InvokeContext& invoke_context,
    Input&& input,
    Emplace&& emplace,
    decltype(ReadFieldUpdate(TypeList<Decay<LocalType>>(), invoke_context, input, std::declval<Decay<LocalType>&>()))*
        enable = nullptr)
{
    Decay<LocalType> temp;
    ReadFieldUpdate(TypeList<Decay<LocalType>>(), invoke_context, input, temp);
    emplace(std::move(temp));
}

template <typename LocalTypes, typename Input, typename... Values>
void ReadField(LocalTypes, InvokeContext& invoke_context, Input&& input, Values&&... values)
{
    ReadFieldImpl(LocalTypes(), Priority<5>(), invoke_context, input, std::forward<Values>(values)...);
}

template <typename LocalTypes, typename Input, typename... Values>
void ClientReadField(LocalTypes, InvokeContext& invoke_context, Input&& input, Values&&... values)
{
    ReadField(LocalTypes(), invoke_context, input, std::forward<Values>(values)...);
}

template <typename LocalType, typename Input, typename Value>
void ClientReadField(TypeList<LocalType*>, InvokeContext& invoke_context, Input&& input, Value* value)
{
    if (value) {
        ReadField(TypeList<LocalType>(), invoke_context, input, *value);
    }
}

template <typename... LocalTypes, typename Value, typename Output>
void BuildField(TypeList<LocalTypes...>,
    Priority<2>,
    InvokeContext& invoke_context,
    Value&& value,
    Output&& output,
    typename std::enable_if<!Decay<Output>::can_set>::type* enable = nullptr)
{
}

template <typename LocalType, typename Output>
void BuildField(TypeList<LocalType>, Priority<1>, InvokeContext& invoke_context, ::capnp::Void, Output&& output)
{
}

template <typename Value, typename Output>
void BuildField(TypeList<std::string>, Priority<1>, InvokeContext& invoke_context, Value&& value, Output&& output)
{
    auto result = output.set(value.size());
    memcpy(result.begin(), value.data(), value.size());
}

template <typename Value, typename FnR, typename... FnParams, typename Output>
void BuildField(TypeList<std::function<FnR(FnParams...)>>,
    Priority<1>,
    InvokeContext& invoke_context,
    Value&& value,
    Output&& output)
{
    using Interface = typename Decay<Output>::CapType;
    using Callback = ProxyCallbackImpl<FnR, FnParams...>;
    // fixme: probably std::forward instead of move
    output.set(
        kj::heap<ProxyServer<Interface>>(new Callback(std::move(value)), true /* owned */, invoke_context.loop));
}

template <typename LocalType, typename Value, typename Output>
void BuildField(TypeList<LocalType*>, Priority<3>, InvokeContext& invoke_context, Value&& value, Output&& output)
{
    if (value) {
        output.setWant();
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
    auto list = output.set(value.size());
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
    auto list = output.set(value.size());
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
    auto list = output.set(value.size());
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

template <typename Impl, typename Interface>
typename Interface::Client BuildPrimitive(InvokeContext& invoke_context,
    std::unique_ptr<Impl>&& impl,
    TypeList<Interface>)
{
    return kj::heap<ProxyServer<Interface>>(impl.release(), true /* owned */, invoke_context.loop);
}

template <typename LocalType, typename Output>
void BuildField(TypeList<LocalType&>,
    Priority<1>,
    InvokeContext& invoke_context,
    LocalType&& value,
    Output&& output,
    typename Output::CapType::Client* enable = nullptr)
{
    output.set(kj::heap<ProxyServer<typename Output::CapType>>(&value, false /* owned */, invoke_context.loop));
}

template <typename LocalType, typename Value, typename Output>
void BuildField(TypeList<boost::optional<LocalType>>,
    Priority<1>,
    InvokeContext& invoke_context,
    Value&& value,
    Output&& output)
{
    if (value) {
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
    using CapType = typename CapSetterMethodTraits<decltype(&Output::setValue)>::CapType;
};

template <typename Output>
struct ValueOutput<Output, Require<decltype(std::declval<Output>().initValue())>>
{
    Output& m_output;
    template <typename... Params>
    auto set(Params&&... params) -> AUTO_RETURN(this->m_output.initValue(std::forward<Params>(params)...))
};

template <typename KeyLocalType, typename ValueLocalType, typename Value, typename Output>
void BuildField(TypeList<std::pair<KeyLocalType, ValueLocalType>>,
    Priority<1>,
    InvokeContext& invoke_context,
    Value&& value,
    Output&& output)
{
    auto kv = output.set();
    BuildField(
        TypeList<KeyLocalType>(), BuildFieldPriority(), invoke_context, value.first, KeyOutput<decltype(kv)>{kv});
    BuildField(
        TypeList<ValueLocalType>(), BuildFieldPriority(), invoke_context, value.second, ValueOutput<decltype(kv)>{kv});
}

template <typename KeyLocalType, typename ValueLocalType, typename Value, typename Output>
void BuildField(TypeList<std::tuple<KeyLocalType, ValueLocalType>>,
    Priority<1>,
    InvokeContext& invoke_context,
    Value&& value,
    Output&& output)
{
    auto kv = output.set();
    BuildField(TypeList<KeyLocalType>(), BuildFieldPriority(), invoke_context, std::get<0>(value),
        KeyOutput<decltype(kv)>{kv});
    BuildField(TypeList<ValueLocalType>(), BuildFieldPriority(), invoke_context, std::get<1>(value),
        ValueOutput<decltype(kv)>{kv});
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
    output.set(BuildPrimitive(
        invoke_context, std::forward<Value>(value), TypeList<Decay<typename Decay<Output>::CapType>>()));
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
    auto accessor = ProxyStruct<Struct>::get(Index());
    FieldOutput<typename Struct::Builder, decltype(accessor)> field_output{output, accessor};
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
    BuildOne<0>(local_type, invoke_context, value, output.set());
}

template <typename CapType,
    typename LocalType,
    typename ServerContext,
    typename Accessor,
    typename Fn,
    typename... Args>
void PassField(TypeList<CapType>,
    TypeList<LocalType*>,
    ServerContext& server_context,
    const Accessor& accessor,
    const Fn& fn,
    Args&&... args)
{
    InvokeContext& invoke_context = server_context;
    boost::optional<Decay<LocalType>> param;
    auto&& input = Make<FieldInput>(server_context.method_context.getParams(), accessor);
    bool want = input.want();
    if (want) {
        ReadField(TypeList<LocalType>(), invoke_context, input, Emplace<decltype(param)>(param));
        if (!param) param.emplace();
    }
    fn.invoke(server_context, std::forward<Args>(args)..., param ? &*param : nullptr);
    auto&& results = server_context.method_context.getResults();
    if (want)
        BuildField(
            TypeList<LocalType>(), BuildFieldPriority(), invoke_context, *param, Make<FieldOutput>(results, accessor));
}

template <typename CapType,
    typename LocalType,
    typename ServerContext,
    typename Accessor,
    typename Fn,
    typename... Args>
Require<typename CapType::Client> PassField(TypeList<CapType>,
    TypeList<LocalType&>,
    ServerContext& server_context,
    const Accessor& accessor,
    const Fn& fn,
    Args&&... args)
{
    // Just drop argument if it is a reference to an interface client, because
    // it would be unclear when the the client should be released. Server will
    // need to provide a custom invokeMethod overload in order to access the
    // client, and can arrange for it to be disposed at the appropriate time.
    fn.invoke(server_context, std::forward<Args>(args)...);
}

template <typename CapType,
    typename LocalType,
    typename ServerContext,
    typename Accessor,
    typename Fn,
    typename... Args>
void PassField(TypeList<CapType>,
    TypeList<LocalType>,
    ServerContext& server_context,
    const Accessor& accessor,
    const Fn& fn,
    Args&&... args)
{
    InvokeContext& invoke_context = server_context;
    boost::optional<Decay<LocalType>> param;
    ReadField(TypeList<LocalType>(), invoke_context,
        Make<FieldInput>(server_context.method_context.getParams(), accessor), Emplace<decltype(param)>(param));
    if (!param) param.emplace();
    fn.invoke(server_context, std::forward<Args>(args)..., static_cast<LocalType&&>(*param));
    auto&& results = server_context.method_context.getResults();
    BuildField(
        TypeList<LocalType>(), BuildFieldPriority(), invoke_context, *param, Make<FieldOutput>(results, accessor));
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
        template <typename Params, typename ParamList>
        void handleField(InvokeContext& invoke_context, Params& params, ParamList)
        {
        }

        BuildParams(ClientException* client_exception) : m_client_exception(client_exception) {}
        ClientException* m_client_exception;
    };

    struct ReadResults : FieldChainHelper<ReadResults, 0>
    {
        template <typename Results, typename ParamList>
        void handleField(InvokeContext& invoke_context, Results& results, ParamList)
        {
            FieldInput<Results, Accessor> input(results, m_client_exception->m_accessor);
            if (input.has()) {
                ReadField(TypeList<Exception>(), invoke_context, input, ThrowEmplace<Exception>());
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
        void handleField(ClientInvokeContext& invoke_context, Params& params, ParamList)
        {
            CallBuild<0>(ParamList(), invoke_context, params, Priority<1>());
        }

        template <size_t I, typename FieldParams, typename Params, typename... Values>
        void CallBuild(FieldParams field_params,
            ClientInvokeContext& invoke_context,
            Params& params,
            typename std::enable_if<(I < sizeof...(Types)), Priority<1>>::type priority,
            Values&&... values)
        {
            // std::move below wonky
            CallBuild<I + 1>(field_params, invoke_context, params, priority, std::forward<Values>(values)...,
                std::move(std::get<I>(m_client_param->m_values)));
        }

        template <size_t I, typename FieldParams, typename Params, typename... Values>
        void CallBuild(FieldParams field_params,
            ClientInvokeContext& invoke_context,
            Params& params,
            Priority<0>,
            Values&&... values)
        {
            BuildField(field_params, BuildFieldPriority(), invoke_context, std::forward<Values>(values)...,
                Make<FieldOutput>(params, m_client_param->m_accessor));
        }

        BuildParams(ClientParam* client_param) : m_client_param(client_param) {}
        ClientParam* m_client_param;
    };

    struct ReadResults : FieldChainHelper<ReadResults, sizeof...(Types)>
    {
        template <typename Results, typename ParamList>
        void handleField(ClientInvokeContext& invoke_context, Results& results, ParamList)
        {
            FieldInput<Results, Accessor> reader(results, m_client_param->m_accessor);
            readTuple<0>(ParamList(), invoke_context, reader);
        }

        static constexpr size_t argc = sizeof...(Types);

        template <int n, typename... Args>
        typename std::enable_if<(n < argc)>::type readTuple(Args&&... args)
        {
            readTuple<n + 1>(std::forward<Args>(args)..., std::get<n>(m_client_param->m_values));
        }

        template <int n, typename ParamList, typename Input, typename... Args>
        typename std::enable_if<n == argc && 0 < argc>::type readTuple(ParamList,
            ClientInvokeContext& invoke_context,
            Input&& input,
            Args&&... args)
        {
            ClientReadField(ParamList(), invoke_context, input, std::forward<Args>(args)...);
        }

        template <int n, typename ParamList, typename Input>
        typename std::enable_if<n == 0 && argc == 0>::type readTuple(ParamList, ClientInvokeContext&, Input&&)
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

template <typename Server, typename MethodContext_>
struct ServerInvokeContext : InvokeContext
{
    using MethodContext = MethodContext_;
    using Results = decltype(std::declval<MethodContext>().getResults());

    Server& proxy_server;
    MethodContext& method_context;
    int req;

    ServerInvokeContext(Server& server, MethodContext& method_context, int req)
        : InvokeContext{*server.m_loop}, proxy_server{server}, method_context{method_context}, req{req}
    {
    }
};

//! Safely convert char pointer to kj pointer.
static inline kj::byte* ByteCast(char* c) { return reinterpret_cast<kj::byte*>(c); }
static inline const kj::byte* ByteCast(const char* c) { return reinterpret_cast<const kj::byte*>(c); }

template <int method_id_, typename MethodResult, typename MethodObj, typename... MethodParams>
struct ServerMethod
{
    MethodResult (MethodObj::*m_method)(MethodParams...);
    static constexpr int method_id = method_id_;
    using ArgTypes = TypeList<MethodParams...>;
    using InvokePriority = Priority<1>;

    // FIXME: maybe call method_context.releaseParams() here
    template <typename ServerContext, typename... Args>
    auto invoke(ServerContext& server_context, Priority<0>, Args&&... args) const
        -> AUTO_RETURN(((*server_context.proxy_server.m_impl).*this->m_method)(std::forward<Args>(args)...))

        // FIXME: maybe call method_context.releaseParams() here
        template <typename ServerContext, typename... Args>
        auto invoke(ServerContext& server_context, Priority<1>, Args&&... args) const
        -> AUTO_RETURN((server_context.proxy_server.invokeMethod(server_context,
            server_context.method_context,
            std::forward<Args>(args)...)))
};

struct ServerDestroy
{
    using ArgTypes = TypeList<>;
    using InvokePriority = Priority<0>;

    template <typename ServerContext, typename... Args>
    void invoke(ServerContext& server_context, InvokePriority, Args&&... args) const
    {
        server_context.proxy_server.invokeDestroy(true /* remote */, std::forward<Args>(args)...);
    }
};

template <int method_id, typename MethodResult, typename MethodObj, typename... MethodParams>
ServerMethod<method_id, MethodResult, MethodObj, MethodParams...> MakeServerMethod(
    MethodResult (MethodObj::*method)(MethodParams...))
{
    return {method};
}

template <typename Accessor, typename Parent>
struct ServerRet : Parent
{
    ServerRet(Accessor accessor, Parent parent) : Parent(parent), m_accessor(accessor) {}

    template <typename ServerContext, typename... Args>
    void invoke(ServerContext& server_context, Args&&... args) const
    {
        auto&& result = Parent::invoke(server_context, std::forward<Args>(args)...);
        auto&& results = server_context.method_context.getResults();
        InvokeContext& invoke_context = server_context;
        BuildField(TypeList<decltype(result)>(), BuildFieldPriority(), invoke_context, std::move(result),
            Make<FieldOutput>(results, m_accessor));
    }

    Accessor m_accessor;
};

template <typename Exception, typename Accessor, typename Parent>
struct ServerExcept : Parent
{
    ServerExcept(Accessor accessor, Parent parent) : Parent(parent), m_accessor(accessor) {}

    template <typename ServerContext, typename... Args>
    void invoke(ServerContext& server_context, Args&&... args) const
    {
        try {
            return Parent::invoke(server_context, std::forward<Args>(args)...);
        } catch (const Exception& e) {
            auto&& results = server_context.method_context.getResults();
            InvokeContext& invoke_context = server_context;
            BuildField(TypeList<Exception>(), BuildFieldPriority(), server_context, e,
                Make<FieldOutput>(results, m_accessor));
        }
    }

    Accessor m_accessor;
};

template <int argc, typename Accessor, typename Parent>
struct ServerField : Parent
{
    ServerField(Accessor accessor, Parent parent) : Parent(parent), m_accessor(accessor) {}

    Accessor m_accessor;

    using _Split = Split<Parent::ArgTypes::size - argc, typename Parent::ArgTypes>;
    using ArgTypes = typename _Split::First;
    using LocalTypes = typename _Split::Second;

    const Parent& parent() const { return *this; }

    template <typename ServerContext, typename... Args>
    auto invoke(ServerContext& server_context, Args&&... args) const
        -> AUTO_RETURN(PassField(TypeList<typename Accessor::CapType>(),
            LocalTypes(),
            server_context,
            this->m_accessor,
            this->parent(),
            std::forward<Args>(args)...))
};

template <int argc, typename Accessor, typename Parent>
ServerField<argc, Accessor, Parent> MakeServerField(Accessor accessor, Parent parent)
{
    return {accessor, parent};
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

template <typename FieldTypes, typename GetRequest, typename ProxyClient, typename... Params>
void clientInvoke(FieldTypes fields, const GetRequest& get_request, ProxyClient& proxy_client, Params&&... params)
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

        FieldChain().handleChain(invoke_context, request, fields, typename Params::BuildParams{&params}...);
        // FIXME: This works with gcc, gives error on argc/argv overload with clang++
        // Should either strip it out removing ClientBuild class, buildParams
        // method definitino, and whatever else, or fix/replace
        // CallWithTupleParams(Make<ClientBuild>(proxy_client, request), params.m_values...);
        LogPrint(::BCLog::IPC, "{%s} IPC client send %s %s\n", invoke_context.client_thread.waiter->m_name,
            TypeName<typename Request::Params>(), request.toString().flatten().cStr());

        proxy_client.m_loop->m_task_set.add(
            request.send().then([&](::capnp::Response<typename Request::Results>&& response) {
                LogPrint(::BCLog::IPC, "{%s} IPC client recv %s %s\n", invoke_context.client_thread.waiter->m_name,
                    TypeName<typename Request::Results>(), response.toString().flatten().cStr());

                // FIXME: This works with gcc, gives error on argc/argv overload with clang++
                // Should either strip it out removing ClientRead class, readResults
                // method definitino, and whatever else, or fix/replace
                // CallWithTupleParams(Make<ClientRead>(proxy_client, response), params.m_values...);
                try {
                    FieldChain().handleChain(
                        invoke_context, response, fields, typename Params::ReadResults{&params}...);
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

template <typename Server, typename MethodContext, typename Fn>
kj::Promise<void> serverInvoke(Server& server, MethodContext& method_context, Fn fn)
{
    auto params = method_context.getParams();
    using Params = decltype(params);

    int req = ++server_reqs;
    LogIpc(*server.m_loop, "IPC server recv request #%i %s %s\n", req, TypeName<typename Params::Reads>(),
        params.toString().flatten().cStr());

    try {
        using ServerContext = ServerInvokeContext<Server, MethodContext>;
        ServerContext server_context{server, method_context, req};
        return ReplaceVoid([&]() { return fn.invoke(server_context, typename Fn::InvokePriority()); },
            [&]() { return kj::Promise<MethodContext>(kj::mv(method_context)); })
            .then([&server, req](MethodContext method_context) {
                LogIpc(*server.m_loop, "IPC server send request #%i %s %s\n", req,
                    TypeName<typename ServerContext::Results::Builds>(),
                    method_context.getResults().toString().flatten().cStr());
            });
    } catch (...) {
        LogIpc(
            *server.m_loop, "IPC server unhandled exception %s\n", boost::current_exception_diagnostic_information());
        throw;
    }
}

} // namespace capnp
} // namespace interfaces

#endif // BITCOIN_INTERFACES_CAPNP_PROXY_H
