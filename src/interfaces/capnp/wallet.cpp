#include <interfaces/capnp/wallet.h>

#include <interfaces/capnp/wallet.capnp.proxy.h>

namespace interfaces {
namespace capnp {

const CTransaction& ProxyClientCustom<messages::PendingWalletTx, PendingWalletTx>::get()
{
    if (!m_tx) {
        m_tx = self().customGet();
    }
    return *m_tx;
}

} // namespace capnp
} // namespace interfaces
