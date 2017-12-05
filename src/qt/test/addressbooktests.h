#ifndef BITCOIN_QT_TEST_ADDRESSBOOKTESTS_H
#define BITCOIN_QT_TEST_ADDRESSBOOKTESTS_H

#include <QObject>
#include <QTest>

namespace interfaces {
class Init;
} // namespace interfaces

class AddressBookTests : public QObject
{
public:
    AddressBookTests(interfaces::Init& init) : m_init(init) {}
    interfaces::Init& m_init;

    Q_OBJECT

private Q_SLOTS:
    void addressBookTests();
};

#endif // BITCOIN_QT_TEST_ADDRESSBOOKTESTS_H
