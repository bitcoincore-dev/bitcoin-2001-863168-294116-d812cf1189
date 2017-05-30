#ifndef BITCOIN_INTERFACE_CHAIN_H
#define BITCOIN_INTERFACE_CHAIN_H

#include <memory>
#include <string>
#include <vector>

namespace interface {

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
};

//! Return implementation of Chain interface.
using MakeChainFn = std::unique_ptr<Chain>();
MakeChainFn MakeChain;

//! Return implementation of Chain::Client interface for a wallet client. This
//! function will be undefined in builds where ENABLE_WALLET is false.
//!
//! Currently, wallets are the only chain clients. But in the future, other
//! types of chain clients could be added, such as tools for monitoring,
//! analysis, or fee estimation. These clients need to expose their own
//! MakeXXXClient functions returning their implementations of the Chain::Client
//! interface.
using MakeWalletClientFn = std::unique_ptr<Chain::Client>(Chain& chain, std::vector<std::string> wallet_filenames);
MakeWalletClientFn MakeWalletClient;

} // namespace interface

#endif // BITCOIN_INTERFACE_CHAIN_H
