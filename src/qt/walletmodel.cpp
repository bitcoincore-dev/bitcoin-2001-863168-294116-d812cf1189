// Copyright (c) 2011-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "walletmodel.h"

#include "addresstablemodel.h"
#include "consensus/validation.h"
#include "guiconstants.h"
#include "guiutil.h"
#include "paymentserver.h"
#include "recentrequeststablemodel.h"
#include "transactiontablemodel.h"

#include "base58.h"
#include "keystore.h"
#include "validation.h"
#include "net.h" // for g_connman
#include "sync.h"
#include "ui_interface.h"
#include "util.h" // for GetBoolArg
#include "wallet/wallet.h"
#include "wallet/walletdb.h" // for BackupWallet

#include <stdint.h>

#include <QDebug>
#include <QSet>
#include <QTimer>

#include <boost/foreach.hpp>

WalletModel::WalletModel(std::unique_ptr<ipc::Wallet> _ipcWallet, ipc::Node& _ipcNode, const PlatformStyle *platformStyle, CWallet *_wallet, OptionsModel *_optionsModel, QObject *parent) :
    QObject(parent), ipcWallet(std::move(_ipcWallet)), ipcNode(_ipcNode), wallet(_wallet), optionsModel(_optionsModel), addressTableModel(0),
    transactionTableModel(0),
    recentRequestsTableModel(0),
    cachedEncryptionStatus(Unencrypted),
    cachedNumBlocks(0)
{
    fHaveWatchOnly = ipcWallet->haveWatchOnly();
    fForceCheckBalanceChanged = false;

    addressTableModel = new AddressTableModel(wallet, this);
    transactionTableModel = new TransactionTableModel(platformStyle, wallet, this);
    recentRequestsTableModel = new RecentRequestsTableModel(wallet, this);

    // This timer will be fired repeatedly to update the balance
    pollTimer = new QTimer(this);
    connect(pollTimer, SIGNAL(timeout()), this, SLOT(pollBalanceChanged()));
    pollTimer->start(MODEL_UPDATE_DELAY);

    subscribeToCoreSignals();
}

WalletModel::~WalletModel()
{
    unsubscribeFromCoreSignals();
}

CAmount WalletModel::getBalance(const CCoinControl *coinControl) const
{
    if (coinControl)
    {
        return ipcWallet->getAvailableBalance(*coinControl);
    }

    return ipcWallet->getBalance();
}

CAmount WalletModel::getUnconfirmedBalance() const
{
    return ipcWallet->getUnconfirmedBalance();
}

CAmount WalletModel::getImmatureBalance() const
{
    return ipcWallet->getImmatureBalance();
}

bool WalletModel::haveWatchOnly() const
{
    return fHaveWatchOnly;
}

CAmount WalletModel::getWatchBalance() const
{
    return ipcWallet->getWatchOnlyBalance();
}

CAmount WalletModel::getWatchUnconfirmedBalance() const
{
    return ipcWallet->getUnconfirmedWatchOnlyBalance();
}

CAmount WalletModel::getWatchImmatureBalance() const
{
    return ipcWallet->getImmatureWatchOnlyBalance();
}

void WalletModel::updateStatus()
{
    EncryptionStatus newEncryptionStatus = getEncryptionStatus();

    if(cachedEncryptionStatus != newEncryptionStatus)
        Q_EMIT encryptionStatusChanged(newEncryptionStatus);
}

void WalletModel::pollBalanceChanged()
{
    // Get required locks upfront. This avoids the GUI from getting stuck on
    // periodical polls if the core is holding the locks for a longer time -
    // for example, during a wallet rescan.
    ipc::WalletBalances newBalances;
    int numBlocks = -1;
    if (!ipcWallet->tryGetBalances(newBalances, numBlocks)) {
        return;
    }

    if(fForceCheckBalanceChanged || ipcNode.getNumBlocks() != cachedNumBlocks)
    {
        fForceCheckBalanceChanged = false;

        // Balance and number of transactions might have changed
        cachedNumBlocks = ipcNode.getNumBlocks();

        checkBalanceChanged(newBalances);
        if(transactionTableModel)
            transactionTableModel->updateConfirmations();
    }
}

