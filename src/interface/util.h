#ifndef BITCOIN_INTERFACE_UTIL_H
#define BITCOIN_INTERFACE_UTIL_H

#include <functional>
#include <memory>
#include <utility>

namespace interface {

//! Substitute for C++14 std::make_unique.
template <typename T, typename... Args>
std::unique_ptr<T> MakeUnique(Args&&... args)
{
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

//! Extension of std::reference_wrapper with operator< for use as map key.
template <typename T>
class Ref : public std::reference_wrapper<T>
{
public:
    using std::reference_wrapper<T>::reference_wrapper;
    bool operator<(const Ref<T>& other) const { return this->get() < other.get(); }
};

} // namespace interface

#endif // BITCOIN_INTERFACE_UTIL_H
