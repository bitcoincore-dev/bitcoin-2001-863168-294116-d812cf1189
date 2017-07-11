#ifndef BITCOIN_IPC_CAPNP_UTIL_H
#define BITCOIN_IPC_CAPNP_UTIL_H

#include <ipc/util.h>

#include <boost/exception/diagnostic_information.hpp>
#include <capnp/capability.h>
#include <capnp/common.h>
#include <capnp/orphan.h>
#include <capnp/pretty-print.h>
#include <capnp/schema.h>
#include <kj/async-io.h>
#include <kj/async.h>
#include <kj/async-inl.h>
#include <kj/common.h>
#include <kj/debug.h>
#include <kj/exception.h>
#include <kj/memory.h>
#include <kj/string.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <algorithm>
#include <fstream>
#include <functional>
#include <future>
#include <iostream>
#include <list>
#include <mutex>
#include <string>
#include <thread>
#include <tuple>
#include <type_traits>

namespace ipc {
namespace capnp {

template <typename Inteface>
class Proxy;

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

    using CleanupList = std::list<std::function<void()>>;
    using CleanupIt = typename CleanupList::iterator;

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

//! Helper for making blocking capnp IPC calls.
template <typename GetRequest, typename Request = ResultOf<GetRequest>>
struct Call;

//! Template specialization of Call class that breaks Params and Results types
//! out of the cannp::Request type.
template <typename GetRequest, typename Params, typename Results>
struct Call<GetRequest, ::capnp::Request<Params, Results>>
{
    //! Constructor. See #MakeCall wrapper for description of arguments.
    Call(EventLoop& loop, GetRequest&& get_request)
        : m_loop(loop), m_get_request(std::forward<GetRequest>(get_request))
    {
        KJ_ASSERT(std::this_thread::get_id() != loop.m_thread_id,
            "Error: IPC call from IPC callback handler not supported (use async)");
    }

    //! Send IPC request and block waiting for response.
    //!
    //! @param[in]  get_return  callable that will run on the event loop thread
    //!                         after the IPC response is received.
    //! @return                 value returned by #getReturn callable
    template <typename GetReturn>
    ResultOf<GetReturn> send(GetReturn&& get_return)
    {
        std::promise<ResultOf<GetReturn>> promise;
        m_loop.sync([&]() {
            auto request = m_get_request();
            if (m_loop.m_debug) {
                printf("============================================================\n");
                printf("Request %s %p pid %i\n%s\n", ::capnp::Schema::from<Params>().getShortDisplayName().cStr(),
                    this, getpid(), request.toString().flatten().cStr());
            }
            m_loop.m_task_set.add(request.send().then([&](::capnp::Response<Results>&& response) {
                m_response = &response;
                if (m_loop.m_debug) {
                    printf("============================================================\n");
                    printf("Response %s %p pid %i\n%s\n",
                        ::capnp::Schema::from<Results>().getShortDisplayName().cStr(), this, getpid(),
                        response.toString().flatten().cStr());
                }
                SetPromise(promise, get_return);
            }));
        });
        return promise.get_future().get();
    }

    //! Send IPC request and block waiting for response.
    void send()
    {
        send([]() {});
    }

    EventLoop& m_loop;
    GetRequest m_get_request;
    ::capnp::Response<Results>* m_response = nullptr;
};

//! Construct helper object for making blocking IPC calls.
//!
//! @param[in] loop        event loop reference
//! @param[in] get_request callable that will run on the event loop thread, and
//!                        should return an IPC request message
//! @return                #Call helper object. Calling Call::send() will run
//!                        #getRequest and send the IPC.
template <typename GetRequest>
Call<GetRequest> MakeCall(EventLoop& loop, GetRequest&& get_request)
{
    return {loop, std::move(get_request)};
}

// FIXME Rename TypeList
template <typename... Types>
struct TypeList
{
};

//! Method traits class.
// FIXME Rename MethodTraits
template <class Pointer>
struct Method;

template <class _Class, class _Result, class... _Args>
struct Method<_Result (_Class::*)(_Args...)>
{
    using Result = _Result;
    template <size_t N>
    using Arg = typename std::tuple_element<N, std::tuple<_Args...>>::type;
    using Fields =
        typename std::conditional<std::is_same<void, Result>::value, TypeList<_Args...>, TypeList<_Args..., _Result>>::type;
};

template <class _Result, class... _Args>
struct Method<std::function<_Result(_Args...)>>
{
    // FIXME dedup with method specialization above
    using Result = _Result;
    template <size_t N>
    using Arg = typename std::tuple_element<N, std::tuple<_Args...>>::type;
    using Fields =
        typename std::conditional<std::is_same<void, Result>::value, TypeList<_Args...>, TypeList<_Args..., _Result>>::type;
};

template <typename Interface>
class ProxyClientBase
{
public:
    ProxyClientBase(EventLoop& loop, typename Interface::Client client) : m_loop(&loop), m_client(std::move(client))
    {
        m_cleanup = loop.addCleanup([this]() {
            detach();
            m_loop = nullptr;
        });
    }

