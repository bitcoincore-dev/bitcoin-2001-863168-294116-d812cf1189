// Copyright (c) 2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef WALLETINITINTERFACE_H
#define WALLETINITINTERFACE_H

#include <string>

class CScheduler;
class CRPCTable;
class InitInterfaces;

class WalletInitInterface {
public:
    /** Get wallet help string */
    virtual std::string GetHelpString(bool showDebug) = 0;
    /** Check wallet parameter interaction */
    virtual bool ParameterInteraction() = 0;
    /** Verify wallets */
    virtual bool Verify() = 0;
    /** Construct wallets*/
    virtual void Construct(InitInterfaces& interfaces) = 0;

    virtual ~WalletInitInterface() {}
};

class DummyWalletInit : public WalletInitInterface {
public:

    std::string GetHelpString(bool showDebug) override {return std::string{};}
    bool ParameterInteraction() override {return true;}
    bool Verify() override {return true;}
    void Construct(InitInterfaces& interfaces) override {}
};

#endif // WALLETINITINTERFACE_H