void WalletModel::checkBalanceChanged(const ipc::WalletBalances& newBalances)
{
    if(newBalances.balanceChanged(cachedBalances)) {
        cachedBalances = newBalances;
        Q_EMIT balanceChanged(newBalances.balance, newBalances.unconfirmedBalance, newBalances.immatureBalance, newBalances.watchOnlyBalance, newBalances.unconfirmedWatchOnlyBalance, newBalances.immatureWatchOnlyBalance);
    }
}

void WalletModel::updateTransaction()
{
    // Balance and number of transactions might have changed
    fForceCheckBalanceChanged = true;
}

void WalletModel::updateAddressBook(const QString &address, const QString &label,
        bool isMine, const QString &purpose, int status)
{
    if(addressTableModel)
        addressTableModel->updateEntry(address, label, isMine, purpose, status);
}

void WalletModel::updateWatchOnlyFlag(bool fHaveWatchonly)
{
    fHaveWatchOnly = fHaveWatchonly;
    Q_EMIT notifyWatchonlyChanged(fHaveWatchonly);
}

bool WalletModel::validateAddress(const QString &address)
{
    CBitcoinAddress addressParsed(address.toStdString());
    return addressParsed.IsValid();
}

WalletModel::SendCoinsReturn WalletModel::prepareTransaction(WalletModelTransaction &transaction, const CCoinControl *coinControl)
{
    CAmount total = 0;
    bool fSubtractFeeFromAmount = false;
    QList<SendCoinsRecipient> recipients = transaction.getRecipients();
    std::vector<CRecipient> vecSend;

    if(recipients.empty())
    {
        return OK;
    }

    QSet<QString> setAddress; // Used to detect duplicates
    int nAddresses = 0;

    // Pre-check input data for validity
    Q_FOREACH(const SendCoinsRecipient &rcp, recipients)
    {
        if (rcp.fSubtractFeeFromAmount)
            fSubtractFeeFromAmount = true;

        if (rcp.paymentRequest.IsInitialized())
        {   // PaymentRequest...
            CAmount subtotal = 0;
            const payments::PaymentDetails& details = rcp.paymentRequest.getDetails();
            for (int i = 0; i < details.outputs_size(); i++)
            {
                const payments::Output& out = details.outputs(i);
                if (out.amount() <= 0) continue;
                subtotal += out.amount();
                const unsigned char* scriptStr = (const unsigned char*)out.script().data();
                CScript scriptPubKey(scriptStr, scriptStr+out.script().size());
                CAmount nAmount = out.amount();
                CRecipient recipient = {scriptPubKey, nAmount, rcp.fSubtractFeeFromAmount};
                vecSend.push_back(recipient);
            }
            if (subtotal <= 0)
            {
                return InvalidAmount;
            }
            total += subtotal;
        }
        else
        {   // User-entered bitcoin address / amount:
            if(!validateAddress(rcp.address))
            {
                return InvalidAddress;
            }
            if(rcp.amount <= 0)
            {
                return InvalidAmount;
            }
            setAddress.insert(rcp.address);
            ++nAddresses;

            CScript scriptPubKey = GetScriptForDestination(CBitcoinAddress(rcp.address.toStdString()).Get());
            CRecipient recipient = {scriptPubKey, rcp.amount, rcp.fSubtractFeeFromAmount};
            vecSend.push_back(recipient);

            total += rcp.amount;
        }
    }
    if(setAddress.size() != nAddresses)
    {
        return DuplicateAddress;
    }

    CAmount nBalance = getBalance(coinControl);

    if(total > nBalance)
    {
        return AmountExceedsBalance;
    }

    {

        CAmount nFeeRequired = 0;
        int nChangePosRet = -1;
        std::string strFailReason;

        auto& newTx = transaction.getWtx();
        newTx = ipcWallet->createTransaction(vecSend, coinControl, true /* sign */, nChangePosRet, nFeeRequired, strFailReason);
        transaction.setTransactionFee(nFeeRequired);
        if (fSubtractFeeFromAmount && newTx)
            transaction.reassignAmounts(nChangePosRet);

        if(!newTx)
        {
            if(!fSubtractFeeFromAmount && (total + nFeeRequired) > nBalance)
            {
                return SendCoinsReturn(AmountWithFeeExceedsBalance);
            }
            Q_EMIT message(tr("Send Coins"), QString::fromStdString(strFailReason),
                         CClientUIInterface::MSG_ERROR);
            return TransactionCreationFailed;
        }

        // reject absurdly high fee. (This can never happen because the
        // wallet caps the fee at maxTxFee. This merely serves as a
        // belt-and-suspenders check)
        if (nFeeRequired > ipcNode.getMaxTxFee())
            return AbsurdFee;
    }

    return SendCoinsReturn(OK);
}

