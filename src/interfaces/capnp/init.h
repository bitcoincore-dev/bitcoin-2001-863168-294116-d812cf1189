#ifndef BITCOIN_INTERFACES_CAPNP_INIT_H
#define BITCOIN_INTERFACES_CAPNP_INIT_H

#include <interfaces/capnp/init.capnp.h>
#include <interfaces/capnp/proxy.h>
#include <interfaces/init.h>

namespace interfaces {
namespace capnp {

//! Specialization of makeWalletClient needed because it takes a Chain& reference
//! argument, not a unique_ptr<Chain> argument, so a manual addCloseHook()
//! callback is needed to clean up the ProxyServer<messages::Chain> proxy object.
template <>
struct ProxyServerMethodTraits<messages::Init::MakeWalletClientParams>
{
    using Context =
        ServerContext<messages::Init, messages::Init::MakeWalletClientParams, messages::Init::MakeWalletClientResults>;
    static std::unique_ptr<ChainClient> invoke(Context& context, std::vector<std::string> wallet_filenames);
};

} // namespace capnp
} // namespace interfaces

#endif // BITCOIN_INTERFACES_CAPNP_INIT_H
