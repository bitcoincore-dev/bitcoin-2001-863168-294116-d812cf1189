#include <interfaces/base.h>

namespace interfaces {

Base::~Base() { close(false /* remote */); }

void Base::addCloseHook(std::unique_ptr<CloseHook> close_hook)
{
    assert(close_hook);
    assert(!close_hook->m_next_hook);
    close_hook->m_next_hook = std::move(m_close_hook);
    m_close_hook = std::move(close_hook);
}

void Base::close(bool remote)
{
    for (auto hook = std::move(m_close_hook); hook; hook = std::move(hook->m_next_hook)) {
        hook->onClose(*this, remote);
    }
}

} // namespace interfaces