WalletModel::SendCoinsReturn WalletModel::sendCoins(WalletModelTransaction &transaction)
{
    QByteArray transaction_array; /* store serialized transaction */

    {
        std::vector<std::pair<std::string, std::string>> vOrderForm;
        Q_FOREACH(const SendCoinsRecipient &rcp, transaction.getRecipients())
        {
            if (rcp.paymentRequest.IsInitialized())
            {
                // Make sure any payment requests involved are still valid.
                if (PaymentServer::verifyExpired(rcp.paymentRequest.getDetails())) {
                    return PaymentRequestExpired;
                }

                // Store PaymentRequests in wtx.vOrderForm in wallet.
                std::string value;
                rcp.paymentRequest.SerializeToString(&value);
                vOrderForm.emplace_back("PaymentRequest", std::move(value));
            }
            else if (!rcp.message.isEmpty()) // Message from normal bitcoin:URI (bitcoin:123...?message=example)
                vOrderForm.emplace_back("Message", rcp.message.toStdString());
        }

        auto& newTx = transaction.getWtx();
        CValidationState state;
        if (!newTx->commit({} /* mapValue */, std::move(vOrderForm), {} /* fromAccount */, state))
            return SendCoinsReturn(TransactionCommitFailed, QString::fromStdString(state.GetRejectReason()));

        CDataStream ssTx(SER_NETWORK, PROTOCOL_VERSION);
        ssTx << newTx->get();
        transaction_array.append(&(ssTx[0]), ssTx.size());
    }

    // Add addresses / update labels that we've sent to to the address book,
    // and emit coinsSent signal for each recipient
    Q_FOREACH(const SendCoinsRecipient &rcp, transaction.getRecipients())
    {
        // Don't touch the address book when we have a payment request
        if (!rcp.paymentRequest.IsInitialized())
        {
            std::string strAddress = rcp.address.toStdString();
            CTxDestination dest = CBitcoinAddress(strAddress).Get();
            std::string strLabel = rcp.label.toStdString();
            {
                // Check if we have a new address or an updated label
                std::string name;
                if (!ipcWallet->getAddress(dest, &name))
                {
                    ipcWallet->setAddressBook(dest, strLabel, "send");
                }
                else if (name != strLabel)
                {
                    ipcWallet->setAddressBook(dest, strLabel, ""); // "" means don't change purpose
                }
            }
        }
        Q_EMIT coinsSent(wallet, rcp, transaction_array);
    }

    checkBalanceChanged(ipcWallet->getBalances()); // update balance immediately, otherwise there could be a short noticeable delay until pollBalanceChanged hits

    return SendCoinsReturn(OK);
}

OptionsModel *WalletModel::getOptionsModel()
{
    return optionsModel;
}

AddressTableModel *WalletModel::getAddressTableModel()
{
    return addressTableModel;
}

TransactionTableModel *WalletModel::getTransactionTableModel()
{
    return transactionTableModel;
}

RecentRequestsTableModel *WalletModel::getRecentRequestsTableModel()
{
    return recentRequestsTableModel;
}

