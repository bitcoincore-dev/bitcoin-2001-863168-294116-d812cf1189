// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_INIT_H
#define BITCOIN_WALLET_INIT_H

#include <ipc/interfaces.h>
#include <string>

class CRPCTable;
class CScheduler;

//! Return the wallets help message.
std::string GetWalletHelpString(bool showDebug);

//! Wallets parameter interaction
bool WalletParameterInteraction();

//! Register wallet RPCs.
void RegisterWalletRPC(CRPCTable &tableRPC);

//! Responsible for reading and validating the -wallet arguments and verifying the wallet database.
//  This function will perform salvage on the wallet if requested, as long as only one wallet is
//  being loaded (WalletParameterInteraction forbids -salvagewallet, -zapwallettxes or -upgradewallet with multiwallet).
bool VerifyWallets();

//! Create wallet IPC clients.
void MakeWalletClients(ipc::Chain& ipc_chain, ipc::Chain::Clients& ipc_clients);

//! Load wallet databases.
bool OpenWallets(ipc::Chain& ipc_chain, ipc::Chain::Client& ipc_client, const std::vector<std::string>& wallet_filenames);

//! Complete startup of wallets.
void StartWallets(CScheduler& scheduler);

//! Flush all wallets in preparation for shutdown.
void FlushWallets();

//! Stop all wallets. Wallets will be flushed first.
void StopWallets();

//! Close all wallets.
void CloseWallets();

#endif // BITCOIN_WALLET_INIT_H
