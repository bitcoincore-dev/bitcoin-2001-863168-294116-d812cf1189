#ifndef BITCOIN_IPC_UTIL_H
#define BITCOIN_IPC_UTIL_H

#include <future>
#include <memory>

namespace ipc {

//! Substitute for for C++14 std::make_unique.
template <typename T, typename... Args>
std::unique_ptr<T> MakeUnique(Args&&... args)
{
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

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

} // namespace ipc

#endif // BITCOIN_IPC_UTIL_H