WalletModel::EncryptionStatus WalletModel::getEncryptionStatus() const
{
    if(!ipcWallet->isCrypted())
    {
        return Unencrypted;
    }
    else if(ipcWallet->isLocked())
    {
        return Locked;
    }
    else
    {
        return Unlocked;
    }
}

bool WalletModel::setWalletEncrypted(bool encrypted, const SecureString &passphrase)
{
    if(encrypted)
    {
        // Encrypt
        return ipcWallet->encryptWallet(passphrase);
    }
    else
    {
        // Decrypt -- TODO; not supported yet
        return false;
    }
}

bool WalletModel::setWalletLocked(bool locked, const SecureString &passPhrase)
{
    if(locked)
    {
        // Lock
        return ipcWallet->lock();
    }
    else
    {
        // Unlock
        return ipcWallet->unlock(passPhrase);
    }
}

bool WalletModel::changePassphrase(const SecureString &oldPass, const SecureString &newPass)
{
    bool retval;
    {
        ipcWallet->lock(); // Make sure wallet is locked before attempting pass change
        retval = ipcWallet->changeWalletPassphrase(oldPass, newPass);
    }
    return retval;
}

bool WalletModel::backupWallet(const QString &filename)
{
    return ipcWallet->backupWallet(filename.toLocal8Bit().data());
}

// Handlers for core signals
static void NotifyKeyStoreStatusChanged(WalletModel *walletmodel)
{
    qDebug() << "NotifyKeyStoreStatusChanged";
    QMetaObject::invokeMethod(walletmodel, "updateStatus", Qt::QueuedConnection);
}

static void NotifyAddressBookChanged(WalletModel *walletmodel,
        const CTxDestination &address, const std::string &label, bool isMine,
        const std::string &purpose, ChangeType status)
{
    QString strAddress = QString::fromStdString(CBitcoinAddress(address).ToString());
    QString strLabel = QString::fromStdString(label);
    QString strPurpose = QString::fromStdString(purpose);

    qDebug() << "NotifyAddressBookChanged: " + strAddress + " " + strLabel + " isMine=" + QString::number(isMine) + " purpose=" + strPurpose + " status=" + QString::number(status);
    QMetaObject::invokeMethod(walletmodel, "updateAddressBook", Qt::QueuedConnection,
                              Q_ARG(QString, strAddress),
                              Q_ARG(QString, strLabel),
                              Q_ARG(bool, isMine),
                              Q_ARG(QString, strPurpose),
                              Q_ARG(int, status));
}

static void NotifyTransactionChanged(WalletModel *walletmodel, const uint256 &hash, ChangeType status)
{
    Q_UNUSED(hash);
    Q_UNUSED(status);
    QMetaObject::invokeMethod(walletmodel, "updateTransaction", Qt::QueuedConnection);
}

static void ShowProgress(WalletModel *walletmodel, const std::string &title, int nProgress)
{
    // emits signal "showProgress"
    QMetaObject::invokeMethod(walletmodel, "showProgress", Qt::QueuedConnection,
                              Q_ARG(QString, QString::fromStdString(title)),
                              Q_ARG(int, nProgress));
}

static void NotifyWatchonlyChanged(WalletModel *walletmodel, bool fHaveWatchonly)
{
    QMetaObject::invokeMethod(walletmodel, "updateWatchOnlyFlag", Qt::QueuedConnection,
                              Q_ARG(bool, fHaveWatchonly));
}

void WalletModel::subscribeToCoreSignals()
{
    // Connect signals to wallet
    handlerStatusChanged = ipcWallet->handleStatusChanged(boost::bind(&NotifyKeyStoreStatusChanged, this));
    handlerAddressBookChanged = ipcWallet->handleAddressBookChanged(boost::bind(NotifyAddressBookChanged, this, _1, _2, _3, _4, _5));
    handlerTransactionChanged = ipcWallet->handleTransactionChanged(boost::bind(NotifyTransactionChanged, this, _1, _2));
    handlerShowProgress = ipcWallet->handleShowProgress(boost::bind(ShowProgress, this, _1, _2));
    handlerWatchonlyChanged = ipcWallet->handleWatchonlyChanged(boost::bind(NotifyWatchonlyChanged, this, _1));
}

