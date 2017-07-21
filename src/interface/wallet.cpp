#include <interface/chain.h>
#include <interface/util.h>
#include <rpc/server.h>
#include <wallet/init.h>
#include <wallet/wallet.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

class CScheduler;

namespace interface {
namespace {

class WalletClientImpl : public Chain::Client
{
public:
    WalletClientImpl(Chain& chain, std::vector<std::string> wallet_filenames)
        : m_chain(chain), m_wallet_filenames(std::move(wallet_filenames))
    {
    }
    void registerRpcs() override { RegisterWalletRPC(::tableRPC); }
    bool prepare() override { return LoadWallets(m_chain, *this, m_wallet_filenames); }
    void start(CScheduler& scheduler) override { StartWallets(scheduler); }
    void stop() override { FlushWallets(); }
    void shutdown() override
    {
        for (CWalletRef wallet : ::vpwallets) {
            wallet->Flush(true /* shutdown */);
        }
    }
    ~WalletClientImpl() override { CloseWallets(); }

    Chain& m_chain;
    std::vector<std::string> m_wallet_filenames;
};

} // namespace

std::unique_ptr<Chain::Client> MakeWalletClient(Chain& chain, std::vector<std::string> wallet_filenames)
{
    return MakeUnique<WalletClientImpl>(chain, std::move(wallet_filenames));
}

} // namespace interface
