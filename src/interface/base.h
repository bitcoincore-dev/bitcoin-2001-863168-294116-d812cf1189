#ifndef BITCOIN_INTERFACE_BASE_H
#define BITCOIN_INTERFACE_BASE_H

#include <cassert>
#include <memory>

namespace interface {

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

//! Base class for interfaces.
class Base
{
public:
    virtual ~Base() { close(false /* remote */); }

    //! Add close hook.
    void addCloseHook(std::unique_ptr<CloseHook> close_hook)
    {
        // FIXME: move to cpp
        assert(close_hook);
        assert(!close_hook->m_next_hook);
        close_hook->m_next_hook = std::move(m_close_hook);
        m_close_hook = std::move(close_hook);
    }

    // Call close hooks.
    void close(bool remote)
    {
        // FIXME: move to cpp
        for (auto hook = std::move(m_close_hook); hook; hook = std::move(hook->m_next_hook)) {
            hook->onClose(*this, false);
        }
    }

    std::unique_ptr<CloseHook> m_close_hook;
};

} // namespace interface

#endif // BITCOIN_INTERFACE_BASE_H
