#include <qt/test/addressbooktests.h>
#include <qt/test/util.h>
#include <test/test_bitcoin.h>

#include <qt/addressbookpage.h>
#include <qt/addresstablemodel.h>
#include <qt/editaddressdialog.h>
#include <qt/callback.h>
#include <qt/optionsmodel.h>
#include <qt/platformstyle.h>
#include <qt/qvalidatedlineedit.h>
#include <qt/walletmodel.h>

#include <key.h>
#include <pubkey.h>
#include <key_io.h>
#include <wallet/wallet.h>
 
#include <QTimer>
#include <QMessageBox>

namespace
{

/**
 * Fill the edit address dialog box with data, submit it, and ensure that
 * the resulting message meets expectations.
 */
void EditAddressAndSubmit(
        EditAddressDialog* dialog,
        const QString& label, const QString& address, QString expected_msg)
{
    QString warning_text;

    dialog->findChild<QLineEdit*>("labelEdit")->setText(label);
    dialog->findChild<QValidatedLineEdit*>("addressEdit")->setText(address);

    ConfirmMessage(&warning_text, 5);
    dialog->accept();
    QCOMPARE(warning_text, expected_msg);
}

/**
 * Test adding various send addresses to the address book.
 *
 * There are three cases tested:
 *
 *   - new_address: a new address would should add as a send address successfully.
 *   - existing_s_address: an existing sending address which won't add successfully.
 *   - existing_r_address: an existing receiving address which won't add successfully.
 *
 * In each case, verify the resulting state of the address book and optionally
 * the warning message presented to the user.
 */
void TestAddAddressesToSendBook()
{
    TestChain100Setup test;
    CWallet wallet("mock", CWalletDBWrapper::CreateMock());
    bool firstRun;
    wallet.LoadWallet(firstRun);

    auto build_address = [&wallet]() {
        CKey key;
        key.MakeNewKey(true);
        CTxDestination dest(GetDestinationForKey(
            key.GetPubKey(), wallet.m_default_address_type));

        return std::make_pair(dest, QString::fromStdString(EncodeDestination(dest)));
    };

    CTxDestination r_key_dest, s_key_dest;

    // Add a preexisting "receive" entry in the address book.
    QString preexisting_r_address;

    // Add a preexisting "send" entry in the address book.
    QString preexisting_s_address;

    // Define a new address (which should add to the address book successfully).
    QString new_address;

    std::tie(r_key_dest, preexisting_r_address) = build_address();
    std::tie(s_key_dest, preexisting_s_address) = build_address();
    std::tie(std::ignore, new_address) = build_address();

    {
        LOCK(wallet.cs_wallet);
        wallet.SetAddressBook(r_key_dest, "already here (r)", "receive");
        wallet.SetAddressBook(s_key_dest, "already here (s)", "send");
    }

    auto check_addbook_size = [&wallet](int expected_size) {
        QCOMPARE(static_cast<int>(wallet.mapAddressBook.size()), expected_size);
    };

    // We should start with the two addresses we added earlier and nothing else.
    check_addbook_size(2);

    // Initialize relevant QT models.
    std::unique_ptr<const PlatformStyle> platformStyle(PlatformStyle::instantiate("other"));
    OptionsModel optionsModel;
    WalletModel walletModel(platformStyle.get(), &wallet, &optionsModel);
    AddressTableModel* addressTableModel = walletModel.getAddressTableModel();
    EditAddressDialog editAddressDialog(EditAddressDialog::NewSendingAddress);
    editAddressDialog.setModel(walletModel.getAddressTableModel());

    EditAddressAndSubmit(
        &editAddressDialog, QString("uhoh"), preexisting_r_address,
        QString("Receiving address \"%1\" cannot be added as a sending address.")
        .arg(preexisting_r_address));

    check_addbook_size(2);

    EditAddressAndSubmit(
        &editAddressDialog, QString("uhoh, different"), preexisting_s_address,
        QString("The entered address \"%1\" is already in the address book.")
        .arg(preexisting_s_address));

    check_addbook_size(2);

    EditAddressAndSubmit(
        &editAddressDialog, QString("new"), new_address, QString(""));

    check_addbook_size(3);
}

} // namespace

void AddressBookTests::addressBookTests()
{
    TestAddAddressesToSendBook();
}
