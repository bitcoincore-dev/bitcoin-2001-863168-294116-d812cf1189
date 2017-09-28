// Copyright (c) 2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLETINITINTERFACE_H
#define BITCOIN_WALLETINITINTERFACE_H

#include <string>

class CScheduler;
class CRPCTable;
struct InitInterfaces;

class WalletInitInterface {
public:
    /** Get wallet help string */
    virtual std::string GetHelpString(bool showDebug) const = 0;
    /** Check wallet parameter interaction */
    virtual bool ParameterInteraction() const = 0;
    /** Verify wallets */
    virtual bool Verify() const = 0;
    /** Add wallets that should be opened to list of init interfaces. */
    virtual void Construct(InitInterfaces& interfaces) const = 0;

    virtual ~WalletInitInterface() {}
};

#endif // BITCOIN_WALLETINITINTERFACE_H
