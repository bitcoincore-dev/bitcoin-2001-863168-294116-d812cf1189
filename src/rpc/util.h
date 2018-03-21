// Copyright (c) 2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_RPC_UTIL_H
#define BITCOIN_RPC_UTIL_H

#include <string>
#include <vector>

class CKey;
class CKeyStore;
class CPubKey;
class CScript;

CPubKey HexToPubKey(const std::string& hex_in);
CPubKey AddrToPubKey(CKeyStore* const keystore, const std::string& addr_in);
void ParseWIFPrivKey(const std::string wif_secret, CKey&, CPubKey*);
CScript CreateMultisigRedeemscript(const int required, const std::vector<CPubKey>& pubkeys, bool sort);

#endif // BITCOIN_RPC_UTIL_H
