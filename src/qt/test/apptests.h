#ifndef BITCOIN_QT_TEST_APPTESTS_H
#define BITCOIN_QT_TEST_APPTESTS_H

#include <QObject>

class BitcoinApplication;
class BitcoinGUI;
class RPCConsole;

class AppTests : public QObject
{
    Q_OBJECT
public:
    AppTests(BitcoinApplication& app) : m_app(app) {}

private Q_SLOTS:
    void appTests();
    void guiTests(BitcoinGUI* window);
    void consoleTests(RPCConsole* console);

private:
    //! Increment number of pending callbacks.
    void expectCallback() { ++m_callbacks; }

    //! RAII helper to decrement number of pending callbacks.
    struct HandleCallback
    {
        AppTests& m_app_tests;
        ~HandleCallback();
    };

    //! Bitcoin application.
    BitcoinApplication& m_app;

    //! Number of expected callbacks pending.
    int m_callbacks = 0;
};

#endif // BITCOIN_QT_TEST_APPTESTS_H
