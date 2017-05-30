#include <interface/chain.h>
#include <util.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace interface {
namespace {

class WalletClientImpl : public Chain::Client
{
public:
    WalletClientImpl(Chain& chain, std::vector<std::string> wallet_filenames)
        : m_chain(chain), m_wallet_filenames(std::move(wallet_filenames))
    {
    }

    Chain& m_chain;
    std::vector<std::string> m_wallet_filenames;
};

} // namespace

std::unique_ptr<Chain::Client> MakeWalletClient(Chain& chain, std::vector<std::string> wallet_filenames)
{
    return MakeUnique<WalletClientImpl>(chain, std::move(wallet_filenames));
}

} // namespace interface
