// Copyright (c) 2016-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TEST_RPCNESTEDTESTS_H
#define BITCOIN_QT_TEST_RPCNESTEDTESTS_H

#include <QObject>
#include <QTest>

#include <txdb.h>
#include <txmempool.h>

namespace interfaces {
class Init;
} // namespace interfaces

class RPCNestedTests : public QObject
{
public:
    RPCNestedTests(interfaces::Init& init) : m_init(init) {}
    interfaces::Init& m_init;

    Q_OBJECT

    private Q_SLOTS:
    void rpcNestedTests();
};

#endif // BITCOIN_QT_TEST_RPCNESTEDTESTS_H
