#ifndef BITCOIN_INTERFACE_CHAIN_H
#define BITCOIN_INTERFACE_CHAIN_H

#include <amount.h>                 // For CAmount
#include <optional.h>               // For Optional and nullopt
#include <policy/feerate.h>         // For CFeeRate
#include <policy/rbf.h>             // For RBFTransactionState
#include <primitives/transaction.h> // For CTransactionRef

#include <memory>
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

class CBlock;
class CCoinControl;
class CScheduler;
class CValidationState;
class uint256;
struct CBlockLocator;
struct FeeCalculation;

namespace interface {

//! Interface for giving wallet processes access to blockchain state.
class Chain
{
public:
    virtual ~Chain() {}

    //! Interface for querying locked chain state, used by legacy code that
    //! assumes state won't change between calls. New code should avoid using
    //! the Lock interface and instead call higher-level Chain methods
    //! that return more information so the chain doesn't need to stay locked
    //! between calls.
    class Lock
    {
    public:
        virtual ~Lock() {}

        //! Get current chain height, not including genesis block (returns 0 if
        //! chain only contains genesis block, nothing if chain does not contain
        //! any blocks).
        virtual Optional<int> getHeight() = 0;

        //! Get block height above genesis block. Returns 0 for genesis block,
        //! for following block, and so on. Returns nothing for a block not
        //! included in the current chain.
        virtual Optional<int> getBlockHeight(const uint256& hash) = 0;

        //! Get block depth. Returns 1 for chain tip, 2 for preceding block, and
        //! so on. Returns 0 for a block not included in the current chain.
        virtual int getBlockDepth(const uint256& hash) = 0;

        //! Get block hash.
        virtual uint256 getBlockHash(int height) = 0;

        //! Get block time.
        virtual int64_t getBlockTime(int height) = 0;

        //! Get max time of block and all ancestors.
        virtual int64_t getBlockTimeMax(int height) = 0;

        //! Get block median time past.
        virtual int64_t getBlockMedianTimePast(int height) = 0;

        //! Check if block is empty.
        virtual bool blockHasTransactions(int height) = 0;

        //! Read block from disk.
        virtual bool readBlockFromDisk(int height, CBlock& block) = 0;

        //! Estimate fraction of total transactions verified if blocks up to
        //! given height are verified.
        virtual double guessVerificationProgress(int height) = 0;

        //! Return height of earliest block in chain with timestamp equal or
        //! greater than the given time, or nothing if there is no block with a
        //! high enough timestamp.
        virtual Optional<int> findEarliestAtLeast(int64_t time) = 0;

        //! Return height of last block in chain with timestamp less than the
        //! given, and height less than or equal to the given, or nothing if
        //! there is no such block.
        virtual Optional<int> findLastBefore(int64_t time, int start_height) = 0;

        //! Return height of last block in the specified range which is pruned, or
        //! nothing if no block in the range is pruned. Range is inclusive.
        virtual Optional<int> findPruned(int start_height = 0, Optional<int> stop_height = nullopt) = 0;

        //! Return height of the highest block on the chain that is an ancestor
        //! of the specified block. Also return the height of the specified
        //! block as an optinal output parameter.
        virtual Optional<int> findFork(const uint256& hash, Optional<int>* height) = 0;

        //! Return true if block hash points to the current chain tip, or to a
        //! possible descendant of the current chain tip that isn't currently
        //! connected.
        virtual bool isPotentialTip(const uint256& hash) = 0;

        //! Get locator for the current chain tip.
        virtual CBlockLocator getLocator() = 0;

        //! Return height of block on the chain using locator.
        virtual Optional<int> findLocatorFork(const CBlockLocator& locator) = 0;

        //! Check if transaction will be final given chain height current time.
        virtual bool checkFinalTx(const CTransaction& tx) = 0;

        //! Check whether segregated witness is enabled on the network.
        virtual bool isWitnessEnabled() = 0;

        //! Add transaction to memory pool.
        virtual bool acceptToMemoryPool(CTransactionRef tx, CValidationState& state) = 0;
    };

    //! Return Lock interface. Chain is locked when this is called, and
    //! unlocked when the returned interface is freed.
    virtual std::unique_ptr<Lock> lock(bool try_lock = false) = 0;

    //! Return Lock interface assuming chain is already locked. This
    //! method is temporary and is only used in a few places to avoid changing
    //! behavior while code is transitioned to use the LockState interface.
    virtual std::unique_ptr<Lock> assumeLocked() = 0;

    //! Return whether node has the block and optionally return block metadata or contents.
    virtual bool findBlock(const uint256& hash, CBlock* block = nullptr, int64_t* time = nullptr) = 0;

    //! Get virtual transaction size.
    virtual int64_t getVirtualTransactionSize(const CTransaction& tx) = 0;

    //! Check if transaction is RBF opt in.
    virtual RBFTransactionState isRBFOptIn(const CTransaction& tx) = 0;

    //! Check if transaction has descendants in mempool.
    virtual bool hasDescendantsInMempool(const uint256& txid) = 0;

    //! Relay transaction.
    virtual bool relayTransaction(const uint256& txid) = 0;

    //! Check if transaction is within chain limit.
    virtual bool transactionWithinChainLimit(const uint256& txid, size_t chain_limit) = 0;

    //! Check chain limits.
    virtual bool checkChainLimits(CTransactionRef tx) = 0;

    //! Get min pool feerate.
    virtual CFeeRate getMinPoolFeeRate() = 0;

    //! Get min relay feerate (-minrelaytxfee / -incrementalrelayfee).
    virtual CFeeRate getMinRelayFeeRate() = 0;

    //! Get incremental relay feerate (-incrementalrelayfee).
    virtual CFeeRate getIncrementalRelayFeeRate() = 0;

    //! Get dust relay feerate (-dustrelayfee).
    virtual CFeeRate getDustRelayFeeRate() = 0;

    //! Get max discard feerate (-discardfee).
    virtual CFeeRate getMaxDiscardFeeRate() = 0;

    //! Get max tx fee (-maxtxfee).
    virtual CAmount getMaxTxFee() = 0;

    //! Get min tx fee.
    virtual CAmount getMinTxFee(unsigned int tx_bytes, const CCoinControl& coin_control, FeeCalculation* calc) = 0;

    //! Get required tx fee (-mintxfee / -minrelaytxfee / -incrementalrelayfee)
    virtual CAmount getRequiredTxFee(unsigned int tx_bytes) = 0;

    //! Check if pruning is enabled.
    virtual bool getPruneMode() = 0;

    //! Check if p2p enabled.
    virtual bool p2pEnabled() = 0;

    //! Get adjusted time.
    virtual int64_t getAdjustedTime() = 0;

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
