#include "ipc/capnp/serialize.h"

#include "clientversion.h"
#include "key.h"
#include "net.h"
#include "net_processing.h"
#include "netbase.h"
#include "wallet/coincontrol.h"

#include <univalue.h>

namespace ipc {
namespace capnp {

void BuildProxy(messages::Proxy::Builder&& builder, const proxyType& proxy)
{
    builder.setProxy(ToArray(Serialize(proxy.proxy)));
    builder.setRandomizeCredentials(proxy.randomize_credentials);
}

void ReadProxy(proxyType& proxy, const messages::Proxy::Reader& reader)
{
    Unserialize(proxy.proxy, reader.getProxy());
    proxy.randomize_credentials = reader.getRandomizeCredentials();
}

void BuildNodeStats(messages::NodeStats::Builder&& builder, const CNodeStats& nodeStats)
{
    builder.setNodeid(nodeStats.nodeid);
    builder.setServices(nodeStats.nServices);
    builder.setRelayTxes(nodeStats.fRelayTxes);
    builder.setLastSend(nodeStats.nLastSend);
    builder.setLastRecv(nodeStats.nLastRecv);
    builder.setTimeConnected(nodeStats.nTimeConnected);
    builder.setTimeOffset(nodeStats.nTimeOffset);
    builder.setAddrName(nodeStats.addrName);
    builder.setVersion(nodeStats.nVersion);
    builder.setCleanSubVer(nodeStats.cleanSubVer);
    builder.setInbound(nodeStats.fInbound);
    builder.setAddnode(nodeStats.fAddnode);
    builder.setStartingHeight(nodeStats.nStartingHeight);
    builder.setSendBytes(nodeStats.nSendBytes);
    auto resultSend = builder.initSendBytesPerMsgCmd().initEntries(nodeStats.mapSendBytesPerMsgCmd.size());
    size_t i = 0;
    for (const auto& entry : nodeStats.mapSendBytesPerMsgCmd) {
        resultSend[i].setKey(entry.first);
        resultSend[i].setValue(entry.second);
        ++i;
    }
    builder.setRecvBytes(nodeStats.nRecvBytes);
    auto resultRecv = builder.initRecvBytesPerMsgCmd().initEntries(nodeStats.mapRecvBytesPerMsgCmd.size());
    i = 0;
    for (const auto& entry : nodeStats.mapRecvBytesPerMsgCmd) {
        resultRecv[i].setKey(entry.first);
        resultRecv[i].setValue(entry.second);
        ++i;
    }
    builder.setWhitelisted(nodeStats.fWhitelisted);
    builder.setPingTime(nodeStats.dPingTime);
    builder.setPingWait(nodeStats.dPingWait);
    builder.setMinPing(nodeStats.dMinPing);
    builder.setAddrLocal(nodeStats.addrLocal);
    builder.setAddr(ToArray(Serialize(nodeStats.addr)));
}

void ReadNodeStats(CNodeStats& nodeStats, const messages::NodeStats::Reader& reader)
{
    nodeStats.nodeid = NodeId(reader.getNodeid());
    nodeStats.nServices = ServiceFlags(reader.getServices());
    nodeStats.fRelayTxes = reader.getRelayTxes();
    nodeStats.nLastSend = reader.getLastSend();
    nodeStats.nLastRecv = reader.getLastRecv();
    nodeStats.nTimeConnected = reader.getTimeConnected();
    nodeStats.nTimeOffset = reader.getTimeOffset();
    nodeStats.addrName = ToString(reader.getAddrName());
    nodeStats.nVersion = reader.getVersion();
    nodeStats.cleanSubVer = ToString(reader.getCleanSubVer());
    nodeStats.fInbound = reader.getInbound();
    nodeStats.fAddnode = reader.getAddnode();
    nodeStats.nStartingHeight = reader.getStartingHeight();
    nodeStats.nSendBytes = reader.getSendBytes();
    for (const auto& entry : reader.getSendBytesPerMsgCmd().getEntries()) {
        nodeStats.mapSendBytesPerMsgCmd[ToString(entry.getKey())] = entry.getValue();
    }
    nodeStats.nRecvBytes = reader.getRecvBytes();
    for (const auto& entry : reader.getRecvBytesPerMsgCmd().getEntries()) {
        nodeStats.mapRecvBytesPerMsgCmd[ToString(entry.getKey())] = entry.getValue();
    }
    nodeStats.fWhitelisted = reader.getWhitelisted();
    nodeStats.dPingTime = reader.getPingTime();
    nodeStats.dPingWait = reader.getPingWait();
    nodeStats.dMinPing = reader.getMinPing();
    nodeStats.addrLocal = ToString(reader.getAddrLocal());
    Unserialize(nodeStats.addr, reader.getAddr());
}

void BuildNodeStateStats(messages::NodeStateStats::Builder&& builder, const CNodeStateStats& nodeStateStats)
{
    builder.setMisbehavior(nodeStateStats.nMisbehavior);
    builder.setSyncHeight(nodeStateStats.nSyncHeight);
    builder.setCommonHeight(nodeStateStats.nCommonHeight);
    auto heights = builder.initHeightInFlight(nodeStateStats.vHeightInFlight.size());
    size_t i = 0;
    for (int height : nodeStateStats.vHeightInFlight) {
        heights.set(i, height);
        ++i;
    }
}

void ReadNodeStateStats(CNodeStateStats& nodeStateStats, const messages::NodeStateStats::Reader& reader)
{
    nodeStateStats.nMisbehavior = reader.getMisbehavior();
    nodeStateStats.nSyncHeight = reader.getSyncHeight();
    nodeStateStats.nCommonHeight = reader.getCommonHeight();
    for (int height : reader.getHeightInFlight()) {
        nodeStateStats.vHeightInFlight.emplace_back(height);
    }
}

void BuildBanMap(messages::BanMap::Builder&& builder, const banmap_t& banMap)
{
    auto resultEntries = builder.initEntries(banMap.size());
    size_t i = 0;
    for (const auto& entry : banMap) {
        resultEntries[i].setSubnet(ToArray(Serialize(entry.first)));
        resultEntries[i].setBanEntry(ToArray(Serialize(entry.first)));
        ++i;
    }
}
void ReadBanMap(banmap_t& banMap, const messages::BanMap::Reader& reader)
{
    for (const auto& entry : reader.getEntries()) {
        banMap.emplace(Unserialize<CSubNet>(entry.getSubnet()), Unserialize<CBanEntry>(entry.getBanEntry()));
    }
}

void BuildUniValue(messages::UniValue::Builder&& builder, const UniValue& uniValue)
{
    builder.setType(uniValue.getType());
    if (uniValue.getType() == UniValue::VARR || uniValue.getType() == UniValue::VOBJ) {
        builder.setValue(uniValue.write());
    } else {
        builder.setValue(uniValue.getValStr());
    }
}

void ReadUniValue(UniValue& uniValue, const messages::UniValue::Reader& reader)
{
    if (reader.getType() == UniValue::VARR || reader.getType() == UniValue::VOBJ) {
        if (!uniValue.read(ToString(reader.getValue()))) {
            throw std::runtime_error("Could not parse UniValue");
        }
    } else {
        uniValue = UniValue(UniValue::VType(reader.getType()), ToString(reader.getValue()));
    }
}

void BuildWalletValueMap(messages::WalletValueMap::Builder&& builder, const WalletValueMap& valueMap)
{
    auto resultEntries = builder.initEntries(valueMap.size());
    size_t i = 0;
    for (const auto& entry : valueMap) {
        resultEntries[i].setKey(entry.first);
        resultEntries[i].setValue(entry.second);
        ++i;
    }
}

void ReadWalletValueMap(WalletValueMap& valueMap, const messages::WalletValueMap::Reader& reader)
{
    valueMap.clear();
    for (const auto& entry : reader.getEntries()) {
        valueMap.emplace(ToString(entry.getKey()), ToString(entry.getValue()));
    }
}

void BuildWalletOrderForm(messages::WalletOrderForm::Builder&& builder, const WalletOrderForm& orderForm)
{
    auto resultEntries = builder.initEntries(orderForm.size());
    size_t i = 0;
    for (const auto& entry : orderForm) {
        resultEntries[i].setKey(entry.first);
        resultEntries[i].setValue(entry.second);
        ++i;
    }
}

void ReadWalletOrderForm(WalletOrderForm& orderForm, const messages::WalletOrderForm::Reader& reader)
{
    orderForm.clear();
    for (const auto& entry : reader.getEntries()) {
        orderForm.emplace_back(ToString(entry.getKey()), ToString(entry.getValue()));
    }
}

void BuildTxDestination(messages::TxDestination::Builder&& builder, const CTxDestination& dest)
{
    if (const CKeyID* keyId = boost::get<CKeyID>(&dest)) {
        builder.setKeyId(ToArray(Serialize(*keyId)));
    } else if (const CScriptID* scriptId = boost::get<CScriptID>(&dest)) {
        builder.setScriptId(ToArray(Serialize(*scriptId)));
    }
}

void ReadTxDestination(CTxDestination& dest, const messages::TxDestination::Reader& reader)
{
    if (reader.hasKeyId()) {
        dest = Unserialize<CKeyID>(reader.getKeyId());
    } else if (reader.hasScriptId()) {
        dest = Unserialize<CScriptID>(reader.getScriptId());
    }
}

void BuildKey(messages::Key::Builder&& builder, const CKey& key)
{
    builder.setSecret(FromBlob(key));
    builder.setIsCompressed(key.IsCompressed());
}

void ReadKey(CKey& key, const messages::Key::Reader& reader)
{
    auto secret = reader.getSecret();
    key.Set(secret.begin(), secret.end(), reader.getIsCompressed());
}

void BuildCoinControl(messages::CoinControl::Builder&& builder, const CCoinControl& coinControl)
{
    BuildTxDestination(builder.initDestChange(), coinControl.destChange);
    builder.setAllowOtherInputs(coinControl.fAllowOtherInputs);
    builder.setAllowWatchOnly(coinControl.fAllowWatchOnly);
    builder.setOverrideFeeRate(coinControl.fOverrideFeeRate);
    builder.setFeeRate(ToArray(Serialize(coinControl.nFeeRate)));
    builder.setConfirmTarget(coinControl.nConfirmTarget);
    builder.setSignalRbf(coinControl.signalRbf);
    std::vector<COutPoint> selected;
    coinControl.ListSelected(selected);
    auto resultSelected = builder.initSetSelected(selected.size());
    size_t i = 0;
    for (const COutPoint& output : selected) {
        resultSelected.set(i, ToArray(Serialize(output)));
        ++i;
    }
}

void ReadCoinControl(CCoinControl& coinControl, const messages::CoinControl::Reader& reader)
{
    ReadTxDestination(coinControl.destChange, reader.getDestChange());
    coinControl.fAllowOtherInputs = reader.getAllowOtherInputs();
    coinControl.fAllowWatchOnly = reader.getAllowWatchOnly();
    coinControl.fOverrideFeeRate = reader.getOverrideFeeRate();
    Unserialize(coinControl.nFeeRate, reader.getFeeRate());
    coinControl.nConfirmTarget = reader.getConfirmTarget();
    coinControl.signalRbf = reader.getSignalRbf();
    for (const auto output : reader.getSetSelected()) {
        coinControl.Select(Unserialize<COutPoint>(output));
    }
}

void BuildCoinsList(messages::CoinsList::Builder&& builder, const Wallet::CoinsList& coinsList)
{
    auto resultEntries = builder.initEntries(coinsList.size());
    size_t i = 0;
    for (const auto& entry : coinsList) {
        BuildTxDestination(resultEntries[i].initDest(), entry.first);
        auto resultCoins = resultEntries[i].initCoins(entry.second.size());
        size_t j = 0;
        for (const auto& coin : entry.second) {
            resultCoins[i].setOutput(ToArray(Serialize(std::get<0>(coin))));
            BuildWalletTxOut(resultCoins[i].initTxOut(), std::get<1>(coin));
            ++j;
        }
        ++i;
    }
}

void ReadCoinsList(Wallet::CoinsList& coinsList, const messages::CoinsList::Reader& reader)
{
    coinsList.clear();
    for (const auto& entry : reader.getEntries()) {
        CTxDestination dest;
        ReadTxDestination(dest, entry.getDest());
        auto& coins = coinsList[dest];
        coins.reserve(entry.getCoins().size());
        for (const auto& coin : entry.getCoins()) {
            coins.emplace_back();
            Unserialize(std::get<0>(coins.back()), coin.getOutput());
            ReadWalletTxOut(std::get<1>(coins.back()), coin.getTxOut());
        }
    }
}

void BuildRecipient(messages::Recipient::Builder&& builder, const CRecipient& recipient)
{
    builder.setScriptPubKey(ToArray(recipient.scriptPubKey));
    builder.setAmount(recipient.nAmount);
    builder.setSubtractFeeFromAmount(recipient.fSubtractFeeFromAmount);
}

void ReadRecipient(CRecipient& recipient, const messages::Recipient::Reader& reader)
{
    recipient.scriptPubKey = ToBlob<CScript>(reader.getScriptPubKey());
    recipient.nAmount = reader.getAmount();
    recipient.fSubtractFeeFromAmount = reader.getSubtractFeeFromAmount();
}

void BuildWalletAddress(messages::WalletAddress::Builder&& builder, const WalletAddress& address)
{
    BuildTxDestination(builder.initDest(), address.dest);
    builder.setIsMine(address.isMine);
    builder.setName(address.name);
    builder.setPurpose(address.purpose);
}

void ReadWalletAddress(WalletAddress& address, const messages::WalletAddress::Reader& reader)
{
    ReadTxDestination(address.dest, reader.getDest());
    address.isMine = isminetype(reader.getIsMine());
    address.name = reader.getName();
    address.purpose = reader.getPurpose();
}

void BuildWalletBalances(messages::WalletBalances::Builder&& builder, const WalletBalances& balances)
{
    builder.setBalance(balances.balance);
    builder.setUnconfirmedBalance(balances.unconfirmedBalance);
    builder.setImmatureBalance(balances.immatureBalance);
    builder.setHaveWatchOnly(balances.haveWatchOnly);
    builder.setWatchOnlyBalance(balances.watchOnlyBalance);
    builder.setUnconfirmedWatchOnlyBalance(balances.unconfirmedWatchOnlyBalance);
    builder.setImmatureWatchOnlyBalance(balances.immatureWatchOnlyBalance);
}

void ReadWalletBalances(WalletBalances& balances, const messages::WalletBalances::Reader& reader)
{
    balances.balance = reader.getBalance();
    balances.unconfirmedBalance = reader.getUnconfirmedBalance();
    balances.immatureBalance = reader.getImmatureBalance();
    balances.haveWatchOnly = reader.getHaveWatchOnly();
    balances.watchOnlyBalance = reader.getWatchOnlyBalance();
    balances.unconfirmedWatchOnlyBalance = reader.getUnconfirmedWatchOnlyBalance();
    balances.immatureWatchOnlyBalance = reader.getImmatureWatchOnlyBalance();
}

void BuildWalletTx(messages::WalletTx::Builder&& builder, const WalletTx& walletTx)
{
    if (walletTx.tx) {
        builder.setTx(ToArray(Serialize(*walletTx.tx)));
    }

    size_t i = 0;
    auto resultTxInIsMine = builder.initTxInIsMine(walletTx.txInIsMine.size());
    for (const auto& ismine : walletTx.txInIsMine) {
        resultTxInIsMine.set(i, ismine);
        ++i;
    }

    i = 0;
    auto resultTxOutIsMine = builder.initTxOutIsMine(walletTx.txOutIsMine.size());
    for (const auto& ismine : walletTx.txOutIsMine) {
        resultTxOutIsMine.set(i, ismine);
        ++i;
    }

    i = 0;
    auto resultTxOutAddress = builder.initTxOutAddress(walletTx.txOutAddress.size());
    for (const auto& address : walletTx.txOutAddress) {
        BuildTxDestination(resultTxOutAddress[i], address);
        ++i;
    }

    i = 0;
    auto resultTxOutAddressIsMine = builder.initTxOutAddressIsMine(walletTx.txOutAddressIsMine.size());
    for (const auto& ismine : walletTx.txOutAddressIsMine) {
        resultTxOutAddressIsMine.set(i, ismine);
        ++i;
    }

    builder.setCredit(walletTx.credit);
    builder.setDebit(walletTx.debit);
    builder.setChange(walletTx.change);
    builder.setTxTime(walletTx.txTime);
    BuildWalletValueMap(builder.initMapValue(), walletTx.mapValue);
    builder.setIsCoinBase(walletTx.isCoinBase);
}

void ReadWalletTx(WalletTx& walletTx, const messages::WalletTx::Reader& reader)
{
    if (reader.hasTx()) {
        // TODO dedup
        auto data = reader.getTx();
        CDataStream stream(reinterpret_cast<const char*>(data.begin()), reinterpret_cast<const char*>(data.end()),
            SER_NETWORK, CLIENT_VERSION);
        walletTx.tx = std::make_shared<CTransaction>(deserialize, stream);
    }

    walletTx.txInIsMine.clear();
    walletTx.txInIsMine.reserve(reader.getTxInIsMine().size());
    for (const auto& ismine : reader.getTxInIsMine()) {
        walletTx.txInIsMine.emplace_back(isminetype(ismine));
    }

    walletTx.txOutIsMine.clear();
    walletTx.txOutIsMine.reserve(reader.getTxOutIsMine().size());
    for (const auto& ismine : reader.getTxOutIsMine()) {
        walletTx.txOutIsMine.emplace_back(isminetype(ismine));
    }

    walletTx.txOutAddress.clear();
    walletTx.txOutAddress.reserve(reader.getTxOutAddress().size());
    for (const auto& address : reader.getTxOutAddress()) {
        walletTx.txOutAddress.emplace_back();
        ReadTxDestination(walletTx.txOutAddress.back(), address);
    }

    walletTx.txOutAddressIsMine.clear();
    walletTx.txOutAddressIsMine.reserve(reader.getTxOutAddressIsMine().size());
    for (const auto& ismine : reader.getTxOutAddressIsMine()) {
        walletTx.txOutAddressIsMine.emplace_back(isminetype(ismine));
    }

    walletTx.credit = reader.getCredit();
    walletTx.debit = reader.getDebit();
    walletTx.change = reader.getChange();
    walletTx.txTime = reader.getTxTime();
    ReadWalletValueMap(walletTx.mapValue, reader.getMapValue());
    walletTx.isCoinBase = reader.getIsCoinBase();
}

void BuildWalletTxOut(messages::WalletTxOut::Builder&& builder, const WalletTxOut& txOut)
{
    builder.setTxOut(ToArray(Serialize(txOut.txOut)));
    builder.setTxTime(txOut.txTime);
    builder.setDepthInMainChain(txOut.depthInMainChain);
    builder.setIsSpent(txOut.isSpent);
}

void ReadWalletTxOut(WalletTxOut& txOut, const messages::WalletTxOut::Reader& reader)
{
    Unserialize(txOut.txOut, reader.getTxOut());
    txOut.txTime = reader.getTxTime();
    txOut.depthInMainChain = reader.getDepthInMainChain();
    txOut.isSpent = reader.getIsSpent();
}

void BuildWalletTxStatus(messages::WalletTxStatus::Builder&& builder, const WalletTxStatus& status)
{
    builder.setBlockHeight(status.blockHeight);
    builder.setBlocksToMaturity(status.blocksToMaturity);
    builder.setDepthInMainChain(status.depthInMainChain);
    builder.setRequestCount(status.requestCount);
    builder.setTimeReceived(status.timeReceived);
    builder.setLockTime(status.lockTime);
    builder.setIsFinal(status.isFinal);
    builder.setIsTrusted(status.isTrusted);
    builder.setIsAbandoned(status.isAbandoned);
    builder.setIsCoinBase(status.isCoinBase);
    builder.setIsInMainChain(status.isInMainChain);
}

void ReadWalletTxStatus(WalletTxStatus& status, const messages::WalletTxStatus::Reader& reader)
{
    status.blockHeight = reader.getBlockHeight();
    status.blocksToMaturity = reader.getBlocksToMaturity();
    status.depthInMainChain = reader.getDepthInMainChain();
    status.requestCount = reader.getRequestCount();
    status.timeReceived = reader.getTimeReceived();
    status.lockTime = reader.getLockTime();
    status.isFinal = reader.getIsFinal();
    status.isTrusted = reader.getIsTrusted();
    status.isAbandoned = reader.getIsAbandoned();
    status.isCoinBase = reader.getIsCoinBase();
    status.isInMainChain = reader.getIsInMainChain();
}

} // namespace capnp
} // namespace ipc
