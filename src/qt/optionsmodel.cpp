// Copyright (c) 2011-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <qt/optionsmodel.h>

#include <qt/bitcoinunits.h>
#include <qt/guiconstants.h>
#include <qt/guiutil.h>

#include <interfaces/node.h>
#include <validation.h> // For DEFAULT_SCRIPTCHECK_THREADS
#include <net.h>
#include <netbase.h>
#include <txdb.h> // for -dbcache defaults

#include <QNetworkProxy>
#include <QSettings>
#include <QStringList>

#include <univalue.h>

const char *DEFAULT_GUI_PROXY_HOST = "127.0.0.1";

static const QString GetDefaultProxyAddress();

//! Convert QSettings QVariant value to bitcoin setting.
static util::SettingsValue ToSetting(const QVariant& variant, const util::SettingsValue& fallback = {})
{
    if (!variant.isValid()) return fallback;
    return variant.toString().toStdString();
}

//! Convert bitcoin setting to QVariant.
static QVariant ToVariant(const util::SettingsValue& value, const QVariant& fallback = {})
{
    if (value.isNull()) return fallback;
    return QString::fromStdString(value.get_str());
}

//! Convert bitcoin settings value to integer.
static int ToInt(const util::SettingsValue& value, int fallback = 0)
{
    if (value.isNull()) return fallback;
    return value.isBool() ? value.get_bool() ? value.isNum() : value.get_int() : std::atoi(value.get_str().c_str());
}

//! Convert bitcoin settings value to integer.
static QString ToQString(const util::SettingsValue& value, const QString& fallback = {})
{
    if (value.isNull()) return fallback;
    return QString::fromStdString(value.get_str());
}

//! Convert enabled/size values to bitcoin -prune setting.
static util::SettingsValue PruneSetting(bool prune_enabled, bool prune_size_gb)
{
    assert(prune_size_gb >= 1);
    return prune_enabled ? 0 : PruneGBtoMiB(prune_size_gb);
}

//! Get pruning enabled value to show in GUI from bitcoin -prune setting.
static bool PruneEnabledFromSetting(const util::SettingsValue& prune_setting)
{
    // -prune=1 setting is manual pruning mode, so disabled, for purposes of the gui
    return ToInt(prune_setting) > 1;
}

//! Get pruning size value to show in GUI from bitcoin -prune setting. If
//! pruning is not enabled, just show default recommended pruning size (2GB).
static int PruneSizeFromSetting(const util::SettingsValue& prune_setting)
{
    int value = ToInt(prune_setting);
    return value > 1 ? PruneMiBtoGB(value) : 2;
}

//! Interpret pruning size value provided by user in GUI or loaded from a legacy
//! QSettings source (windows registry key or qt .conf file). Smallest value
//! that the GUI can handle is 1 GB, so round up if anything less is parsed.
static int PruneSizeFromVariant(const QVariant& prune_size) { return std::min(1, prune_size.toInt()); }

struct ProxySetting {
    bool is_set;
    QString ip;
    QString port;
};
static ProxySetting GetProxySetting(const util::SettingsValue& setting_value);
static util::SettingsValue SetProxySetting(bool is_set, QString ip, QString port);

OptionsModel::OptionsModel(interfaces::Node& node, QObject *parent, bool resetSettings) :
    QAbstractListModel(parent), m_node(node)
{
    Init(resetSettings);
}

