// Copyright (c) 2011-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include "config/bitcoin-config.h"
#endif

#include "optionsmodel.h"

#include "bitcoinunits.h"
#include "guiutil.h"

#include "amount.h"
#include "init.h"
#include "main.h" // For DEFAULT_SCRIPTCHECK_THREADS
#include "net.h"
#include "txdb.h" // for -dbcache defaults
#include "chainparams.h"
#include "intro.h" 
#include "policy/policy.h"
#include "txmempool.h" // for fPriorityAccurate

#ifdef ENABLE_WALLET
#include "wallet/wallet.h"
#include "wallet/walletdb.h"
#endif

#include <QNetworkProxy>
#include <QSettings>
#include <QStringList>

static QString CanonicalMempoolReplacement()
{
    if (!fEnableReplacement) {
        return "never";
    } else if (fReplacementHonourOptOut) {
        return "fee,optin";
    } else {
        return "fee,-optin";
    }
}

OptionsModel::OptionsModel(QObject *parent, bool resetSettings) :
    QAbstractListModel(parent)
{
    Init(resetSettings);
}

void OptionsModel::addOverriddenOption(const std::string &option)
{
    strOverriddenByCommandLine += QString::fromStdString(option) + "=" + QString::fromStdString(mapArgs[option]) + " ";
}

// Writes all missing QSettings with their default values
void OptionsModel::Init(bool resetSettings)
{
    if (resetSettings)
        Reset();

    checkAndMigrate();

    QSettings settings;

    // Ensure restart flag is unset on client startup
    setRestartRequired(false);

    // These are Qt-only settings:

    // Window
    if (!settings.contains("fHideTrayIcon"))
        settings.setValue("fHideTrayIcon", false);
    fHideTrayIcon = settings.value("fHideTrayIcon").toBool();
    Q_EMIT hideTrayIconChanged(fHideTrayIcon);
    
    if (!settings.contains("fMinimizeToTray"))
        settings.setValue("fMinimizeToTray", false);
    fMinimizeToTray = settings.value("fMinimizeToTray").toBool() && !fHideTrayIcon;

    if (!settings.contains("fMinimizeOnClose"))
        settings.setValue("fMinimizeOnClose", false);
    fMinimizeOnClose = settings.value("fMinimizeOnClose").toBool();

    // Display
    if (!settings.contains("nDisplayUnit"))
        settings.setValue("nDisplayUnit", BitcoinUnits::BTC);
    nDisplayUnit = settings.value("nDisplayUnit").toInt();

    if (!settings.contains("bDisplayAddresses"))
        settings.setValue("bDisplayAddresses", false);
    bDisplayAddresses = settings.value("bDisplayAddresses", false).toBool();

    if (!settings.contains("strThirdPartyTxUrls"))
        settings.setValue("strThirdPartyTxUrls", "");
    strThirdPartyTxUrls = settings.value("strThirdPartyTxUrls", "").toString();

    if (!settings.contains("fCoinControlFeatures"))
        settings.setValue("fCoinControlFeatures", false);
    fCoinControlFeatures = settings.value("fCoinControlFeatures", false).toBool();

    // These are shared with the core or have a command-line parameter
    // and we want command-line parameters to overwrite the GUI settings.
    //
    // If setting doesn't exist create it with defaults.
    //
    // If SoftSetArg() or SoftSetBoolArg() return false we were overridden
    // by command-line and show this in the UI.

    // Main
    if (!settings.contains("nDatabaseCache"))
        settings.setValue("nDatabaseCache", (qint64)nDefaultDbCache);
    if (!SoftSetArg("-dbcache", settings.value("nDatabaseCache").toString().toStdString()))
        addOverriddenOption("-dbcache");

    if (!settings.contains("nThreadsScriptVerif"))
        settings.setValue("nThreadsScriptVerif", DEFAULT_SCRIPTCHECK_THREADS);
    if (!SoftSetArg("-par", settings.value("nThreadsScriptVerif").toString().toStdString()))
        addOverriddenOption("-par");

    if (!settings.contains("strDataDir"))
        settings.setValue("strDataDir", Intro::getDefaultDataDirectory());

    // Wallet
#ifdef ENABLE_WALLET
    if (!settings.contains("bSpendZeroConfChange"))
        settings.setValue("bSpendZeroConfChange", true);
    if (!SoftSetBoolArg("-spendzeroconfchange", settings.value("bSpendZeroConfChange").toBool()))
        addOverriddenOption("-spendzeroconfchange");
#endif

    // Network
    if (!settings.contains("nNetworkPort"))
        settings.setValue("nNetworkPort", (quint16)Params().GetDefaultPort());
    if (!SoftSetArg("-port", settings.value("nNetworkPort").toString().toStdString()))
        addOverriddenOption("-port");

    if (!settings.contains("fUseUPnP"))
        settings.setValue("fUseUPnP", DEFAULT_UPNP);
    if (!SoftSetBoolArg("-upnp", settings.value("fUseUPnP").toBool()))
        addOverriddenOption("-upnp");

    if (!settings.contains("fListen"))
        settings.setValue("fListen", DEFAULT_LISTEN);
    if (!SoftSetBoolArg("-listen", settings.value("fListen").toBool()))
        addOverriddenOption("-listen");

    if (!settings.contains("fUseProxy"))
        settings.setValue("fUseProxy", false);
    if (!settings.contains("addrProxy"))
        settings.setValue("addrProxy", "127.0.0.1:9050");
    // Only try to set -proxy, if user has enabled fUseProxy
    if (settings.value("fUseProxy").toBool() && !SoftSetArg("-proxy", settings.value("addrProxy").toString().toStdString()))
        addOverriddenOption("-proxy");
    else if(!settings.value("fUseProxy").toBool() && !GetArg("-proxy", "").empty())
        addOverriddenOption("-proxy");

    if (!settings.contains("fUseSeparateProxyTor"))
        settings.setValue("fUseSeparateProxyTor", false);
    if (!settings.contains("addrSeparateProxyTor"))
        settings.setValue("addrSeparateProxyTor", "127.0.0.1:9050");
    // Only try to set -onion, if user has enabled fUseSeparateProxyTor
    if (settings.value("fUseSeparateProxyTor").toBool() && !SoftSetArg("-onion", settings.value("addrSeparateProxyTor").toString().toStdString()))
        addOverriddenOption("-onion");
    else if(!settings.value("fUseSeparateProxyTor").toBool() && !GetArg("-onion", "").empty())
        addOverriddenOption("-onion");

    // rwconf settings that require a restart
    f_peerbloomfilters = GetBoolArg("-peerbloomfilters", DEFAULT_PEERBLOOMFILTERS);

    // Display
    if (!settings.contains("language"))
        settings.setValue("language", "");
    if (!SoftSetArg("-lang", settings.value("language").toString().toStdString()))
        addOverriddenOption("-lang");

    language = settings.value("language").toString();
}

