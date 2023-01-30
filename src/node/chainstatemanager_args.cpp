// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <node/chainstatemanager_args.h>

#include <arith_uint256.h>
#include <logging.h>
#include <tinyformat.h>
#include <uint256.h>
#include <util/strencodings.h>
#include <util/system.h>
#include <util/translation.h>
#include <validation.h>

#include <algorithm>
#include <chrono>
#include <optional>
#include <string>

namespace node {
std::optional<bilingual_str> ApplyArgsManOptions(const ArgsManager& args, ChainstateManager::Options& opts)
{
    if (auto value{args.GetBoolArg("-checkblockindex")}) opts.check_block_index = *value;

    if (auto value{args.GetBoolArg("-checkpoints")}) opts.checkpoints_enabled = *value;

    if (auto value{args.GetArg("-minimumchainwork")}) {
        if (!IsHexNumber(*value)) {
            return strprintf(Untranslated("Invalid non-hex (%s) minimum chain work value specified"), *value);
        }
        opts.minimum_chain_work = UintToArith256(uint256S(*value));
    }

    if (auto value{args.GetArg("-assumevalid")}) opts.assumed_valid_block = uint256S(*value);

    if (auto value{args.GetIntArg("-maxtipage")}) opts.max_tip_age = std::chrono::seconds{*value};

    int par_value = args.GetIntArg("-par", DEFAULT_SCRIPTCHECK_THREADS);
    if (par_value <= 0) {
        // -par=0 means autodetect (number of cores - 1 script threads)
        // -par=-n means "leave n cores free" (number of cores - n - 1 script threads)
        par_value += GetNumCores();
    }
    // Subtract 1 because the main thread counts towards the par threads.
    opts.worker_threads_num = std::clamp(par_value - 1, 0, MAX_SCRIPTCHECK_THREADS);
    LogPrintf("Script verification uses %d additional threads\n", opts.worker_threads_num);

    return std::nullopt;
}
} // namespace node
