#ifndef BITCOIN_QT_TEST_WALLETTESTS_H
#define BITCOIN_QT_TEST_WALLETTESTS_H

#include <QObject>
#include <QTest>

namespace interfaces {
class Init;
} // namespace interfaces

class WalletTests : public QObject
{
 public:
    WalletTests(interfaces::Init& init) : m_init(init) {}
    interfaces::Init& m_init;

    Q_OBJECT

private Q_SLOTS:
    void walletTests();
};

#endif // BITCOIN_QT_TEST_WALLETTESTS_H
