#include <init.h>
#include <interface/chain.h>
#include <rpc/server.h>
#include <util.h>
#include <wallet/wallet.h>
#include <wallet/walletutil.h>

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
    void registerRpcs() override { RegisterWalletRPCCommands(::tableRPC); }
    bool prepare() override
    {
        for (const std::string& filename : m_wallet_filenames) {
            CWalletRef wallet =
                CWallet::CreateWalletFromFile(m_chain, filename, fs::absolute(filename, GetWalletDir()));
            if (!wallet) return false;
            ::vpwallets.push_back(wallet);
        }
        return true;
    }
    void start(CScheduler& scheduler) override
    {
        for (CWalletRef wallet : ::vpwallets) {
            wallet->postInitProcess(scheduler);
        }
    }
    void stop() override
    {
        for (CWalletRef wallet : ::vpwallets) {
            wallet->Flush(false);
        }
    }
    void shutdown() override
    {
        for (CWalletRef wallet : ::vpwallets) {
            wallet->Flush(true);
        }
    }
    ~WalletClientImpl() override
    {
        for (CWalletRef wallet : ::vpwallets) {
            delete wallet;
        }
        ::vpwallets.clear();
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
