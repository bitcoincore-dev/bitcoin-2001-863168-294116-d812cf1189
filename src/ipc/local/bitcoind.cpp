#include <ipc/interfaces.h>

#include <ipc/util.h>
#include <validation.h>

namespace ipc {
namespace local {
namespace {

class LockedStateImpl : public Chain::LockedState
{
};

class LockingStateImpl : public LockedStateImpl, public CCriticalBlock
{
    using CCriticalBlock::CCriticalBlock;
};

class ChainImpl : public Chain
{
public:
    std::unique_ptr<Chain::LockedState> lockState(bool try_lock) override
    {
        auto result = MakeUnique<LockingStateImpl>(::cs_main, "cs_main", __FILE__, __LINE__, try_lock);
        if (try_lock && result && !*result) return {};
        return result;
    }
    std::unique_ptr<Chain::LockedState> assumeLocked() override { return MakeUnique<LockedStateImpl>(); }
};

} // namespace

std::unique_ptr<Chain> MakeChain() { return MakeUnique<ChainImpl>(); }

} // namespace local
} // namespace ipc