    virtual void detach() { typename Interface::Client(std::move(m_client)); }

    // Destructor separate method so subclass can run code after it.
    void shutdown()
    {
        if (m_loop) {
            m_loop->sync([&]() {
                m_loop->removeCleanup(m_cleanup);
                detach();
            });
        }
    }

    ~ProxyClientBase() noexcept { shutdown(); }

    template <typename MethodTraits, typename GetRequest, typename... _Args>
    void invoke(MethodTraits, const GetRequest& get_request, _Args&&... args);

    EventLoop* m_loop;
    typename EventLoop::CleanupIt m_cleanup;
    typename Interface::Client m_client;
};

class ProxyServerBase
{
public:
    ProxyServerBase(EventLoop& loop) : m_loop(loop) {}

    template <typename Context,
        typename Object,
        typename Object2,
        typename MethodResult,
        typename... MethodArgs,
        typename... Fields>
    kj::Promise<void> invokeMethod(Context& context,
        Object2& object,
        MethodResult (Object::*method)(MethodArgs...),
        const Fields&... fields);

    template <typename Context, typename FnR, typename... FnArgs, typename... Fields>
    kj::Promise<void> invokeFunction(Context& context, std::function<FnR(FnArgs...)>& fn, const Fields&... fields);

    template <typename Context, typename Object, typename MethodType, typename... Fields>
    kj::Promise<void> invokeMethodAsync(Context& context,
        Object& object,
        const MethodType& method,
        const Fields&... fields);

    template <typename Context, typename FnType, typename... Fields>
    kj::Promise<void> invokeFunctionAsync(Context& context, FnType& fn, const Fields&... fields);

    EventLoop& m_loop;
};

//! Invoke callback returning Void if callback returns void
// FIXME Rename ReturnVoid
template <typename Callable,
    typename Result = ResultOf<Callable>,
    typename Enable = typename std::enable_if<!std::is_same<Result, void>::value>::type>
Result ReplaceVoid(Callable&& callable)
{
    return callable();
}

template <typename Callable,
    typename Result = ResultOf<Callable>,
    typename Enable = typename std::enable_if<std::is_same<Result, void>::value>::type>
::capnp::Void ReplaceVoid(Callable&& callable)
{
    callable();
    return {};
}

//! Split TypeList list into two parts
// FIXME Rename SplitArgs, ArgsOut->_First, Result->First, ArgsIn->_Second, NextArgs->Second
template <int argc, typename ArgsIn, typename ArgsOut = TypeList<>, bool base = argc == 0>
struct PopArgs;

template <typename ArgsIn, typename ArgsOut>
struct PopArgs<0, ArgsIn, ArgsOut, true>
{
    using Result = ArgsOut;
    using NextArgs = ArgsIn;
};

template <int argc, typename ArgIn, typename... ArgsIn, typename... ArgsOut>
struct PopArgs<argc, TypeList<ArgIn, ArgsIn...>, TypeList<ArgsOut...>, false>
{
    using Next = PopArgs<argc - 1, TypeList<ArgsIn...>, TypeList<ArgsOut..., ArgIn>>;
    using Result = typename Next::Result;
    using NextArgs = typename Next::NextArgs;
};

template <typename Getter, typename Setter, typename OptionalGetter, typename OptionalSetter>
struct Accessor
{
    Getter getter;
    Setter setter;
    OptionalGetter optional_getter;
    OptionalSetter optional_setter;
};

template <typename Getter, typename Setter, typename OptionalGetter, typename OptionalSetter>
Accessor<Getter, Setter, OptionalGetter, OptionalSetter> MakeAccessor(Getter getter,
    Setter setter,
    OptionalGetter optional_getter,
    OptionalSetter optional_setter)
{
    return {getter, setter, optional_getter, optional_setter};
}

//! Function object that assigns all arguments to members of a tuple
// Could be replaced by lambda with c++14
template <typename... Types>
struct AssignTuple
{
    AssignTuple(std::tuple<Types...>& tuple) : m_tuple(tuple) {}
    std::tuple<Types...>& m_tuple;

