#include <ipc/interfaces.h>

#include <ipc/util.h>
#include <wallet/wallet.h>

namespace ipc {
namespace local {
namespace {

class WalletClientImpl : public Chain::Client
{
public:
    WalletClientImpl(Chain& chain) : m_chain(chain) {}
    Chain& m_chain;
};

} // namespace

std::unique_ptr<Chain::Client> MakeWalletClient(Chain& chain, ChainClientOptions options)
{
    return MakeUnique<WalletClientImpl>(chain);
}

} // namespace local
} // namespace ipc
