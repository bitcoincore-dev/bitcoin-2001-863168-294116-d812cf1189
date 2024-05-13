// Copyright (c) 2015-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_ZMQ_ZMQNOTIFICATIONINTERFACE_H
#define BITCOIN_ZMQ_ZMQNOTIFICATIONINTERFACE_H

#include <primitives/transaction.h>
#include <validationinterface.h>

#include <cstdint>
#include <functional>
#include <list>
#include <memory>
#include <boost/signals2/connection.hpp>

class CBlock;
class CBlockIndex;
class CZMQAbstractNotifier;

class CZMQNotificationInterface final : public CValidationInterface
{
public:
    virtual ~CZMQNotificationInterface();

    std::list<const CZMQAbstractNotifier*> GetActiveNotifiers() const;

    static std::unique_ptr<CZMQNotificationInterface> Create(std::function<bool(CBlock&, const CBlockIndex&)> get_block_by_index);

protected:
    bool Initialize();
    void Shutdown();

    void TransactionAddedToWallet(const CTransactionRef& tx, const uint256 &hashBlock);

    // CValidationInterface
    void TransactionAddedToMempool(const CTransactionRef& tx, uint64_t mempool_sequence) override;
    void TransactionRemovedFromMempool(const CTransactionRef& tx, MemPoolRemovalReason reason, uint64_t mempool_sequence) override;
    void BlockConnected(ChainstateRole role, const std::shared_ptr<const CBlock>& pblock, const CBlockIndex* pindexConnected) override;
    void BlockDisconnected(const std::shared_ptr<const CBlock>& pblock, const CBlockIndex* pindexDisconnected) override;
    void UpdatedBlockTip(const CBlockIndex *pindexNew, const CBlockIndex *pindexFork, bool fInitialDownload, const std::shared_ptr<const CBlock>& block) override;

private:
    CZMQNotificationInterface();

    void* pcontext{nullptr};
    std::list<std::unique_ptr<CZMQAbstractNotifier>> notifiers;
    boost::signals2::connection m_wtx_added_connection;
    std::function<bool(CBlock&, const CBlockIndex&)> m_get_block_by_index;
};

extern std::unique_ptr<CZMQNotificationInterface> g_zmq_notification_interface;

#endif // BITCOIN_ZMQ_ZMQNOTIFICATIONINTERFACE_H
