#ifndef BITCOIN_INTERFACES_CAPNP_UTIL_H
#define BITCOIN_INTERFACES_CAPNP_UTIL_H

#include <capnp/schema.h>

#include <future>
#include <list>
#include <mutex>
#include <tuple>
#include <type_traits>
#include <utility>

namespace interfaces {
namespace capnp {

// C++11 workaround for C++14 auto return functions
// (http://en.cppreference.com/w/cpp/language/template_argument_deduction#auto-returning_functions)
#define AUTO_DO_RETURN(pre, x) \
    decltype(x)                \
    {                          \
        pre;                   \
        return x;              \
    }

#define AUTO_RETURN(x) AUTO_DO_RETURN(, x)

//! Shortcut for std::decay.
template <typename T>
using Decay = typename std::decay<T>::type;

//! Empty object that discards any arguments it is initialized with. Useful as
//! function argument, to avoid having to write a template function when don't
//! care about values of generic arguments.
struct Discard
{
    template <typename... Args>
    Discard(Args&&...)
    {
    }
};

//! Invoke callable `fn()` that may return void. If it does return void, replace
//! return value with value of `ret()`. This is useful for avoiding code
//! duplication and branching in generic code that forwards calls to functions.
template <typename Fn, typename Ret>
auto ReplaceVoid(Fn&& fn, Ret&& ret) ->
    typename std::enable_if<std::is_same<void, decltype(fn())>::value, decltype(ret())>::type
{
    fn();
    return ret();
}

//! Overload of above for non-void `fn()` case.
template <typename Fn, typename Ret>
auto ReplaceVoid(Fn&& fn, Ret&& ret) ->
    typename std::enable_if<!std::is_same<void, decltype(fn())>::value, decltype(fn())>::type
{
    return fn();
}

//! Type holding a list of types.
//!
//! Example:
//!   TypeList<int, bool, void>
template <typename... Types>
struct TypeList
{
    static constexpr size_t size = sizeof...(Types);
};

//! Split TypeList into two halfs at position index.
//!
//! Example:
//!   is_same<TypeList<int, double>, Split<2, TypeList<int, double, float, bool>>::First>
//!   is_same<TypeList<float, bool>, Split<2, TypeList<int, double, float, bool>>::Second>
template <std::size_t index, typename List, typename _First = TypeList<>, bool done = index == 0>
struct Split;

//! Specialization of above (base case)
template <typename _Second, typename _First>
struct Split<0, _Second, _First, true>
{
    using First = _First;
    using Second = _Second;
};

//! Specialization of above (recursive case)
template <std::size_t index, typename Type, typename... _Second, typename... _First>
struct Split<index, TypeList<Type, _Second...>, TypeList<_First...>, false>
{
    using _Next = Split<index - 1, TypeList<_Second...>, TypeList<_First..., Type>>;
    using First = typename _Next::First;
    using Second = typename _Next::Second;
};

//! SFINAE helper, basically the same as to C++17's void_t, but allowing types other than void to be returned.
template <typename SfinaeExpr, typename Result_>
struct _Require
{
    using Result = Result_;
};
template <typename SfinaeExpr, typename Result = void>
using Require = typename _Require<SfinaeExpr, Result>::Result;

//! Construct a template class value by deducing template arguments from the
//! types of constructor arguments, so they don't need to be specified manually.
//!
//! Example:
//!   Make<std::pair>(5, true) // Constructs std::pair<int, bool>(5, true);
template <template <typename...> class Class, typename... Types, typename... Args>
Class<Types..., typename std::remove_reference<Args>::type...> Make(Args&&... args)
{
    return Class<Types..., typename std::remove_reference<Args>::type...>{std::forward<Args>(args)...};
}

//! Generic utility functions used by capnp code. This mostly consists of
//! helpers that work around lack of C++14 functionality in C++11. This file
//! puts all C++14 workarounds in one place, so if/when bitcoin decides to
//! decides to upgrade to C++14, code can be accordingly simplified.
//!
//! C++14 has two features that really simplify generic programming. One is
//! auto-returning functions
//! (http://en.cppreference.com/w/cpp/language/template_argument_deduction#auto-returning_functions):
//!
//!    auto DoSomething(Arg arg) {
//!        return expression(arg);
//!    }
//!
//! which in c++11 has to be written:
//!
//!    auto DoSomething(Arg arg) -> decltype(expression(arg)) {
//!        return expression(arg);
//!    }
//!
//! Another is generic lambdas (http://en.cppreference.com/w/cpp/language/lambda):
//!
//!    [capture](auto arg) { do_something(arg); }
//!
//! which in c++11 has to be written like
//!
//!    struct DoSomething {
//!        Capture m_capture;
//!
//!        template<typename Arg>
//!        void operator()(Arg arg) {
//!            return do_something(arg);
//!        }
//!     };

//! Functor wrapping std::get.
//!
//! Example:
//!   Get<3>()(a) // Equivalent to std::get<3>(a)
template <std::size_t I>
struct Get
{
    template <typename Tuple>
    auto operator()(Tuple&& tuple) -> decltype(std::get<I>(tuple))&
    {
        return std::get<I>(tuple);
    }
};

//! Functor composing two other functors.
//!
//! Example:
//!   Make<Compose>(sin, atan2)(5, 9) == sin(atan2(5, 9))
template <typename Fn1, typename Fn2>
struct Compose
{
    Fn1&& fn1; // FIXME: Remove rvalue reference?
    Fn2&& fn2;

