// Copyright (c) 2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_NODE_NODE_H
#define BITCOIN_NODE_NODE_H

#include <list>
#include <memory>

class BanMan;
class CConnman;
class PeerLogicValidation;
namespace interfaces {
class Chain;
class ChainClient;
} // namespace interfaces
using ChainClients = std::list<std::reference_wrapper<interfaces::ChainClient>>;

//! Node struct containing chain state and connection state.
//!
//! More state could be moved into this struct to eliminate global variables,
//! and allow creating multiple instances of validation and networking objects
//! and linking outside of bitcoind for simulation or testing.
struct Node
{
    std::unique_ptr<CConnman> connman;
    std::unique_ptr<PeerLogicValidation> peer_logic;
    std::unique_ptr<BanMan> banman;
    std::unique_ptr<interfaces::Chain> chain;
    std::unique_ptr<interfaces::ChainClient> wallet_client;
    ChainClients chain_clients;

    //! Declare default constructor and destructor that are not inline, so code
    //! instantiating the Node struct doesn't need to #include CConnman, BanMan,
    //! etc class declarations.
    Node();
    ~Node();
};

#endif // BITCOIN_NODE_NODE_H
