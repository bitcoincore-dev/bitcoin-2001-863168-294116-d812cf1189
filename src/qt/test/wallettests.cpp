#include "wallettests.h"

#include "qt/optionsmodel.h"
#include "qt/platformstyle.h"
#include "qt/sendcoinsdialog.h"
#include "qt/sendcoinsentry.h"
#include "qt/transactiontablemodel.h"
#include "qt/transactionview.h"
#include "qt/walletmodel.h"
#include "test/test_bitcoin.h"
#include "validation.h"
#include "wallet/wallet.h"
#include "qt/bitcoinamountfield.h"
#include "qt/qvalidatedlineedit.h"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QDebug>

namespace
{

void ConfirmMessage(QString* text = nullptr)
{
    QTimer::singleShot(0, Qt::PreciseTimer, [&]() {
        for (QWidget* widget : QApplication::topLevelWidgets()) {
            if (widget->inherits("QMessageBox")) {
                QMessageBox* messageBox = qobject_cast<QMessageBox*>(widget);
                if (text) *text = messageBox->text();
                messageBox->defaultButton()->click();
            }
        }
    });
}

void ConfirmSend(QString* text = nullptr, bool cancel = false)
{
    QTimer::singleShot(0, Qt::PreciseTimer, [&]() {
            for (QWidget* widget : QApplication::topLevelWidgets()) {
                if (widget->inherits("SendConfirmationDialog")) {
                    SendConfirmationDialog* dialog = qobject_cast<SendConfirmationDialog*>(widget);
                    if (text) *text = dialog->text();
                    QAbstractButton* button = dialog->button(cancel ? QMessageBox::Cancel : QMessageBox::Yes);
                    button->setEnabled(true);
                    button->click();
                }
            } });
}

uint256 SendCoins(CWallet& wallet, SendCoinsDialog& sendCoinsDialog, const CBitcoinAddress& address, CAmount amount)
{
    uint256 txid;
    auto notify = [&txid](CWallet*, const uint256& hash, ChangeType status) {
        if (status == CT_NEW) txid = hash;
    };
    QVBoxLayout* entries = sendCoinsDialog.findChild<QVBoxLayout*>("entries");
    SendCoinsEntry* entry = qobject_cast<SendCoinsEntry*>(entries->itemAt(0)->widget());
    entry->findChild<QValidatedLineEdit*>("payTo")->setText(QString::fromStdString(address.ToString()));
    entry->findChild<BitcoinAmountField*>("payAmount")->setValue(5 * COIN);
    boost::signals2::scoped_connection c = wallet.NotifyTransactionChanged.connect(notify);
    ConfirmSend();
    QMetaObject::invokeMethod(&sendCoinsDialog, "on_sendButton_clicked");
    return txid;
}

QModelIndex FindTx(const QAbstractItemModel& model, const uint256& txid)
{
    QString hash = QString::fromStdString(txid.ToString());
    int rows = model.rowCount({});
    for (int row = 0; row < rows; ++row) {
        QModelIndex index = model.index(row, 0, {});
        if (model.data(index, TransactionTableModel::TxHashRole) == hash) {
            return index;
        }
    }
    return {};
}

void BumpFee(TransactionView& view, const uint256& txid, bool expectDisabled, std::string expectError, bool cancel)
{
    QTableView* table = view.findChild<QTableView*>("transactionView");
    QModelIndex index = FindTx(*table->selectionModel()->model(), txid);
    QVERIFY2(index.isValid(), "Could not find BumpFee txid");

    // Select row in table, invoke context menu, and make sure bumpfee action is
    // enabled or disabled as expected.
    QAction* action = view.findChild<QAction*>("bumpFeeAction");
    table->selectionModel()->select(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    action->setEnabled(expectDisabled);
    table->customContextMenuRequested({});
    QCOMPARE(action->isEnabled(), !expectDisabled);

    action->setEnabled(true);
    QString text;
    if (expectError.empty()) {
        ConfirmSend(&text, cancel);
    } else {
        ConfirmMessage(&text);
    }
    action->trigger();
    printf("FIXME compare '%s'\n", text.toStdString().c_str());
}

}

void WalletTests::walletTests()
{
    // Set up wallet and chain with 105 blocks (5 mature blocks for spending).
    TestChain100Setup test;
    for (int i = 0; i < 5; ++i) {
        test.CreateAndProcessBlock({}, GetScriptForRawPubKey(test.coinbaseKey.GetPubKey()));
    }
    bitdb.MakeMock();
    CWallet wallet("wallet_test.dat");
    bool firstRun;
    wallet.LoadWallet(firstRun);
    wallet.SetAddressBook(test.coinbaseKey.GetPubKey().GetID(), "", "receive");
    wallet.AddKeyPubKey(test.coinbaseKey, test.coinbaseKey.GetPubKey());
    wallet.ScanForWalletTransactions(chainActive.Genesis(), true);
    wallet.SetBroadcastTransactions(true);

    // Create widgets for sending coins and listing transactions.
    std::unique_ptr<const PlatformStyle> platformStyle(PlatformStyle::instantiate("other"));
    SendCoinsDialog sendCoinsDialog(platformStyle.get());
    TransactionView transactionView(platformStyle.get());
    OptionsModel optionsModel;
    WalletModel walletModel(platformStyle.get(), &wallet, &optionsModel);
    sendCoinsDialog.setModel(&walletModel);
    transactionView.setModel(&walletModel);

    // Send two transactions.
    fWalletRbf = false;
    uint256 txid = SendCoins(wallet, sendCoinsDialog, CBitcoinAddress(test.coinbaseKey.GetPubKey().GetID()), 5 * COIN);
    fWalletRbf = true;
    uint256 rbftxid = SendCoins(wallet, sendCoinsDialog, CBitcoinAddress(test.coinbaseKey.GetPubKey().GetID()), 10 * COIN);

    // Call bumpfee. FIXME describe.
    BumpFee(transactionView, txid, true /* expect disabled */, "lala" /* expected error */, false /* cancel */);
    BumpFee(transactionView, rbftxid, false /* expect disabled */, {} /* expected error */, false /* cancel */);
    BumpFee(transactionView, rbftxid, false /* expect disabled */, "lala" /* expected error */, true /* cancel */);

    bitdb.Flush(true);
    bitdb.Reset();
}
