#ifndef BITCOIN_IPC_CAPNP_UTIL_H
#define BITCOIN_IPC_CAPNP_UTIL_H

#include "ipc/util.h"

#include <capnp/capability.h>
#include <capnp/pretty-print.h>
#include <capnp/schema.h>
#include <kj/async-io.h>
#include <kj/async.h>
#include <kj/string.h>

#include <future>
#include <mutex>
#include <thread>

#include <sys/types.h>
#include <unistd.h>

namespace ipc {
namespace capnp {

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

    kj::AsyncIoContext m_io_context;
    LoggingErrorHandler m_error_handler;
    kj::TaskSet m_task_set{m_error_handler};
    std::thread m_thread;
    std::thread::id m_thread_id = std::this_thread::get_id();
    std::mutex m_post_mutex;
    std::function<void()> m_post_fn;
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

} // namespace capnp
} // namespace ipc

#endif // BITCOIN_IPC_CAPNP_UTIL_H
