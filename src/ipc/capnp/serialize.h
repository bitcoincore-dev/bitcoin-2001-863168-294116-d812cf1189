#ifndef BITCOIN_IPC_CAPNP_SERIALIZE_H
#define BITCOIN_IPC_CAPNP_SERIALIZE_H

#include "clientversion.h"
#include "ipc/capnp/messages.capnp.h"
#include "ipc/interfaces.h"
#include "streams.h"
#include "support/allocators/secure.h"

#include <kj/string.h>
#include <string>

namespace ipc {
namespace capnp {

//!@{
//! Functions to serialize / deserialize bitcoin objects that don't
//! already provide their own serialization.
void BuildProxy(messages::Proxy::Builder&& builder, const proxyType& proxy);
void ReadProxy(proxyType& proxy, const messages::Proxy::Reader& reader);
void BuildNodeStats(messages::NodeStats::Builder&& builder, const CNodeStats& nodeStats);
void ReadNodeStats(CNodeStats& nodeStats, const messages::NodeStats::Reader& reader);
void BuildNodeStateStats(messages::NodeStateStats::Builder&& builder, const CNodeStateStats& nodeStateStats);
void ReadNodeStateStats(CNodeStateStats& nodeStateStats, const messages::NodeStateStats::Reader& reader);
void BuildBanMap(messages::BanMap::Builder&& builder, const banmap_t& banMap);
void ReadBanMap(banmap_t& banMap, const messages::BanMap::Reader& reader);
void BuildWalletValueMap(messages::WalletValueMap::Builder&& builder, const WalletValueMap& valueMap);
void ReadWalletValueMap(WalletValueMap& valueMap, const messages::WalletValueMap::Reader& reader);
void BuildWalletOrderForm(messages::WalletOrderForm::Builder&& builder, const WalletOrderForm& orderForm);
void ReadWalletOrderForm(WalletOrderForm& orderForm, const messages::WalletOrderForm::Reader& reader);
void BuildTxDestination(messages::TxDestination::Builder&& builder, const CTxDestination& dest);
void ReadTxDestination(CTxDestination& dest, const messages::TxDestination::Reader& reader);
void BuildKey(messages::Key::Builder&& builder, const CKey& key);
void ReadKey(CKey& key, const messages::Key::Reader& reader);
void BuildCoinControl(messages::CoinControl::Builder&& builder, const CCoinControl& coinControl);
void ReadCoinControl(CCoinControl& coinControl, const messages::CoinControl::Reader& reader);
void BuildCoinsList(messages::CoinsList::Builder&& builder, const Wallet::CoinsList& coinsList);
void ReadCoinsList(Wallet::CoinsList& coinsList, const messages::CoinsList::Reader& reader);
void BuildRecipient(messages::Recipient::Builder&& builder, const CRecipient& recipient);
void ReadRecipient(CRecipient& recipient, const messages::Recipient::Reader& reader);
void BuildWalletAddress(messages::WalletAddress::Builder&& builder, const WalletAddress& address);
void ReadWalletAddress(WalletAddress& address, const messages::WalletAddress::Reader& reader);
void BuildWalletBalances(messages::WalletBalances::Builder&& builder, const WalletBalances& balances);
void ReadWalletBalances(WalletBalances& balances, const messages::WalletBalances::Reader& reader);
void BuildWalletTx(messages::WalletTx::Builder&& builder, const WalletTx& walletTx);
void ReadWalletTx(WalletTx& walletTx, const messages::WalletTx::Reader& reader);
void BuildWalletTxOut(messages::WalletTxOut::Builder&& builder, const WalletTxOut& txOut);
void ReadWalletTxOut(WalletTxOut& txOut, const messages::WalletTxOut::Reader& reader);
void BuildWalletTxStatus(messages::WalletTxStatus::Builder&& builder, const WalletTxStatus& status);
void ReadWalletTxStatus(WalletTxStatus& status, const messages::WalletTxStatus::Reader& reader);
//!@}

//! Convert kj::StringPtr to std::string.
inline std::string ToString(const kj::StringPtr& str) { return {str.cStr(), str.size()}; }

//! Convert kj::ArrayPtr to std::string.
inline std::string ToString(const kj::ArrayPtr<const kj::byte>& data)
{
    return {reinterpret_cast<const char*>(data.begin()), data.size()};
}

//! Convert kj::String to SecureString.
//! TODO: Make secure.
inline SecureString ToSecureString(const kj::ArrayPtr<const kj::byte>& data)
{
    return {(const char*)data.begin(), data.size()};
}

//! Convert array object to kj::ArrayPtr.
template <typename Array>
inline kj::ArrayPtr<const kj::byte> ToArray(const Array& array)
{
    return {reinterpret_cast<const kj::byte*>(array.data()), array.size()};
}

//! Convert base_blob to kj::ArrayPtr.
template <typename Blob>
inline kj::ArrayPtr<const kj::byte> FromBlob(const Blob& blob)
{
    return {blob.begin(), blob.size()};
}

//! Convert kj::ArrayPtr to base_blob
template <typename Blob>
inline Blob ToBlob(kj::ArrayPtr<const kj::byte> data)
{
    // TODO: Avoid temp vector.
    return Blob(std::vector<unsigned char>(data.begin(), data.begin() + data.size()));
}

//! Serialize bitcoin value.
template <typename T>
CDataStream Serialize(const T& value)
{
    CDataStream stream(SER_NETWORK, CLIENT_VERSION);
    value.Serialize(stream);
    return stream;
}

//! Deserialize bitcoin value.
template <typename T>
T Unserialize(T& value, const kj::ArrayPtr<const kj::byte>& data)
{
    // Could optimize, it unnecessarily copies the data into a temporary vector.
    CDataStream stream(reinterpret_cast<const char*>(data.begin()), reinterpret_cast<const char*>(data.end()),
        SER_NETWORK, CLIENT_VERSION);
    value.Unserialize(stream);
    return value;
}

//! Deserialize bitcoin value.
template <typename T>
T Unserialize(const kj::ArrayPtr<const kj::byte>& data)
{
    T value;
    Unserialize(value, data);
    return value;
}

} // namespace serialize
} // namespace ipc

#endif // BITCOIN_IPC_CAPNP_SERIALIZE_H