    template <typename... Args>
    auto operator()(Args&&... args) -> AUTO_RETURN(this->fn1(this->fn2(std::forward<Args>(args)...)))
};

//! See Bind() below.
template <typename Fn, typename BindArgs, typename BoundArgs = TypeList<>>
struct BoundFn;

template <typename Fn, typename... BoundArgs>
struct BoundFn<Fn, TypeList<>, TypeList<BoundArgs...>>
{
    Fn& m_fn;

    template <typename... FreeArgs>
    auto operator()(BoundArgs&... bound_args, FreeArgs&&... free_args)
        -> AUTO_RETURN(this->m_fn(bound_args..., std::forward<FreeArgs>(free_args)...))
};

//! See Bind() below.
template <typename Fn, typename BindArg, typename... BindArgs, typename... BoundArgs>
struct BoundFn<Fn, TypeList<BindArg, BindArgs...>, TypeList<BoundArgs...>>
    : BoundFn<Fn, TypeList<BindArgs...>, TypeList<BoundArgs..., BindArg>>
{
    using Base = BoundFn<Fn, TypeList<BindArgs...>, TypeList<BoundArgs..., BindArg>>;
    BindArg& m_bind_arg;

    BoundFn(Fn& fn, BindArg& bind_arg, BindArgs&... bind_args) : Base{fn, bind_args...}, m_bind_arg(bind_arg) {}

    template <typename... FreeArgs>
    auto operator()(BoundArgs&... bound_args, FreeArgs&&... free_args) ->
        typename std::result_of<Base(BoundArgs&..., BindArg&, FreeArgs...)>::type
    {
        return Base::operator()(bound_args..., m_bind_arg, std::forward<FreeArgs>(free_args)...);
    }
};

//! std::bind replacement
template <typename Fn, typename... BindArgs>
BoundFn<Fn, TypeList<BindArgs...>> Bind(Fn&& fn, BindArgs&... bind_args)
{
    return {fn, bind_args...};
}

//! Return tuple element or nullptr
template <size_t index, typename Tuple, typename std::enable_if<index != std::tuple_size<Tuple>::value, int>::type = 0>
typename std::tuple_element<index, Tuple>::type& NullGet(Tuple& tuple)
{
    return std::get<index>(tuple);
}

//! Specialization of above.
template <size_t index, typename Tuple, typename std::enable_if<index == std::tuple_size<Tuple>::value, int>::type = 0>
std::nullptr_t NullGet(Tuple& tuple)
{
    return nullptr;
}

//! See BindTuple() below.
template <typename Fn, typename Tuple>
struct BoundTupleFn
{
    Fn& m_fn;
    Tuple& m_tuple;
    static constexpr size_t size = std::tuple_size<Tuple>::value;

    template <size_t i = 0,
        typename T = Tuple,
        typename std::enable_if<i == std::tuple_size<T>::value, int>::type = 0,
        typename... Args>
    auto operator()(Args&&... args) -> AUTO_RETURN(this->m_fn(std::forward<Args>(args)...));