void OptionsModel::Reset()
{
    QSettings settings;

    // Save the strDataDir setting
    QString dataDir = Intro::getDefaultDataDirectory();
    dataDir = settings.value("strDataDir", dataDir).toString();

    // Remove rw config file
    EraseRWConfigFile();

    // Remove all entries from our QSettings object
    settings.clear();

    // Set strDataDir
    settings.setValue("strDataDir", dataDir);

    // Set that this was reset
    settings.setValue("fReset", true);

    // default setting for OptionsModel::StartAtStartup - disabled
    if (GUIUtil::GetStartOnSystemStartup())
        GUIUtil::SetStartOnSystemStartup(false);
}

int OptionsModel::rowCount(const QModelIndex & parent) const
{
    return OptionIDRowCount;
}

// read QSettings values and return them
QVariant OptionsModel::data(const QModelIndex & index, int role) const
{
    if(role == Qt::EditRole)
    {
        QSettings settings;
        switch(index.row())
        {
        case StartAtStartup:
            return GUIUtil::GetStartOnSystemStartup();
        case HideTrayIcon:
            return fHideTrayIcon;
        case MinimizeToTray:
            return fMinimizeToTray;
        case NetworkPort:
            return settings.value("nNetworkPort");
        case MapPortUPnP:
#ifdef USE_UPNP
            return settings.value("fUseUPnP");
#else
            return false;
#endif
        case MinimizeOnClose:
            return fMinimizeOnClose;

        // default proxy
        case ProxyUse:
            return settings.value("fUseProxy", false);
        case ProxyIP: {
            // contains IP at index 0 and port at index 1
            QStringList strlIpPort = settings.value("addrProxy").toString().split(":", QString::SkipEmptyParts);
            return strlIpPort.at(0);
        }
        case ProxyPort: {
            // contains IP at index 0 and port at index 1
            QStringList strlIpPort = settings.value("addrProxy").toString().split(":", QString::SkipEmptyParts);
            return strlIpPort.at(1);
        }

        // separate Tor proxy
        case ProxyUseTor:
            return settings.value("fUseSeparateProxyTor", false);
        case ProxyIPTor: {
            // contains IP at index 0 and port at index 1
            QStringList strlIpPort = settings.value("addrSeparateProxyTor").toString().split(":", QString::SkipEmptyParts);
            return strlIpPort.at(0);
        }
        case ProxyPortTor: {
            // contains IP at index 0 and port at index 1
            QStringList strlIpPort = settings.value("addrSeparateProxyTor").toString().split(":", QString::SkipEmptyParts);
            return strlIpPort.at(1);
        }

#ifdef ENABLE_WALLET
        case SpendZeroConfChange:
            return settings.value("bSpendZeroConfChange");
#endif
        case DisplayUnit:
            return nDisplayUnit;
        case DisplayAddresses:
            return bDisplayAddresses;
        case ThirdPartyTxUrls:
            return strThirdPartyTxUrls;
        case Language:
            return settings.value("language");
        case CoinControlFeatures:
            return fCoinControlFeatures;
        case DatabaseCache:
            return settings.value("nDatabaseCache");
        case ThreadsScriptVerif:
            return settings.value("nThreadsScriptVerif");
        case Listen:
            return settings.value("fListen");
        case maxuploadtarget:
            return qlonglong(CNode::GetMaxOutboundTarget() / 1024 / 1024);
        case peerbloomfilters:
            return f_peerbloomfilters;
        case mempoolreplacement:
            return CanonicalMempoolReplacement();
        case maxorphantx:
            return qlonglong(GetArg("-maxorphantx", DEFAULT_MAX_ORPHAN_TRANSACTIONS));
        case maxmempool:
            return qlonglong(GetArg("-maxmempool", DEFAULT_MAX_MEMPOOL_SIZE));
        case mempoolexpiry:
            return qlonglong(GetArg("-mempoolexpiry", DEFAULT_MEMPOOL_EXPIRY));
        case rejectunknownscripts:
            return fRequireStandard;
        case bytespersigop:
            return nBytesPerSigOp;
        case bytespersigopstrict:
            return nBytesPerSigOpStrict;
        case limitancestorcount:
            return qlonglong(GetArg("-limitancestorcount", DEFAULT_ANCESTOR_LIMIT));
        case limitancestorsize:
            return qlonglong(GetArg("-limitancestorsize", DEFAULT_ANCESTOR_SIZE_LIMIT));
        case limitdescendantcount:
            return qlonglong(GetArg("-limitdescendantcount", DEFAULT_DESCENDANT_LIMIT));
        case limitdescendantsize:
            return qlonglong(GetArg("-limitdescendantsize", DEFAULT_DESCENDANT_SIZE_LIMIT));
        case spamfilter:
            return bool(GetArg("-spamfilter", DEFAULT_SPAMFILTER));
        case rejectbaremultisig:
            return !fIsBareMultisigStd;
        case datacarriersize:
            return fAcceptDatacarrier ? qlonglong(nMaxDatacarrierBytes) : qlonglong(0);
        case blockmaxsize:
            return qlonglong(GetArg("-blockmaxsize", DEFAULT_BLOCK_MAX_SIZE) / 1000);
        case blockprioritysize:
            return qlonglong(GetArg("-blockprioritysize", DEFAULT_BLOCK_PRIORITY_SIZE) / 1000);
        case blockmaxweight:
            return qlonglong(GetArg("-blockmaxweight", DEFAULT_BLOCK_MAX_WEIGHT) / 1000);
        case priorityaccurate:
            return fPriorityAccurate;
        default:
            return QVariant();
        }
    }
    return QVariant();
}

