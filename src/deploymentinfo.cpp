// Copyright (c) 2016-2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <deploymentinfo.h>

#include <consensus/params.h>
#include <script/interpreter.h>
#include <tinyformat.h>

const struct VBDeploymentInfo VersionBitsDeploymentInfo[Consensus::MAX_VERSION_BITS_DEPLOYMENTS] = {
    {
        /*.name =*/ "testdummy",
        /*.gbt_force =*/ true,
    },
    {
        /*.name =*/ "checktemplateverify",
        /*.gbt_force =*/ true,
    },
    {
        /*.name =*/ "anyprevout",
        /*.gbt_force =*/ true,
    },
    {
        /*.name =*/ "vault",
        /*.gbt_force =*/ true,
    },
};

std::string DeploymentName(Consensus::BuriedDeployment dep)
{
    assert(ValidDeployment(dep));
    switch (dep) {
    case Consensus::DEPLOYMENT_HEIGHTINCB:
        return "bip34";
    case Consensus::DEPLOYMENT_CLTV:
        return "bip65";
    case Consensus::DEPLOYMENT_DERSIG:
        return "bip66";
    case Consensus::DEPLOYMENT_CSV:
        return "csv";
    case Consensus::DEPLOYMENT_SEGWIT:
        return "segwit";
    case Consensus::DEPLOYMENT_TAPROOT:
        return "taproot";
    } // no default case, so the compiler can warn about missing cases
    return "";
}

#define FLAG_NAME(flag) {std::string(#flag), (uint32_t)SCRIPT_VERIFY_##flag},
const std::map<std::string, uint32_t> g_verify_flag_names{
    FLAG_NAME(P2SH)
    FLAG_NAME(STRICTENC)
    FLAG_NAME(DERSIG)
    FLAG_NAME(LOW_S)
    FLAG_NAME(SIGPUSHONLY)
    FLAG_NAME(MINIMALDATA)
    FLAG_NAME(NULLDUMMY)
    FLAG_NAME(DISCOURAGE_UPGRADABLE_NOPS)
    FLAG_NAME(CLEANSTACK)
    FLAG_NAME(MINIMALIF)
    FLAG_NAME(NULLFAIL)
    FLAG_NAME(CHECKLOCKTIMEVERIFY)
    FLAG_NAME(CHECKSEQUENCEVERIFY)
    FLAG_NAME(WITNESS)
    FLAG_NAME(DISCOURAGE_UPGRADABLE_WITNESS_PROGRAM)
    FLAG_NAME(WITNESS_PUBKEYTYPE)
    FLAG_NAME(CONST_SCRIPTCODE)
    FLAG_NAME(TAPROOT)
    FLAG_NAME(DISCOURAGE_UPGRADABLE_PUBKEYTYPE)
    FLAG_NAME(DISCOURAGE_OP_SUCCESS)
    FLAG_NAME(DISCOURAGE_UPGRADABLE_TAPROOT_VERSION)
    FLAG_NAME(DEFAULT_CHECK_TEMPLATE_VERIFY_HASH)
    FLAG_NAME(DISCOURAGE_UPGRADABLE_CHECK_TEMPLATE_VERIFY_HASH)
    FLAG_NAME(DISCOURAGE_CHECK_TEMPLATE_VERIFY_HASH)
    FLAG_NAME(ANYPREVOUT)
    FLAG_NAME(DISCOURAGE_ANYPREVOUT)
    FLAG_NAME(VAULT)
    FLAG_NAME(VAULT_REPLACEABLE_RECOVERY)
    FLAG_NAME(VAULT_UNAUTH_RECOVERY_STRUCTURE)
};
#undef FLAG_NAME

std::string FormatScriptFlags(uint32_t flags)
{
    if (flags == 0) return "";

    std::string res{};
    uint32_t leftover = flags;
    for (const auto& [name, flag] : g_verify_flag_names) {
        if ((flags & flag) != 0) {
            if (!res.empty()) res += ",";
            res += name;
            leftover &= ~flag;
        }
    }
    if (leftover != 0) {
        if (!res.empty()) res += ",";
        res += strprintf("0x%08x", leftover);
    }
    return res;
}
