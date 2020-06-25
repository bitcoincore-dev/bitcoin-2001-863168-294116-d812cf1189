// Copyright (c) 2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_UTIL_UINT256HASH_H
#define BITCOIN_UTIL_UINT256HASH_H

#include <uint256.h>

#include <cstddef>
#include <cstdint>

class uint256Hash
{
public:
    uint256Hash();
    size_t operator()(const uint256& input) const;

private:
    const uint64_t m_k0;
    const uint64_t m_k1;
};

#endif // BITCOIN_UTIL_UINT256HASH_H