// write QSettings values
bool OptionsModel::setData(const QModelIndex & index, const QVariant & value, int role)
{
    bool successful = true; /* set to false on parse error */
    if(role == Qt::EditRole)
    {
        QSettings settings;
        switch(index.row())
        {
        case StartAtStartup:
            successful = GUIUtil::SetStartOnSystemStartup(value.toBool());
            break;
        case HideTrayIcon:
            fHideTrayIcon = value.toBool();
            settings.setValue("fHideTrayIcon", fHideTrayIcon);
    		Q_EMIT hideTrayIconChanged(fHideTrayIcon);
            break;
        case MinimizeToTray:
            fMinimizeToTray = value.toBool();
            settings.setValue("fMinimizeToTray", fMinimizeToTray);
            break;
        case NetworkPort:
            if (settings.value("nNetworkPort") != value) {
                // If the port input box is empty, set to default port
                if (value.toString().isEmpty()) {
                    settings.setValue("nNetworkPort", (quint16)Params().GetDefaultPort());
                }
                else {
                    settings.setValue("nNetworkPort", (quint16)value.toInt());
                }
                setRestartRequired(true);
            }
            break;
        case MapPortUPnP: // core option - can be changed on-the-fly
            settings.setValue("fUseUPnP", value.toBool());
            MapPort(value.toBool());
            break;
        case MinimizeOnClose:
            fMinimizeOnClose = value.toBool();
            settings.setValue("fMinimizeOnClose", fMinimizeOnClose);
            break;

        // default proxy
        case ProxyUse:
            if (settings.value("fUseProxy") != value) {
                settings.setValue("fUseProxy", value.toBool());
                setRestartRequired(true);
            }
            break;
        case ProxyIP: {
            // contains current IP at index 0 and current port at index 1
            QStringList strlIpPort = settings.value("addrProxy").toString().split(":", QString::SkipEmptyParts);
            // if that key doesn't exist or has a changed IP
            if (!settings.contains("addrProxy") || strlIpPort.at(0) != value.toString()) {
                // construct new value from new IP and current port
                QString strNewValue = value.toString() + ":" + strlIpPort.at(1);
                settings.setValue("addrProxy", strNewValue);
                setRestartRequired(true);
            }
        }
        break;
        case ProxyPort: {
            // contains current IP at index 0 and current port at index 1
            QStringList strlIpPort = settings.value("addrProxy").toString().split(":", QString::SkipEmptyParts);
            // if that key doesn't exist or has a changed port
            if (!settings.contains("addrProxy") || strlIpPort.at(1) != value.toString()) {
                // construct new value from current IP and new port
                QString strNewValue = strlIpPort.at(0) + ":" + value.toString();
                settings.setValue("addrProxy", strNewValue);
                setRestartRequired(true);
            }
        }
        break;

        // separate Tor proxy
        case ProxyUseTor:
            if (settings.value("fUseSeparateProxyTor") != value) {
                settings.setValue("fUseSeparateProxyTor", value.toBool());
                setRestartRequired(true);
            }
            break;
        case ProxyIPTor: {
            // contains current IP at index 0 and current port at index 1
            QStringList strlIpPort = settings.value("addrSeparateProxyTor").toString().split(":", QString::SkipEmptyParts);
            // if that key doesn't exist or has a changed IP
            if (!settings.contains("addrSeparateProxyTor") || strlIpPort.at(0) != value.toString()) {
                // construct new value from new IP and current port
                QString strNewValue = value.toString() + ":" + strlIpPort.at(1);
                settings.setValue("addrSeparateProxyTor", strNewValue);
                setRestartRequired(true);
            }
        }
        break;
        case ProxyPortTor: {
            // contains current IP at index 0 and current port at index 1
            QStringList strlIpPort = settings.value("addrSeparateProxyTor").toString().split(":", QString::SkipEmptyParts);
            // if that key doesn't exist or has a changed port
            if (!settings.contains("addrSeparateProxyTor") || strlIpPort.at(1) != value.toString()) {
                // construct new value from current IP and new port
                QString strNewValue = strlIpPort.at(0) + ":" + value.toString();
                settings.setValue("addrSeparateProxyTor", strNewValue);
                setRestartRequired(true);
            }
        }
        break;

#ifdef ENABLE_WALLET
        case SpendZeroConfChange:
            if (settings.value("bSpendZeroConfChange") != value) {
                settings.setValue("bSpendZeroConfChange", value);
                setRestartRequired(true);
            }
            break;
#endif
        case DisplayUnit:
            setDisplayUnit(value);
            break;
        case DisplayAddresses:
            bDisplayAddresses = value.toBool();
            settings.setValue("bDisplayAddresses", bDisplayAddresses);
            break;
        case ThirdPartyTxUrls:
            if (strThirdPartyTxUrls != value.toString()) {
                strThirdPartyTxUrls = value.toString();
                settings.setValue("strThirdPartyTxUrls", strThirdPartyTxUrls);
                setRestartRequired(true);
            }
            break;
        case Language:
            if (settings.value("language") != value) {
                settings.setValue("language", value);
                setRestartRequired(true);
            }
            break;
        case CoinControlFeatures:
            fCoinControlFeatures = value.toBool();
            settings.setValue("fCoinControlFeatures", fCoinControlFeatures);
            Q_EMIT coinControlFeaturesChanged(fCoinControlFeatures);
            break;
        case DatabaseCache:
            if (settings.value("nDatabaseCache") != value) {
                settings.setValue("nDatabaseCache", value);
                setRestartRequired(true);
            }
            break;
        case ThreadsScriptVerif:
            if (settings.value("nThreadsScriptVerif") != value) {
                settings.setValue("nThreadsScriptVerif", value);
                setRestartRequired(true);
            }
            break;
        case Listen:
            if (settings.value("fListen") != value) {
                settings.setValue("fListen", value);
                setRestartRequired(true);
            }
            break;
        case maxuploadtarget:
        {
            qlonglong nv = value.toLongLong() * 1024 * 1024;
            if (qlonglong(CNode::GetMaxOutboundTarget()) != nv) {
                ModifyRWConfigFile("maxuploadtarget", value.toString().toStdString());
                CNode::SetMaxOutboundTarget(nv);
            }
            break;
        }
        case peerbloomfilters:
            if (f_peerbloomfilters != value) {
                ModifyRWConfigFile("peerbloomfilters", strprintf("%d", value.toBool()));
                f_peerbloomfilters = value.toBool();
            }
            break;
        case mempoolreplacement:
        {
            QString nv = value.toString();
            if (nv != CanonicalMempoolReplacement()) {
                if (nv == "never") {
                    fEnableReplacement = false;
                    fReplacementHonourOptOut = true;
                } else if (nv == "fee,optin") {
                    fEnableReplacement = true;
                    fReplacementHonourOptOut = true;
                } else {  // "fee,-optin"
                    fEnableReplacement = true;
                    fReplacementHonourOptOut = false;
                }
                ModifyRWConfigFile("mempoolreplacement", nv.toStdString());
            }
            break;
        }
        case maxorphantx:
        {
            unsigned int nMaxOrphanTx = GetArg("-maxorphantx", DEFAULT_MAX_ORPHAN_TRANSACTIONS);
            unsigned int nNv = value.toLongLong();
            if (nNv != nMaxOrphanTx) {
                std::string strNv = value.toString().toStdString();
                mapArgs["-maxorphantx"] = strNv;
                ModifyRWConfigFile("maxorphantx", strNv);
                if (nNv < nMaxOrphanTx) {
                    LOCK(cs_main);
                    unsigned int nEvicted = LimitOrphanTxSize(nNv);
                    if (nEvicted > 0) {
                        LogPrint("mempool", "maxorphantx reduced from %d to %d, removed %u tx\n", nMaxOrphanTx, nNv, nEvicted);
                    }
                }
            }
            break;
        }
        case maxmempool:
        {
            long long nOldValue = GetArg("-maxmempool", DEFAULT_MAX_MEMPOOL_SIZE);
            long long nNv = value.toLongLong();
            if (nNv != nOldValue) {
                std::string strNv = value.toString().toStdString();
                mapArgs["-maxmempool"] = strNv;
                ModifyRWConfigFile("maxmempool", strNv);
                if (nNv < nOldValue) {
                    LimitMempoolSize();
                }
            }
            break;
        }
        case mempoolexpiry:
        {
            long long nOldValue = GetArg("-mempoolexpiry", DEFAULT_MEMPOOL_EXPIRY);
            long long nNv = value.toLongLong();
            if (nNv != nOldValue) {
                std::string strNv = value.toString().toStdString();
                mapArgs["-mempoolexpiry"] = strNv;
                ModifyRWConfigFile("mempoolexpiry", strNv);
                if (nNv < nOldValue) {
                    LimitMempoolSize();
                }
            }
            break;
        }
        case rejectunknownscripts:
        {
            const bool fNewValue = value.toBool();
            if (fNewValue != fRequireStandard) {
                fRequireStandard = fNewValue;
                // This option is inverted in the config:
                ModifyRWConfigFile("acceptnonstdtxn", strprintf("%d", ! fNewValue));
            }
            break;
        }
        case bytespersigop:
        {
            unsigned int nNv = value.toLongLong();
            if (nNv != nBytesPerSigOp) {
                ModifyRWConfigFile("bytespersigop", value.toString().toStdString());
                nBytesPerSigOp = nNv;
            }
            break;
        }
        case bytespersigopstrict:
        {
            unsigned int nNv = value.toLongLong();
            if (nNv != nBytesPerSigOpStrict) {
                ModifyRWConfigFile("bytespersigopstrict", value.toString().toStdString());
                nBytesPerSigOpStrict = nNv;
            }
            break;
        }
        case limitancestorcount:
        {
            long long nOldValue = GetArg("-limitancestorcount", DEFAULT_ANCESTOR_LIMIT);
            long long nNv = value.toLongLong();
            if (nNv != nOldValue) {
                std::string strNv = value.toString().toStdString();
                mapArgs["-limitancestorcount"] = strNv;
                ModifyRWConfigFile("limitancestorcount", strNv);
            }
            break;
        }
        case limitancestorsize:
        {
            long long nOldValue = GetArg("-limitancestorsize", DEFAULT_ANCESTOR_SIZE_LIMIT);
            long long nNv = value.toLongLong();
            if (nNv != nOldValue) {
                std::string strNv = value.toString().toStdString();
                mapArgs["-limitancestorsize"] = strNv;
                ModifyRWConfigFile("limitancestorsize", strNv);
            }
            break;
        }
        case limitdescendantcount:
        {
            long long nOldValue = GetArg("-limitdescendantcount", DEFAULT_DESCENDANT_LIMIT);
            long long nNv = value.toLongLong();
            if (nNv != nOldValue) {
                std::string strNv = value.toString().toStdString();
                mapArgs["-limitdescendantcount"] = strNv;
                ModifyRWConfigFile("limitdescendantcount", strNv);
            }
            break;
        }
        case limitdescendantsize:
        {
            long long nOldValue = GetArg("-limitdescendantsize", DEFAULT_DESCENDANT_SIZE_LIMIT);
            long long nNv = value.toLongLong();
            if (nNv != nOldValue) {
                std::string strNv = value.toString().toStdString();
                mapArgs["-limitdescendantsize"] = strNv;
                ModifyRWConfigFile("limitdescendantsize", strNv);
            }
            break;
        }
        case spamfilter:
        {
            bool fOldValue = GetArg("-spamfilter", DEFAULT_SPAMFILTER);
            bool fNewValue = value.toBool();
            if (fOldValue != fNewValue) {
                std::string strNv = strprintf("%d", fNewValue);
                mapArgs["-spamfilter"] = strNv;
                ModifyRWConfigFile("spamfilter", strNv);
            }
            break;
        }
        case rejectbaremultisig:
        {
            // The config and internal option is inverted
            const bool fNewValue = ! value.toBool();
            if (fNewValue != fIsBareMultisigStd) {
                fIsBareMultisigStd = fNewValue;
                ModifyRWConfigFile("permitbaremultisig", strprintf("%d", fNewValue));
            }
            break;
        }
        case datacarriersize:
        {
            const int nNewSize = value.toInt();
            const bool fNewEn = (nNewSize > 0);
            if (fNewEn && unsigned(nNewSize) != nMaxDatacarrierBytes) {
                ModifyRWConfigFile("datacarriersize", value.toString().toStdString());
                nMaxDatacarrierBytes = nNewSize;
            }
            if (fNewEn != fAcceptDatacarrier) {
                ModifyRWConfigFile("datacarrier", strprintf("%d", fNewEn));
                fAcceptDatacarrier = fNewEn;
            }
            break;
        }
        case blockmaxsize:
        case blockprioritysize:
        case blockmaxweight:
        {
            const int nNewValue_kB = value.toInt();
            const int nOldValue_kB = data(index, role).toInt();
            if (nNewValue_kB != nOldValue_kB) {
                std::string strNv = strprintf("%d000", nNewValue_kB);
                std::string strKey;
                switch(index.row()) {
                    case blockmaxsize:
                        strKey = "blockmaxsize";
                        break;
                    case blockprioritysize:
                        strKey = "blockprioritysize";
                        break;
                    case blockmaxweight:
                        strKey = "blockmaxweight";
                        break;
                }
                mapArgs["-" + strKey] = strNv;
                ModifyRWConfigFile(strKey, strNv);
            }
            break;
        }
        case priorityaccurate:
        {
            const bool fNewValue = value.toBool();
            if (fNewValue != fPriorityAccurate) {
                fPriorityAccurate = fNewValue;
                ModifyRWConfigFile("priorityaccurate", strprintf("%d", fNewValue));
            }
            break;
        }
        case corepolicy:
            ModifyRWConfigFile("corepolicy", value.toString().toStdString());
            break;
        default:
            break;
        }
    }

    Q_EMIT dataChanged(index, index);

    return successful;
}

