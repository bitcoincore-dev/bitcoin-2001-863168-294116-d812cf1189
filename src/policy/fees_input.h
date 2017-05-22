// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_POLICY_FEES_INPUT_H
#define BITCOIN_POLICY_FEES_INPUT_H

#include "amount.h"
#include "fs.h"

#include <map>
#include <memory>
#include <vector>

class CAutoFile;
class CBlockPolicyEstimator;
class CTxMemPool;
class CTxMemPoolEntry;
class UniValue;
class uint256;

class CBlockPolicyInput
{
public:
    CBlockPolicyInput(CBlockPolicyEstimator& estimator);

    /** Process a transaction accepted to the mempool*/
    void processTx(const CTxMemPoolEntry& entry, bool validFeeEstimate);

    /** Remove a transaction from the mempool tracking stats*/
    void removeTx(const uint256& hash, bool inBlock);

    /** Process all the transactions that have been included in a block */
    void processBlock(unsigned int nBlockHeight, std::vector<const CTxMemPoolEntry*>& entries);

    /** Empty mempool transactions on shutdown to record failure to confirm for txs still in mempool */
    void flushUnconfirmed(CTxMemPool& pool);

    /** Write estimation data to a file */
    bool writeData(const fs::path& filename);

    /** Read estimation data from a file */
    bool readData(const fs::path& filename);

    /** Write incoming block and transaction events to log file. */
    bool writeLog(const std::string& filename);

    /** Read block and transaction events from log file. */
    bool readLog(const std::string& filename, std::function<bool(UniValue&)> filter);

private:
    CBlockPolicyEstimator& estimator;
    std::unique_ptr<std::basic_ostream<char>> log;
};

#endif /*BITCOIN_POLICY_FEES_INPUT_H */
