// Copyright (c) 2016-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/test/wallet_test_fixture.h>

#include <rpc/server.h>
#include <wallet/db.h>

WalletTestingSetup::WalletTestingSetup(const std::string& chainName):
    TestingSetup(chainName), m_wallet(m_chain.get(), "mock", CWalletDBWrapper::CreateMock())
{
    bool fFirstRun;
    m_wallet.LoadWallet(fFirstRun);
    m_wallet.m_handler = m_chain->handleNotifications(m_wallet);

    m_chain_client->registerRpcs();
}

WalletTestingSetup::~WalletTestingSetup()
{
    m_wallet.m_handler->disconnect();
}