/** Updates current unit in memory, settings and emits displayUnitChanged(newUnit) signal */
void OptionsModel::setDisplayUnit(const QVariant &value)
{
    if (!value.isNull())
    {
        QSettings settings;
        nDisplayUnit = value.toInt();
        settings.setValue("nDisplayUnit", nDisplayUnit);
        Q_EMIT displayUnitChanged(nDisplayUnit);
    }
}

bool OptionsModel::getProxySettings(QNetworkProxy& proxy) const
{
    // Directly query current base proxy, because
    // GUI settings can be overridden with -proxy.
    proxyType curProxy;
    if (GetProxy(NET_IPV4, curProxy)) {
        proxy.setType(QNetworkProxy::Socks5Proxy);
        proxy.setHostName(QString::fromStdString(curProxy.proxy.ToStringIP()));
        proxy.setPort(curProxy.proxy.GetPort());

        return true;
    }
    else
        proxy.setType(QNetworkProxy::NoProxy);

    return false;
}

void OptionsModel::setRestartRequired(bool fRequired)
{
    QSettings settings;
    return settings.setValue("fRestartRequired", fRequired);
}

bool OptionsModel::isRestartRequired()
{
    QSettings settings;
    return settings.value("fRestartRequired", false).toBool();
}

void OptionsModel::checkAndMigrate()
{
    // Migration of default values
    // Check if the QSettings container was already loaded with this client version
    QSettings settings;
    static const char strSettingsVersionKey[] = "nSettingsVersion";
    int settingsVersion = settings.contains(strSettingsVersionKey) ? settings.value(strSettingsVersionKey).toInt() : 0;
    if (settingsVersion < CLIENT_VERSION)
    {
        // -dbcache was bumped from 100 to 300 in 0.13
        // see https://github.com/bitcoin/bitcoin/pull/8273
        // force people to upgrade to the new value if they are using 100MB
        if (settingsVersion < 130000 && settings.contains("nDatabaseCache") && settings.value("nDatabaseCache").toLongLong() == 100)
            settings.setValue("nDatabaseCache", (qint64)nDefaultDbCache);

        settings.setValue(strSettingsVersionKey, CLIENT_VERSION);
    }
}
