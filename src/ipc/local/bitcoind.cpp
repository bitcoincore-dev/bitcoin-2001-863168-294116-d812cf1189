#include <ipc/interfaces.h>

#include <ipc/util.h>

namespace ipc {
namespace local {
namespace {

class ChainImpl : public Chain
{
};

} // namespace

std::unique_ptr<Chain> MakeChain() { return MakeUnique<ChainImpl>(); }

} // namespace local
} // namespace ipc
