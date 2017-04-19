#ifndef BITCOIN_IPC_CAPNP_SERIALIZE_H
#define BITCOIN_IPC_CAPNP_SERIALIZE_H

#include "clientversion.h"
#include "ipc/capnp/messages.capnp.h"
#include "streams.h"
#include "support/allocators/secure.h"

#include <kj/string.h>
#include <string>

class CNodeStats;
class CNodeStateStats;

namespace ipc {
namespace capnp {

//!@{
//! Functions to serialize / deserialize bitcoin objects that don't
//! already provide their own serialization.
void BuildNodeStats(messages::NodeStats::Builder&& builder, const CNodeStats& nodeStats);
void ReadNodeStats(CNodeStats& nodeStats, const messages::NodeStats::Reader& reader);
void BuildNodeStateStats(messages::NodeStateStats::Builder&& builder, const CNodeStateStats& nodeStateStats);
void ReadNodeStateStats(CNodeStateStats& nodeStateStats, const messages::NodeStateStats::Reader& reader);
//!@}

//! Convert kj::String to std::string.
inline std::string ToString(const kj::StringPtr& str) { return {str.cStr(), str.size()}; }

//! Convert kj::String to SecureString.
//! TODO: Make secure.
inline SecureString ToSecureString(const kj::ArrayPtr<const kj::byte>& data)
{
    return {(const char*)data.begin(), data.size()};
}

inline kj::ArrayPtr<const kj::byte> FromSecureString(const SecureString& str)
{
    return {reinterpret_cast<const kj::byte*>(str.data()), str.size()};
}

//! Convert CDataStream to kj::ArrayPtr.
inline kj::ArrayPtr<const kj::byte> ToArray(const CDataStream& stream)
{
    return {reinterpret_cast<const kj::byte*>(stream.data()), stream.size()};
}

//! Convert base_blob to kj::ArrayPtr.
template <typename Blob>
inline kj::ArrayPtr<const kj::byte> ToArray(const Blob& blob)
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
