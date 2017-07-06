// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_INIT_H
#define BITCOIN_WALLET_INIT_H

#include <string>

class CRPCTable;
class CScheduler;

class WalletInit {
public:

    //! Return the wallets help message.
    static std::string GetWalletHelpString(bool showDebug);

    //! Wallets parameter interaction
    static bool WalletParameterInteraction();

    //! Register wallet RPCs.
    static void RegisterWalletRPC(CRPCTable &tableRPC);

    //! Responsible for reading and validating the -wallet arguments and verifying the wallet database.
    static bool VerifyWallets();

    //! Load wallet databases.
    static bool OpenWallets();

    //! Complete startup of wallets.
    static void StartWallets(CScheduler& scheduler);

    //! Flush all wallets in preparation for shutdown.
    static void FlushWallets();

    //! Stop all wallets. Wallets will be flushed first.
    static void StopWallets();

    //! Close all wallets.
    static void CloseWallets();
};

#endif // BITCOIN_WALLET_INIT_H
