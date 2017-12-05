#ifndef BITCOIN_INTERFACES_BASE_H
#define BITCOIN_INTERFACES_BASE_H

#include <cassert>
#include <memory>

namespace interfaces {

class Base;

//! Close hook.
class CloseHook
{
public:
    virtual ~CloseHook() {}

    //! Handle interface being disconnected.
    virtual void onClose(Base& interface, bool remote) {}

    std::unique_ptr<CloseHook> m_next_hook;
};

//! Close hook that encapsulate and deletes a moveable object.
template <typename T>
class Deleter : public CloseHook
{
public:
    Deleter(T&& value) : m_value(std::move(value)) {}
    void onClose(Base& interface, bool remote) override { T(std::move(m_value)); }
    T m_value;
};

//! Base class for interfaces.
class Base
{
public:
    virtual ~Base();

    //! Add close hook.
    void addCloseHook(std::unique_ptr<CloseHook> close_hook);

    // Call close hooks.
    void close(bool remote);

    std::unique_ptr<CloseHook> m_close_hook;
};

} // namespace interfaces

#endif // BITCOIN_INTERFACES_BASE_H
