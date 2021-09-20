// Copyright (c) 2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_INIT_CHAINSTATE_H
#define BITCOIN_INIT_CHAINSTATE_H

#include <cstdint> // for int64_t

class ArgsManager;
struct bilingual_str;
class CChainParams;
class CClientUIInterface;
class ChainstateManager;
struct NodeContext;

bool LoadChainstateSequence(bool& fLoaded,
                            bilingual_str& strLoadError,
                            bool fReset,
                            CClientUIInterface& uiInterface,
                            ChainstateManager& chainman,
                            NodeContext& node,
                            bool fPruneMode,
                            const CChainParams& chainparams,
                            const ArgsManager& args,
                            bool fReindexChainState,
                            int64_t nBlockTreeDBCache,
                            int64_t nCoinDBCache,
                            int64_t nCoinCacheUsage);
#endif // BITCOIN_INIT_CHAINSTATE_H