    template <typename... Values>
    void operator()(Values&&... values)
    {
        assign<0>(std::forward<Values>(values)...);
    }

    template <size_t I, typename Value, typename... Values>
    void assign(Value&& value, Values&&... values)
    {
        // FIXME Weird rvalue issue std::move below
        std::get<I>(m_tuple) = std::move(value);
        assign<I + 1>(values...);
    }

    template <size_t I>
    void assign()
    {
    }
};

template <int priority>
struct Priority : Priority<priority - 1>
{
};

template <>
struct Priority<0>
{
};

template <int priority, bool enable>
using PriorityEnable = typename std::enable_if<enable, Priority<priority>>::type;

template <int argn, typename Accessor, typename... Types>
struct ClientArg
{
    ClientArg(Accessor accessor, Types&... values) : m_accessor(accessor), m_values(values...) {}

    struct BuildParams
    {
        template <typename MethodArgs, typename Params, typename Fn, typename... FwdArgs>
        void operator()(MethodArgs method_args, EventLoop& loop, Params& params, Fn&& fn, FwdArgs&&... fwd_args)
        {
            (*this)(method_args, loop, params);
            using Pop = PopArgs<sizeof...(Types), MethodArgs>;
            fn(typename Pop::NextArgs(), loop, params, std::forward<FwdArgs>(fwd_args)...);
        }

        template <typename MethodArgs, typename Params>
        void operator()(MethodArgs, EventLoop& loop, Params& params)
        {
            using Pop = PopArgs<sizeof...(Types), MethodArgs>;
            using FieldArgs = typename Pop::Result;
            CallBuild<0>(FieldArgs(), loop, params, Priority<1>());
        }

        template <size_t I, typename FieldArgs, typename Params, typename... Values>
        void CallBuild(FieldArgs field_args,
            EventLoop& loop,
            Params& params,
            PriorityEnable<1, (I < sizeof...(Types))> priority,
            Values&&... values)
        {
            // std::move below wonky
            CallBuild<I + 1>(field_args, loop, params, priority, std::forward<Values>(values)...,
                std::move(std::get<I>(m_client_arg->m_values)));
        }

        template <size_t I, typename FieldArgs, typename Params, typename... Values>
        void CallBuild(FieldArgs field_args, EventLoop& loop, Params& params, Priority<0>, Values&&... values)
        {
            BuildField(field_args, Priority<2>(), loop, std::forward<Values>(values)...,
                m_client_arg->m_accessor.setter, m_client_arg->m_accessor.optional_setter, params);
        }

        ClientArg* m_client_arg;
    };

    struct ReadResults
    {
        template <typename MethodArgs, typename Results, typename Fn, typename... FwdArgs>
        void operator()(MethodArgs method_args,
            EventLoop& loop,
            const Results& results,
            Fn&& fn,
            FwdArgs&&... fwd_args)
        {
            (*this)(method_args, loop, results);
            using Pop = PopArgs<sizeof...(Types), MethodArgs>;
            fn(typename Pop::NextArgs(), loop, results, std::forward<FwdArgs>(fwd_args)...);
        }
        template <typename MethodArgs, typename Results>
        void operator()(MethodArgs method_args, EventLoop& loop, Results& results)
        {
            readTuple(method_args, loop, results, m_client_arg->m_accessor.getter);
        }

        template <typename MethodArgs, typename Results, typename Getter>
        void readTuple(MethodArgs, EventLoop& loop, Results& results, const Getter&)
        {
            using Pop = PopArgs<sizeof...(Types), MethodArgs>;
            using FieldArgs = typename Pop::Result;
            PassField(FieldArgs(), ReadField(FieldArgs(), Priority<1>(), loop, results,
                                       m_client_arg->m_accessor.getter, m_client_arg->m_accessor.optional_getter),
                AssignTuple<Types&...>(m_client_arg->m_values));
        }

        template <typename MethodArgs, typename Results>
        void readTuple(MethodArgs, EventLoop& loop, Results& results, std::nullptr_t)
        {
            // don't try to assign to tuple if no getter
            // could lead to compile errors because arg references may be const
        }

        ClientArg* m_client_arg;
    };

