// Copyright (c) 2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_FEEBUMPER_H
#define BITCOIN_WALLET_FEEBUMPER_H

#include <primitives/transaction.h>

class CWallet;
class CWalletTx;
class uint256;
class CCoinControl;
enum class FeeEstimateMode;

enum class BumpFeeResult
{
    OK,
    INVALID_ADDRESS_OR_KEY,
    INVALID_REQUEST,
    INVALID_PARAMETER,
    WALLET_ERROR,
    MISC_ERROR,
};

/* return whether transaction can be bumped */
bool TransactionCanBeBumped(CWallet* pWallet, const uint256& txid);

/* create bumpfee transaction */
BumpFeeResult CreateBumpFeeTransaction(const CWallet* pWallet,
    const uint256& txid,
    const CCoinControl& coin_control,
    CAmount totalFee,
    std::vector<std::string>& vErrors,
    CAmount& nOldFee,
    CAmount& nNewFee,
    CMutableTransaction& mtx);

/* signs the new transaction,
 * returns false if the tx couldn't be found or if it was
 * impossible to create the signature(s)
 */
bool SignBumpFeeTransaction(CWallet* pWallet, CMutableTransaction& mtx);

/* commits the fee bump,
 * returns true, in case of CWallet::CommitTransaction was successful
 * but, eventually sets vErrors if the tx could not be added to the mempool (will try later)
 * or if the old transaction could not be marked as replaced
 */
BumpFeeResult CommitBumpFeeTransaction(CWallet* pWallet,
    const uint256& txid,
    CMutableTransaction&& mtx,
    std::vector<std::string>& vErrors,
    uint256& bumpedTxid);

#endif // BITCOIN_WALLET_FEEBUMPER_H
