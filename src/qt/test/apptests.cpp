#include "apptests.h"

#include "chainparams.h"
#include "init.h"
#include "qt/bitcoin.h"
#include "qt/bitcoingui.h"
#include "qt/networkstyle.h"
#include "qt/rpcconsole.h"
#include "validation.h"

#if defined(HAVE_CONFIG_H)
#include "config/bitcoin-config.h"
#endif
#ifdef ENABLE_WALLET
#include "wallet/db.h"
#endif

#include <QAction>
#include <QEventLoop>
#include <QLineEdit>
#include <QScopedPointer>
#include <QTest>
#include <QTextEdit>
#include <QtGlobal>
#if QT_VERSION >= 0x050000
#include <QtTest/QtTestWidgets>
#endif
#include <QtTest/QtTestGui>
#include <new>
#include <string>
#include <univalue.h>

namespace {
//! Call getblockchaininfo RPC and check first field of JSON output.
void TestRpcCommand(RPCConsole* console)
{
    QEventLoop loop;
    QTextEdit* messagesWidget = console->findChild<QTextEdit*>("messagesWidget");
    QObject::connect(messagesWidget, SIGNAL(textChanged()), &loop, SLOT(quit()));
    QLineEdit* lineEdit = console->findChild<QLineEdit*>("lineEdit");
    QTest::keyClicks(lineEdit, "getblockchaininfo");
    QTest::keyClick(lineEdit, Qt::Key_Return);
    loop.exec();
    QString output = messagesWidget->toPlainText();
    UniValue value;
    value.read(output.right(output.size() - output.lastIndexOf(QChar::ObjectReplacementCharacter) - 1).toStdString());
    QCOMPARE(value["chain"].get_str(), std::string("regtest"));
}
} // namespace

//! Entry point for BitcoinApplication tests.
void AppTests::appTests()
{
    m_app.parameterSetup();
    m_app.createOptionsModel(true /* reset settings */);
    QScopedPointer<const NetworkStyle> style(
        NetworkStyle::instantiate(QString::fromStdString(Params().NetworkIDString())));
    m_app.createWindow(style.data());
    BitcoinCore::baseInitialize();
    connect(&m_app, SIGNAL(windowShown(BitcoinGUI*)), this, SLOT(guiTests(BitcoinGUI*)));
    expectCallback("guiTests");
    m_app.requestInitialize();
    m_app.exec();

    // Reset some global state to avoid interfering with later tests.
    ResetShutdownRequested();
#ifdef ENABLE_WALLET
    ::bitdb.Close();
    ::bitdb.Reset();
#endif
    UnloadBlockIndex();
}

//! Entry point for BitcoinGUI tests.
void AppTests::guiTests(BitcoinGUI* window)
{
    HandleCallback callback{"guiTests", *this};
    connect(window, SIGNAL(consoleShown(RPCConsole*)), this, SLOT(consoleTests(RPCConsole*)));
    expectCallback("consoleTests");
    QAction* action = window->findChild<QAction*>("openRPCConsoleAction");
    action->activate(QAction::Trigger);
}

//! Entry point for RPCConsole tests.
void AppTests::consoleTests(RPCConsole* console)
{
    HandleCallback callback{"consoleTests", *this};
    TestRpcCommand(console);
}

//! Destructor to shut down after the last expected callback completes.
AppTests::HandleCallback::~HandleCallback()
{
    auto& callbacks = m_app_tests.m_callbacks;
    auto it = callbacks.find(m_callback);
    assert(it != callbacks.end());
    callbacks.erase(it);
    if (callbacks.empty()) {
        m_app_tests.m_app.requestShutdown();
    }
}
