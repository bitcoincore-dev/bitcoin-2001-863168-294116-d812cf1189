#ifndef BITCOIN_IPC_INTERFACES_H
#define BITCOIN_IPC_INTERFACES_H

#include <memory>
#include <vector>

namespace ipc {

//! Interface for giving wallet processes access to blockchain state.
class Chain
{
public:
    virtual ~Chain() {}

    //! Interface to let node manage chain clients (wallets, or maybe tools for
    //! monitoring and analysis in the future).
    class Client
    {
    public:
        virtual ~Client() {}
    };

    //! List of clients.
    using Clients = std::vector<std::unique_ptr<Client>>;
};

//! Protocol IPC interface should use to communicate with implementation.
enum Protocol {
    LOCAL, //!< Call functions linked into current executable.
};

//! Create IPC chain interface, communicating with requested protocol. Returns
//! null if protocol isn't implemented or is not available in the current build
//! configuration.
std::unique_ptr<Chain> MakeChain(Protocol protocol);

//! Chain client creation options.
struct ChainClientOptions
{
    //! Type of IPC chain client. Currently wallet processes are the only
    //! clients. In the future other types of client processes could be added
    //! (tools for monitoring, analysis, fee estimation, etc).
    enum Type { WALLET = 0 };
    Type type;
};

//! Create chain client interface, communicating with requested protocol.
//! Returns null if protocol or client type aren't implemented or available in
//! the current build configuration.
std::unique_ptr<Chain::Client> MakeChainClient(Protocol protocol, Chain& chain, ChainClientOptions options);

} // namespace ipc

#endif // BITCOIN_IPC_INTERFACES_H