    template <size_t i = 0,
        typename T = Tuple,
        typename std::enable_if<i != std::tuple_size<T>::value, int>::type = 0,
        typename... Args>
    auto operator()(Args&&... args)
        -> AUTO_RETURN(this->operator()<i + 1>(NullGet<i>(this->m_tuple), std::forward<Args>(args)...));
};

//! Bind tuple members as arguments to function
template <typename Fn, typename Tuple>
BoundTupleFn<Fn, Tuple> BindTuple(Fn&& fn, Tuple& bind_args)
{
    return {fn, bind_args};
}

//! Safely convert unusual char pointer types to standard ones.
static inline char* CharCast(char* c) { return c; }
static inline const char* CharCast(const char* c) { return c; }
static inline char* CharCast(unsigned char* c) { return reinterpret_cast<char*>(c); }
static inline const char* CharCast(const unsigned char* c) { return reinterpret_cast<const char*>(c); }

//! Thread that can run callbacks.
class AsyncThread
{
public:
    void run();

    //! Queue a callback to run on the thread. Callback should return true if successful, or false it is blocked and
    //! should be queued to rerun later.
    //! processing more callbacks.
    template <typename Callback>
    void post(Callback&& callback)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_ready.emplace_back(std::move(callback));
        m_cv.notify_all();
    }

    void stop()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (m_done) return;
        m_ready.emplace_back([this]() {
            m_done = true;
            return true;
        });
        m_cv.notify_all();
    }

    void cancel()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_done = true;
        m_cv.notify_all();
    }

    //! Run a callback on the thread as soon as possible and wait for it to complete.
    template <typename Callback>
    void sync(Callback&& callback)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        bool done = false;
        m_ready.emplace_front([&]() {
            callback();
            done = true;
            return true;
        });
        m_cv.notify_all();
        m_cv.wait(lock, [&]() { return done; });
    }

    void unblock()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_ready.splice(m_ready.end(), m_blocked);
        m_cv.notify_all();
    }

    ~AsyncThread() { stop(); }

    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::list<std::function<bool()>> m_ready;
    std::list<std::function<bool()>> m_blocked;
    bool m_done = false;
    std::future<void> m_result;
};

template <typename Pointer>
auto MaybeGet(Pointer&& pointer) -> typename std::remove_reference<decltype(*pointer)>::type
{
    return pointer ? *pointer : typename std::remove_reference<decltype(*pointer)>::type();
}

template <typename Pointer, typename Value>
auto MaybeSet(Pointer&& pointer, Value&& value) -> typename std::remove_reference<decltype(*pointer)>::type
{
    if (!pointer) return {};
    auto prev = *pointer;
    *pointer = std::forward<Value>(value);
    return prev;
}

template <typename T>
struct TempSetter
{
    template <typename U>
    TempSetter(T& ref, U&& value) : m_ref(ref), m_prev(std::move(ref))
    {
        m_ref = std::forward<U>(value);
    }

    ~TempSetter() { m_ref = std::move(m_prev); }

    T& m_ref;
    T m_prev;
};

//! Return capnp schema name with filename prefix removed.
template <typename T>
const char* TypeName()
{
    // DisplayName string looks like
    // "interfaces/capnp/messages.capnp:ChainNotifications.resendWalletTransactions$Results"
    // This discards the part of the string before the first ':' character.
    // Another alternative would be to use the displayNamePrefixLength field,
    // but this discards everything before the last '.' character, throwing away
    // the object name, which is useful.
    const char* display_name = ::capnp::Schema::from<T>().getProto().getDisplayName().cStr();
    const char* short_name = strchr(display_name, ':');
    return short_name ? short_name + 1 : display_name;
}

std::string ThreadName(const char* exe_name);
std::string LongThreadName(const char* exe_name);

#define LogIpc(loop, format, ...) \
    LogPrint(::BCLog::IPC, "{%s} " format, LongThreadName((loop).m_exe_name), ##__VA_ARGS__)

} // namespace capnp
} // namespace interfaces

#endif // BITCOIN_INTERFACES_CAPNP_UTIL_H