void WalletModel::unsubscribeFromCoreSignals()
{
    // Disconnect signals from wallet
    handlerStatusChanged->disconnect();
    handlerAddressBookChanged->disconnect();
    handlerTransactionChanged->disconnect();
    handlerShowProgress->disconnect();
    handlerWatchonlyChanged->disconnect();
}

// WalletModel::UnlockContext implementation
WalletModel::UnlockContext WalletModel::requestUnlock()
{
    bool was_locked = getEncryptionStatus() == Locked;
    if(was_locked)
    {
        // Request UI to unlock wallet
        Q_EMIT requireUnlock();
    }
    // If wallet is still locked, unlock was failed or cancelled, mark context as invalid
    bool valid = getEncryptionStatus() != Locked;

    return UnlockContext(this, valid, was_locked);
}

WalletModel::UnlockContext::UnlockContext(WalletModel *_wallet, bool _valid, bool _relock):
        wallet(_wallet),
        valid(_valid),
        relock(_relock)
{
}

WalletModel::UnlockContext::~UnlockContext()
{
    if(valid && relock)
    {
        wallet->setWalletLocked(true);
    }
}

void WalletModel::UnlockContext::CopyFrom(const UnlockContext& rhs)
{
    // Transfer context; old object no longer relocks wallet
    *this = rhs;
    rhs.relock = false;
}

bool WalletModel::getPubKey(const CKeyID &address, CPubKey& vchPubKeyOut) const
{
    return ipcWallet->getPubKey(address, vchPubKeyOut);
}

bool WalletModel::havePrivKey(const CKeyID &address) const
{
    return ipcWallet->haveKey(address);
}

bool WalletModel::getPrivKey(const CKeyID &address, CKey& vchPrivKeyOut) const
{
    return ipcWallet->getKey(address, vchPrivKeyOut);
}

bool WalletModel::isLockedCoin(uint256 hash, unsigned int n) const
{
    return ipcWallet->isLockedCoin(COutPoint(hash, n));
}

void WalletModel::lockCoin(COutPoint& output)
{
    ipcWallet->lockCoin(output);
}

void WalletModel::unlockCoin(COutPoint& output)
{
    ipcWallet->unlockCoin(output);
}

void WalletModel::listLockedCoins(std::vector<COutPoint>& vOutpts)
{
    ipcWallet->listLockedCoins(vOutpts);
}

void WalletModel::loadReceiveRequests(std::vector<std::string>& vReceiveRequests)
{
    ipcWallet->getDestValues("rr", vReceiveRequests); // receive request
}

bool WalletModel::saveReceiveRequest(const std::string &sAddress, const int64_t nId, const std::string &sRequest)
{
    CTxDestination dest = CBitcoinAddress(sAddress).Get();

    std::stringstream ss;
    ss << nId;
    std::string key = "rr" + ss.str(); // "rr" prefix = "receive request" in destdata

    if (sRequest.empty())
        return ipcWallet->eraseDestData(dest, key);
    else
        return ipcWallet->addDestData(dest, key, sRequest);
}

bool WalletModel::transactionCanBeAbandoned(uint256 hash) const
{
    return ipcWallet->transactionCanBeAbandoned(hash);
}

bool WalletModel::abandonTransaction(uint256 hash) const
{
    return ipcWallet->abandonTransaction(hash);
}

bool WalletModel::isWalletEnabled()
{
   return !GetBoolArg("-disablewallet", DEFAULT_DISABLE_WALLET);
}

bool WalletModel::hdEnabled() const
{
    return ipcWallet->hdEnabled();
}

int WalletModel::getDefaultConfirmTarget() const
{
    return ipcNode.getTxConfirmTarget();
}

bool WalletModel::getDefaultWalletRbf() const
{
    return ipcNode.getWalletRbf();
}
