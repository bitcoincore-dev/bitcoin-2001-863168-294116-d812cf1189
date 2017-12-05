#ifndef BITCOIN_INTERFACE_CAPNP_UTIL_H
#define BITCOIN_INTERFACE_CAPNP_UTIL_H

#include <tuple>
#include <type_traits>
#include <utility>

namespace interface {
namespace capnp {

// C++11 workaround for C++14 auto return functions
// (http://en.cppreference.com/w/cpp/language/template_argument_deduction#auto-returning_functions)
#define AUTO_RETURN(x) \
    decltype(x) { return x; }

//! Shortcut for remove_cv + remove_reference.
//!
//! Example:
//!   is_same<int, Plain<const int&&>>
template <typename T>
using Plain = typename std::remove_cv<typename std::remove_reference<T>::type>::type;

//! Type holding a list of types.
//!
//! Example:
//!   TypeList<int, bool, void>
template <typename... Types>
struct TypeList
{
};

//! Split TypeList into two halfs at position index.
//!
//! Example:
//!   is_same<TypeList<int, double>, Split<2, TypeList<int, double, float, bool>>::First>
//!   is_same<TypeList<float, bool>, Split<2, TypeList<int, double, float, bool>>::Last>
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

// Map any type to void. Useful for SFINAE with template type parameters.
template <typename T>
using Void = void;

//! Construct a template class value by deducing template arguments from the
//! types of constructor arguments, so they don't need to be specified manually.
//!
//! Example:
//!   Make<std::pair>(5, true) // Constructs std::pair<int, bool>(5, true);
template <template <typename...> class Class, typename... Args>
Class<Args...> Make(Args&&... args)
{
    return Class<Args...>{std::forward<Args>(args)...};
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

template <size_t index, typename Tuple, typename std::enable_if<index != std::tuple_size<Tuple>::value, int>::type = 0>
typename std::tuple_element<index, Tuple>::type& NullGet(Tuple& tuple)
{
    return std::get<index>(tuple);
}

template <size_t index, typename Tuple, typename std::enable_if<index == std::tuple_size<Tuple>::value, int>::type = 0>
std::nullptr_t NullGet(Tuple& tuple)
{
    return nullptr;
}

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
        -> AUTO_RETURN(this->operator() < i + 1 > (NullGet<i>(this->m_tuple), std::forward<Args>(args)...));
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

} // namespace capnp
} // namespace interface

#endif // BITCOIN_INTERFACE_CAPNP_UTIL_H
