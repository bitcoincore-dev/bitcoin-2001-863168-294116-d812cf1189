// Copyright (c) 2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_NODE_NODE_H
#define BITCOIN_NODE_NODE_H

#include <memory>
#include <vector>

class BanMan;
class CConnman;

namespace interfaces {
class Chain;
class ChainClient;
} // namespace interfaces

//! Pointers to interfaces used during init and destroyed on shutdown.
struct Node
{
    Node();
    ~Node();

    std::unique_ptr<interfaces::Chain> chain;
    std::vector<std::unique_ptr<interfaces::ChainClient>> chain_clients;
    std::unique_ptr<CConnman> connman;
    std::unique_ptr<BanMan> banman;
};

#endif // BITCOIN_NODE_NODE_H