    Accessor m_accessor;
    std::tuple<Types&...> m_values;
    ClientArg* m_client_arg;
};

template <int argn, typename Accessor, typename... Types>
ClientArg<argn, Accessor, Types...> MakeClientArg(Accessor accessor, Types&... values)
{
    return {std::move(accessor), values...};
}

template <typename... BindArgs, typename Fn, typename... FnArgs>
void CallFn(BindArgs... bind_args, Fn&& fn, FnArgs&&... fn_args)
{
    fn(bind_args..., std::forward<FnArgs>(fn_args)...);
}

template <typename... BindArgs>
void CallFn(BindArgs... bind_args)
{
}

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

//! Safely convert unusual char pointer types to standard ones.
static inline char* CharCast(unsigned char* c) { return reinterpret_cast<char*>(c); }
static inline const char* CharCast(const unsigned char* c) { return reinterpret_cast<const char*>(c); }

//! Safely convert char pointer to kj pointer.
static inline kj::byte* ByteCast(char* c) { return reinterpret_cast<kj::byte*>(c); }
static inline const kj::byte* ByteCast(const char* c) { return reinterpret_cast<const kj::byte*>(c); }

// Adapter to let BuildField overloads methods work set & init list elements as
// if they were fields of a struct. If BuildField is changed to use some kind of
// accessor class instead of calling method pointers, then then maybe this could
// go away or be simplified, because would no longer be a need to return
// ListElemBuilder method pointers emulating capnp struct method pointers..
template <typename ListType>
struct ListElemBuilder;

template <typename T, ::capnp::Kind kind>
struct ListElemBuilder<::capnp::List<T, kind>>
{
    using List = ::capnp::List<T, kind>;

    ListElemBuilder(typename List::Builder& builder, size_t index) : m_builder(builder), m_index(index) {}

    template <typename R, typename... A>
    R fwd_set(const A&... a)
    {
        m_builder.set(m_index, a...);
    }

    template <typename R, typename... A>
    R fwd_init(const A&... a)
    {
        return m_builder.init(m_index, a...);
    }

    template <typename R>
    R fwd_get_mutable()
    {
        return m_builder[m_index];
    }

    using PrimitiveSet = void (ListElemBuilder::*)(T);
    template <typename U = void, typename Enable = typename std::enable_if<kind == ::capnp::Kind::PRIMITIVE, U>::type>
    PrimitiveSet getSetter(Enable* enable = nullptr)
    {
        return &ListElemBuilder::template fwd_set<void, T>;
    }

    using BlobInit = typename T::Builder (ListElemBuilder::*)(const size_t&);
    template <typename U = void, typename Enable = typename std::enable_if<kind == ::capnp::Kind::BLOB, U>::type>
    BlobInit getSetter(Enable* enable = nullptr)
    {
        return &ListElemBuilder::template fwd_init<typename T::Builder, size_t>;
    }

    using StructInit = typename T::Builder (ListElemBuilder::*)();
    template <typename U = void, typename Enable = typename std::enable_if<kind == ::capnp::Kind::STRUCT, U>::type>
    StructInit getSetter(Enable* enable = nullptr)
    {
        return &ListElemBuilder::template fwd_get_mutable<typename T::Builder>;
    }

    typename List::Builder& m_builder;
    size_t m_index;
};

template <int argc, typename Accessor>
struct ServerField
{
    template <typename MethodArgs, typename Params, typename Results, typename Fn, typename... FwdArgs>
    void operator()(MethodArgs,
        EventLoop& loop,
        const Params& params,
        Results& results,
        Fn&& fn,
        FwdArgs&&... fwd_args) const
    {
        using Pop = PopArgs<argc, MethodArgs>;
        using FieldArgs = typename Pop::Result;
        auto value =
            ReadField(FieldArgs(), Priority<1>(), loop, params, m_accessor.getter, m_accessor.optional_getter);
        // FIXME Weird rvalue issue std::move below
        PassField(FieldArgs(), std::move(value), fn, typename Pop::NextArgs(), loop, params, results,
            std::forward<FwdArgs>(fwd_args)...);
        BuildField(FieldArgs(), Priority<2>(), loop, std::move(value), m_accessor.setter, m_accessor.optional_setter,
            results);
    }

