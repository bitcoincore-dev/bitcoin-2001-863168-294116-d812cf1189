#ifndef BITCOIN_INTERFACE_CHAIN_H
#define BITCOIN_INTERFACE_CHAIN_H

#include <memory>
#include <string>
#include <vector>

class CScheduler;

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

        //! Register rpcs.
        virtual void registerRpcs() = 0;

        //! Prepare for execution, loading any needed state.
        virtual bool prepare() = 0;

        //! Start client execution and provide a scheduler. (Scheduler is
        //! ignored if client is out-of-process).
        virtual void start(CScheduler& scheduler) = 0;

        //! Stop client execution and prepare for shutdown.
        virtual void stop() = 0;

        //! Shut down client.
        virtual void shutdown() = 0;
    };
};

//! Return implementation of Chain interface.
std::unique_ptr<Chain> MakeChain();

//! Return implementation of Chain::Client interface for a wallet client. This
//! function will be undefined in builds where ENABLE_WALLET is false.
//!
//! Currently, wallets are the only chain clients. But in the future, other
//! types of chain clients could be added, such as tools for monitoring,
//! analysis, or fee estimation. These clients need to expose their own
//! MakeXXXClient functions returning their implementations of the Chain::Client
//! interface.
std::unique_ptr<Chain::Client> MakeWalletClient(Chain& chain, std::vector<std::string> wallet_filenames);

} // namespace interface

#endif // BITCOIN_INTERFACE_CHAIN_H