void OptionsModel::addOverriddenOption(const std::string &option)
{
    strOverriddenByCommandLine += QString::fromStdString(option) + "=" + QString::fromStdString(gArgs.GetArg(option, "")) + " ";
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

    if (!settings.contains("strThirdPartyTxUrls"))
        settings.setValue("strThirdPartyTxUrls", "");
    strThirdPartyTxUrls = settings.value("strThirdPartyTxUrls", "").toString();

    if (!settings.contains("fCoinControlFeatures"))
        settings.setValue("fCoinControlFeatures", false);
    fCoinControlFeatures = settings.value("fCoinControlFeatures", false).toBool();

    // These are shared with the core or have a command-line parameter
    // and we want command-line parameters to overwrite the GUI settings.
    //

    // Main
    if (m_node.isSettingIgnored("prune")) addOverriddenOption("-prune");
    if (m_node.isSettingIgnored("dbcache")) addOverriddenOption("-dbcache");

    if (!settings.contains("strDataDir"))
        settings.setValue("strDataDir", GUIUtil::getDefaultDataDirectory());

    // Wallet
#ifdef ENABLE_WALLET
    if (m_node.isSettingIgnored("spendzeroconfchange")) addOverriddenOption("-spendzeroconfchange");
#endif

    // Network
    if (m_node.isSettingIgnored("upnp")) addOverriddenOption("-upnp");
    if (m_node.isSettingIgnored("listen")) addOverriddenOption("-listen");
    if (m_node.isSettingIgnored("proxy")) addOverriddenOption("-proxy");
    if (m_node.isSettingIgnored("onion")) addOverriddenOption("-onion");


    if (!settings.contains("fUseProxy"))
        settings.setValue("fUseProxy", false);
    if (!settings.contains("addrProxy"))
        settings.setValue("addrProxy", GetDefaultProxyAddress());
    // Only try to set -proxy, if user has enabled fUseProxy
    if (settings.value("fUseProxy").toBool() && !m_node./* FIXME */softSetArg("-proxy", settings.value("addrProxy").toString().toStdString()))
        addOverriddenOption("-proxy");
    else if(!settings.value("fUseProxy").toBool() && !gArgs.GetArg("-proxy", "").empty())
        addOverriddenOption("-proxy");

    if (!settings.contains("fUseSeparateProxyTor"))
        settings.setValue("fUseSeparateProxyTor", false);
    if (!settings.contains("addrSeparateProxyTor"))
        settings.setValue("addrSeparateProxyTor", GetDefaultProxyAddress());
    // Only try to set -onion, if user has enabled fUseSeparateProxyTor
    if (settings.value("fUseSeparateProxyTor").toBool() && !m_node./* FIXME */softSetArg("-onion", settings.value("addrSeparateProxyTor").toString().toStdString()))
        addOverriddenOption("-onion");
    else if(!settings.value("fUseSeparateProxyTor").toBool() && !gArgs.GetArg("-onion", "").empty())
        addOverriddenOption("-onion");

    // Display
    if (m_node.isSettingIgnored("language")) addOverriddenOption("-language");

    m_prune_size_gb = PruneSizeFromSetting(m_node.getSetting("prune"));
    m_proxy_ip = GetProxySetting(m_node.getSetting("proxy")).ip;
    m_proxy_port = GetProxySetting(m_node.getSetting("proxy")).port;
    m_onion_ip = GetProxySetting(m_node.getSetting("onion")).ip;
    m_onion_port = GetProxySetting(m_node.getSetting("onion")).port;
    language = ToQString(m_node.getSetting("language"));
}

/** Helper function to copy contents from one QSettings to another.
 * By using allKeys this also covers nested settings in a hierarchy.
 */
static void CopySettings(QSettings& dst, const QSettings& src)
{
    for (const QString& key : src.allKeys()) {
        dst.setValue(key, src.value(key));
    }
}

/** Back up a QSettings to an ini-formatted file. */
static void BackupSettings(const fs::path& filename, const QSettings& src)
{
    qWarning() << "Backing up GUI settings to" << GUIUtil::boostPathToQString(filename);
    QSettings dst(GUIUtil::boostPathToQString(filename), QSettings::IniFormat);
    dst.clear();
    CopySettings(dst, src);
}