    Accessor m_accessor;
};

template <typename Accessor>
struct ServerFieldRet
{
    template <typename Params, typename Results, typename Fn, typename... FwdArgs>
    void operator()(TypeList<> args,
        EventLoop& loop,
        const Params& params,
        Results& results,
        Fn&& fn,
        FwdArgs&&... fwd_args) const
    {
        // FIXME Weird rvalue issue static_cast below
        auto&& result = fn(args, loop, params, results, static_cast<FwdArgs&>(fwd_args)...);
        using FieldType = TypeList<decltype(result)>;
        BuildField(FieldType(), Priority<2>(), loop, std::move(result), m_accessor.setter, m_accessor.optional_setter,
            results);
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

template <typename Interface>
template <typename MethodTraits, typename GetRequest, typename... _Args>
void ProxyClientBase<Interface>::invoke(MethodTraits, const GetRequest& get_request, _Args&&... args)
{
    if (!m_loop) {
        std::cerr << "ProxyServerClient call made after event loop shutdown." << std::endl;
        return;
    }
    using MethodArgs = typename MethodTraits::Fields;
    auto call = MakeCall(*m_loop, [&]() {
        auto request = (m_client.*get_request)(nullptr);
        using Params = decltype(request);
        CallFn<MethodArgs, EventLoop&, Params&>(MethodArgs(), *m_loop, request, typename _Args::BuildParams{&args}...);
        return request;
    });
    call.send([&]() {
        using Results = decltype(*call.m_response);
        CallFn<MethodArgs, EventLoop&, const Results&>(
            MethodArgs(), *m_loop, *call.m_response, typename _Args::ReadResults{&args}...);
    });
}

template <typename MethodArgs, typename Context, typename Fn, typename... Fields>
inline kj::Promise<void> serverInvoke(Context& context, EventLoop& loop, const Fn& fn, const Fields&... fields)
{
    auto params = context.getParams();
    auto results = context.getResults();
    using Params = decltype(params);
    using Results = decltype(results);
    try {
        CallFn<MethodArgs, EventLoop&, const Params&, Results>(MethodArgs(), loop, params, results, fields..., fn);
    } catch (...) {
        std::cerr << "ProxyServerBase unhandled exception" << std::endl
                  << boost::current_exception_diagnostic_information();
        throw;
    }
    return kj::READY_NOW;
}

template <typename Context,
    typename Object,
    typename Object2,
    typename MethodResult,
    typename... MethodArgs,
    typename... Fields>
kj::Promise<void> ProxyServerBase::invokeMethod(Context& context,
    Object2& object,
    MethodResult (Object::*method)(MethodArgs...),
    const Fields&... fields)
{
    using Params = decltype(context.getParams());
    using Results = decltype(context.getResults());
    return serverInvoke<TypeList<MethodArgs...>>(context, m_loop,
        [&](TypeList<>, EventLoop& loop, const Params& params, Results& results, MethodArgs&... method_args) {
            // FIXME: maybe call context.releaseParams() here
            return ReplaceVoid([&]() { return (object.*method)(static_cast<MethodArgs>(method_args)...); });
        },
        fields...);
}

template <typename Context, typename FnR, typename... FnArgs, typename... Fields>
kj::Promise<void> ProxyServerBase::invokeFunction(Context& context,
    std::function<FnR(FnArgs...)>& fn,
    const Fields&... fields)
{
    using Params = decltype(context.getParams());
    using Results = decltype(context.getResults());
    return serverInvoke<TypeList<FnArgs...>>(context, m_loop,
        [&](TypeList<>, EventLoop& loop, const Params& params, Results& results, FnArgs&... fn_args) {
            // FIXME: maybe call context.releaseParams() here
            // fixme: static cast needs to do right hting for lvalue ref args
            return ReplaceVoid([&]() { return fn(static_cast<FnArgs&&>(fn_args)...); });
        },
        fields...);
}

template <typename Context, typename Object, typename MethodType, typename... Fields>
kj::Promise<void> ProxyServerBase::invokeMethodAsync(Context& context,
    Object& object,
    const MethodType& method,
    const Fields&... fields)
{
    return m_loop.async(kj::mvCapture(context,
        [this, &object, method, fields...](Context context) { invokeMethod(context, object, method, fields...); }));
}

template <typename Context, typename FnType, typename... Fields>
kj::Promise<void> ProxyServerBase::invokeFunctionAsync(Context& context, FnType& fn, const Fields&... fields)
{
    return m_loop.async(
        kj::mvCapture(context, [this, &fn, fields...](Context context) { invokeFunction(context, fn, fields...); }));
}

} // namespace capnp
} // namespace ipc

#endif // BITCOIN_IPC_CAPNP_UTIL_H
