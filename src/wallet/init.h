// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_INIT_H
#define BITCOIN_WALLET_INIT_H

#include <interface/chain.h>
#include <walletinitinterface.h>
#include <string>

class CRPCTable;
class CScheduler;
struct InitInterfaces;

class WalletInit : public WalletInitInterface {
public:

    //! Return the wallets help message.
    std::string GetHelpString(bool showDebug) override;

    //! Wallets parameter interaction
    bool ParameterInteraction() override;

    //! Responsible for reading and validating the -wallet arguments and verifying the wallet database.
    //  This function will perform salvage on the wallet if requested, as long as only one wallet is
    //  being loaded (WalletParameterInteraction forbids -salvagewallet, -zapwallettxes or -upgradewallet with multiwallet).
    bool Verify() override;

    //! Add wallets that should be opened to list of init interfaces.
    void Construct(InitInterfaces& interfaces) override;
};

#endif // BITCOIN_WALLET_INIT_H