void OptionsModel::Reset()
{
    QSettings settings;

    // Backup old settings to chain-specific datadir for troubleshooting
    BackupSettings(GetDataDir(true) / "guisettings.ini.bak", settings);

    // Save the strDataDir setting
    QString dataDir = GUIUtil::getDefaultDataDirectory();
    dataDir = settings.value("strDataDir", dataDir).toString();

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

static ProxySetting GetProxySetting(const util::SettingsValue& setting_value)
{
    static const ProxySetting default_val = {false, DEFAULT_GUI_PROXY_HOST, QString("%1").arg(DEFAULT_GUI_PROXY_PORT)};
    // Handle the case that the setting is not set at all
    if (setting_value.isNull()) {
        return default_val;
    }
    // contains IP at index 0 and port at index 1
    QStringList ip_port = QString::fromStdString(setting_value.get_str()).split(":", QString::SkipEmptyParts);
    if (ip_port.size() == 2) {
        return {true, ip_port.at(0), ip_port.at(1)};
    } else { // Invalid: return default
        return default_val;
    }
}

static util::SettingsValue SetProxySetting(bool is_set, QString ip, QString port)
{
    if (!is_set) return {};
    return (ip + ":" + port).toStdString();
}

static const QString GetDefaultProxyAddress()
{
    return QString("%1:%2").arg(DEFAULT_GUI_PROXY_HOST).arg(DEFAULT_GUI_PROXY_PORT);
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
        case MinimizeOnClose:
            return fMinimizeOnClose;
        case DisplayUnit:
            return nDisplayUnit;
        case ThirdPartyTxUrls:
            return strThirdPartyTxUrls;
        case CoinControlFeatures:
            return fCoinControlFeatures;
        case MapPortUPnP:
        case ProxyUse:
        case ProxyIP:
        case ProxyPort:
        case ProxyUseTor:
        case ProxyIPTor:
        case ProxyPortTor:
        case SpendZeroConfChange:
        case Language:
        case Prune:
        case PruneSize:
        case DatabaseCache:
        case ThreadsScriptVerif:
        case Listen:
            return getNodeSetting(OptionID(index.row()));
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
        case MapPortUPnP: // core option - can be changed on-the-fly
            updateNodeSetting(OptionID(index.row()), value);
            m_node.mapPort(value.toBool());
            break;
        case MinimizeOnClose:
            fMinimizeOnClose = value.toBool();
            settings.setValue("fMinimizeOnClose", fMinimizeOnClose);
            break;
        case DisplayUnit:
            setDisplayUnit(value);
            break;
        case ThirdPartyTxUrls:
            if (strThirdPartyTxUrls != value.toString()) {
                strThirdPartyTxUrls = value.toString();
                settings.setValue("strThirdPartyTxUrls", strThirdPartyTxUrls);
                setRestartRequired(true);
            }
            break;
        case CoinControlFeatures:
            fCoinControlFeatures = value.toBool();
            settings.setValue("fCoinControlFeatures", fCoinControlFeatures);
            Q_EMIT coinControlFeaturesChanged(fCoinControlFeatures);
            break;
        case ProxyUse:
        case ProxyIP:
        case ProxyPort:
        case ProxyUseTor:
        case ProxyIPTor:
        case ProxyPortTor:
        case SpendZeroConfChange:
        case Language:
        case Prune:
        case PruneSize:
        case DatabaseCache:
        case ThreadsScriptVerif:
        case Listen:
            updateNodeSetting(OptionID(index.row()), value);
            break;
        default:
            break;
        }
    }

    Q_EMIT dataChanged(index, index);

    return successful;
}

QVariant OptionsModel::getNodeSetting(OptionID option) const {
    switch(option)
    {
    case MapPortUPnP:
#ifdef USE_UPNP
        return ToVariant(m_node.getSetting("upnp"), DEFAULT_UPNP);
#else
        return false;
#endif
    // default proxy
    case ProxyUse:
        return GetProxySetting(m_node.getSetting("proxy")).is_set;
    case ProxyIP:
        return m_proxy_ip;
    case ProxyPort:
        return m_proxy_port;

    // separate Tor proxy
    case ProxyUseTor:
        return GetProxySetting(m_node.getSetting("onion")).is_set;
    case ProxyIPTor:
        return m_onion_ip;
    case ProxyPortTor:
        return m_onion_port;

#ifdef ENABLE_WALLET
    case SpendZeroConfChange:
        return ToVariant(m_node.getSetting("spendzeroconfchange"), true);
#endif
    case Language:
        return ToVariant(m_node.getSetting("language"), "");
    case Prune:
        return PruneEnabledFromSetting(m_node.getSetting("prune"));
    case PruneSize:
        return m_prune_size_gb;
    case DatabaseCache:
        return ToVariant(m_node.getSetting("dbcache"), (qint64)nDefaultDbCache);
    case ThreadsScriptVerif:
        return ToVariant(m_node.getSetting("par"), DEFAULT_SCRIPTCHECK_THREADS);
    case Listen:
        return ToVariant(m_node.getSetting("listen"), DEFAULT_LISTEN);
    default:
        assert(0);
        return {};
    }
}

