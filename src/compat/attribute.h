// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_COMPAT_ATTRIBUTE_H
#define BITCOIN_COMPAT_ATTRIBUTE_H

#if defined(__GNUC__)
#define FORCED_INLINE inline __attribute__((always_inline))
#elif defined(_MSC_VER)
#define FORCED_INLINE __forceinline
#else
#define FORCED_INLINE inline
#endif

#endif // BITCOIN_COMPAT_ATTRIBUTE_H
