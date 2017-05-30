#include <interfaces/chain.h>

#include <util.h>

namespace interfaces {
namespace {

class ChainImpl : public Chain
{
};

} // namespace

std::unique_ptr<Chain> MakeChain() { return MakeUnique<ChainImpl>(); }

} // namespace interfaces
