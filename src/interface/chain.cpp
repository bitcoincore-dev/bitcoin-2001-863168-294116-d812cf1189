#include <interface/chain.h>

#include <util.h>

namespace interface {
namespace {

class ChainImpl : public Chain
{
};

} // namespace

std::unique_ptr<Chain> MakeChain() { return MakeUnique<ChainImpl>(); }

} // namespace interface
