#include <ipc/interfaces.h>

#include <ipc/util.h>
#include <wallet/init.h>
#include <wallet/wallet.h>

namespace ipc {
namespace local {
namespace {

class WalletClientImpl : public Chain::Client
{
public:
    WalletClientImpl(Chain& chain, std::vector<std::string> wallet_filenames)
        : m_chain(chain), m_wallet_filenames(std::move(wallet_filenames))
    {
    }
    void registerRpcs() override { RegisterWalletRPCCommands(m_chain, m_rpc_handlers); }
    bool prepare() override { return InitLoadWallet(m_chain, *this, m_wallet_filenames); }
    void start(CScheduler& scheduler) override
    {
        for (CWalletRef wallet : ::vpwallets) {
            wallet->postInitProcess(scheduler);
        }
    }
    void stop() override
    {
        for (CWalletRef wallet : ::vpwallets) {
            wallet->Flush(false /* shutdown */);
        }
    }
    void shutdown() override
    {
        for (CWalletRef wallet : ::vpwallets) {
            wallet->Flush(true /* shutdown */);
        }
    }
    ~WalletClientImpl()
    {
        for (CWalletRef wallet : ::vpwallets) {
            delete wallet;
        }
        ::vpwallets.clear();
    }

    Chain& m_chain;
    std::vector<std::string> m_wallet_filenames;
    std::vector<std::unique_ptr<Handler>> m_rpc_handlers;
};

} // namespace

std::unique_ptr<Chain::Client> MakeWalletClient(Chain& chain, ChainClientOptions options)
{
    return MakeUnique<WalletClientImpl>(chain, std::move(options.wallet_filenames));
}

} // namespace local
} // namespace ipc
