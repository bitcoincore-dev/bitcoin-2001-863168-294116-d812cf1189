// Copyright (c) 2009-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <cstddef>
#include <cstdint>

#if defined(__i386__)

extern "C" int64_t __divdi3(int64_t, int64_t);

extern "C" int64_t __wrap___divmoddi4(int64_t u, int64_t v, int64_t* rp)
{
    int64_t d = __divdi3(u, v);
    *rp = u - (d * v);
    return d;
}

#endif
