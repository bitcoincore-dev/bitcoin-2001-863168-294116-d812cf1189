// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <util/check.h>

#include <tinyformat.h>

#include <cstdlib>
#include <iostream>

void assertion_fail(const char* file, int line, const char* func, const char* assertion)
{
    auto str = strprintf("%s:%s %s: Assertion `%s' failed.\n", file, line, func, assertion);
    fwrite(str.data(), 1, str.size(), stderr);
    std::abort();
}

void inline_unreachable(const char* file, int line, const char* func)
{
    std::cerr << format_internal_error("Unreachable code reached (fatal, aborting)", file, line, func, PACKAGE_BUGREPORT) << std::endl;
    std::abort();
}
