// Copyright (c) 2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <util/uint256hash.h>

#include <crypto/siphash.h>
#include <random.h>

uint256Hash::uint256Hash()
    : m_k0{GetRand(UINT64_MAX)}, m_k1{GetRand(UINT64_MAX)}
{
}

size_t uint256Hash::operator()(const uint256& input) const
{
    return SipHashUint256(m_k0, m_k1, input);
}
