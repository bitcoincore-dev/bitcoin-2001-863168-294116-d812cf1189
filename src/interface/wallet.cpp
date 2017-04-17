#include <interface/wallet.h>

#include <interface/handler.h>
#include <rpc/server.h>
#include <util.h>
#include <wallet/init.h>
#include <wallet/rpcwallet.h>
#include <wallet/wallet.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace interface {
namespace {

class WalletImpl : public Wallet
{
public:
    WalletImpl(CWallet& wallet) : m_wallet(wallet) {}

    std::unique_ptr<Handler> handleShowProgress(ShowProgressFn fn) override
    {
        return MakeHandler(m_wallet.ShowProgress.connect(fn));
    }

    CWallet& m_wallet;
};

} // namespace

std::unique_ptr<Wallet> MakeWallet(CWallet& wallet) { return MakeUnique<WalletImpl>(wallet); }

} // namespace interface
