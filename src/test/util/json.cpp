// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/util/json.h>

#include <boost/test/unit_test.hpp>
#include <string>

#include <univalue.h>

UniValue read_json(const std::string& jsondata)
{
    UniValue v;
    if (!v.read(jsondata) || !v.isArray()) {
        BOOST_ERROR("Parse error.");
        return UniValue(UniValue::VARR);
    }
    return v.get_array();
}
