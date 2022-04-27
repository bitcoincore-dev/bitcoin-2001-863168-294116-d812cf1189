// Copyright (c) 2023-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <script/transaction_unserialize.h>

#include <span.h>

#include <cstddef>
#include <cstring>
#include <ios>
#include <string>

namespace {

/** A class that deserializes a single CTransaction one time. */
class TxInputStream
{
public:
    TxInputStream(const unsigned char *txTo, size_t txToLen) :
    m_data(txTo),
    m_remaining(txToLen)
    {}

    void read(Span<std::byte> dst)
    {
        if (dst.size() > m_remaining) {
            throw std::ios_base::failure(std::string(__func__) + ": end of data");
        }

        if (dst.data() == nullptr) {
            throw std::ios_base::failure(std::string(__func__) + ": bad destination buffer");
        }

        if (m_data == nullptr) {
            throw std::ios_base::failure(std::string(__func__) + ": bad source buffer");
        }

        memcpy(dst.data(), m_data, dst.size());
        m_remaining -= dst.size();
        m_data += dst.size();
    }

    template<typename T>
    TxInputStream& operator>>(T&& obj)
    {
        ::Unserialize(*this, obj);
        return *this;
    }

private:
    const unsigned char* m_data;
    size_t m_remaining;
};
} // namespace

namespace bitcoinconsensus {
CTransaction UnserializeTx(const unsigned char* txTo, unsigned int txToLen)
{
    TxInputStream stream(txTo, txToLen);
    return {deserialize, TX_WITH_WITNESS, stream};
}

size_t TxSize(const CTransaction& tx)
{
    return GetSerializeSize(TX_WITH_WITNESS(tx));
}
} // namespace bitcoinconsensus