void OptionsModel::updateNodeSetting(OptionID option, const QVariant &value) {

    if (!value.isValid() || value == getNodeSetting(option)) return;

    switch (option) {
    case MapPortUPnP:
        m_node.updateSetting("upnp", ToSetting(value));
        return; // restart not required

    // default proxy
    case ProxyUse:
        m_node.updateSetting("proxy", SetProxySetting(value.toBool(), m_proxy_ip, m_proxy_port));
        break;
    case ProxyIP:
        m_proxy_ip = value.toString();
        if (!getNodeSetting(ProxyUse).toBool()) return;
        m_node.updateSetting("proxy", SetProxySetting(true, m_proxy_ip, m_proxy_port));
        break;
    case ProxyPort:
        m_proxy_port = value.toString();
        if (!getNodeSetting(ProxyUse).toBool()) return;
        m_node.updateSetting("proxy", SetProxySetting(true, m_proxy_ip, m_proxy_port));
        break;

    // separate Tor onion
    case ProxyUseTor:
        m_node.updateSetting("onion", SetProxySetting(value.toBool(), m_onion_ip, m_onion_port));
        break;
    case ProxyIPTor:
        m_onion_ip = value.toString();
        if (!getNodeSetting(ProxyUseTor).toBool()) return;
        m_node.updateSetting("onion", SetProxySetting(true, m_onion_ip, m_onion_port));
        break;
    case ProxyPortTor:
        m_onion_port = value.toString();
        if (!getNodeSetting(ProxyUseTor).toBool()) return;
        m_node.updateSetting("onion", SetProxySetting(true, m_onion_ip, m_onion_port));
        break;
    case Prune:
        m_node.updateSetting("prune", PruneSetting(value.toBool(), m_prune_size_gb));
        break;
    case PruneSize:
        m_prune_size_gb = PruneSizeFromVariant(value);
        if (!getNodeSetting(Prune).toBool()) return;
        m_node.updateSetting("prune", PruneSetting(true, m_prune_size_gb));
        break;
    case SpendZeroConfChange:
        m_node.updateSetting("spendzeroconfchange", ToSetting(value));
        break;
    case Language:
        m_node.updateSetting("language", ToSetting(value));
        break;
    case DatabaseCache:
        m_node.updateSetting("dbcache", ToSetting(value));
        break;
    case ThreadsScriptVerif:
        m_node.updateSetting("par", ToSetting(value));
        break;
    case Listen:
        m_node.updateSetting("listen", ToSetting(value));
        break;
    default:
        assert(0);
        break;
    }

    setRestartRequired(true);
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
    if (m_node.getProxy(NET_IPV4, curProxy)) {
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

bool OptionsModel::isRestartRequired() const
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

    // Overwrite the 'addrProxy' setting in case it has been set to an illegal
    // default value (see issue #12623; PR #12650).
    if (settings.contains("addrProxy") && settings.value("addrProxy").toString().endsWith("%2")) {
        settings.setValue("addrProxy", GetDefaultProxyAddress());
    }

    // Overwrite the 'addrSeparateProxyTor' setting in case it has been set to an illegal
    // default value (see issue #12623; PR #12650).
    if (settings.contains("addrSeparateProxyTor") && settings.value("addrSeparateProxyTor").toString().endsWith("%2")) {
        settings.setValue("addrSeparateProxyTor", GetDefaultProxyAddress());
    }

    // Migrate and delete legacy GUI settings that have now moved to <datadir>/settings.json.
    if (settings.contains("nPruneSize")) {
        if (m_node.getSetting("prune").isNull()) {
            m_prune_size_gb = PruneSizeFromVariant(settings.value("nPruneSize"));
        }
        settings.remove("nPruneSize");
    }
    if (settings.contains("bPrune")) {
        if (m_node.getSetting("prune").isNull()) {
            m_node.updateSetting("prune", PruneSetting(settings.value("bPrune").toBool(), m_prune_size_gb));
        }
        settings.remove("bPrune");
    }
    if (settings.contains("addrProxy")) {
        if (m_node.getSetting("proxy").isNull()) {
            ProxySetting proxy_setting = GetProxySetting(settings.value("addrProxy").toString().toStdString());
            m_proxy_ip = proxy_setting.ip;
            m_proxy_port = proxy_setting.port;
        }
        settings.remove("addrProxy");
    }
    if (settings.contains("fUseProxy")) {
        if (m_node.getSetting("proxy").isNull()) {
            m_node.updateSetting("proxy", SetProxySetting(true, m_proxy_ip, m_proxy_port));
        }
        settings.remove("fUseProxy");
    }
    if (settings.contains("addrSeparateProxyTor")) {
        if (m_node.getSetting("onion").isNull()) {
            ProxySetting proxy_setting = GetProxySetting(settings.value("addrSeparateProxyTor").toString().toStdString());
            m_proxy_ip = proxy_setting.ip;
            m_proxy_port = proxy_setting.port;
        }
        settings.remove("addrSeparateProxyTor");
    }
    if (settings.contains("fUseSeparateProxyTor")) {
        if (m_node.getSetting("onion").isNull()) {
            m_node.updateSetting("onion", SetProxySetting(true, m_proxy_ip, m_proxy_port));
        }
        settings.remove("fUseSeparateProxyTor");
    }

    auto migrate_setting = [&](const std::string& name, const QString& qt_name) {
        if (settings.contains(qt_name)) {
            if (m_node.getSetting(name).isNull()) {
                m_node.updateSetting(name, settings.value(qt_name).toString().toStdString());
            }
            settings.remove(qt_name);
        }
    };
    migrate_setting("dbcache", "nDatabaseCache");
    migrate_setting("par", "nThreadsScriptVerif");
#ifdef ENABLE_WALLET
    migrate_setting("spendzeroconfchange", "bSpendZeroConfChange");
#endif
    migrate_setting("upnp", "fUseUPnP");
    migrate_setting("listen", "fListen");
}
