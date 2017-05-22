// Copyright (c) 2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <policy/fees_input.h>

#include <chainparams.h>
#include <clientversion.h>
#include <policy/fees.h>
#include <streams.h>
#include <util.h>
#include <utilstrencodings.h>
#include <utiltime.h>

#include <univalue.h>

struct FeeEstInput::Block {
    UniValue json_txs{UniValue::VARR};
    CBlockPolicyEstimator::Block* est_block = nullptr;
};

FeeEstInput::FeeEstInput(CBlockPolicyEstimator& estimator) : estimator(estimator) {}

void FeeEstInput::processTx(const uint256& hash, unsigned int height, CAmount fee, size_t size, Block* block, bool valid)
{
    estimator.processTx(hash, height, fee, size, block ? block->est_block : nullptr, valid);
    if (log) {
        UniValue tx(UniValue::VOBJ);
        tx.pushKV("hash", hash.ToString());
        tx.pushKV("height", int(height));
        tx.pushKV("fee", fee);
        tx.pushKV("size", uint64_t(size));
        if (block) {
            block->json_txs.push_back(std::move(tx));
        } else {
            UniValue value(UniValue::VOBJ);
            value.pushKV("tx", std::move(tx));
            value.pushKV("valid", UniValue(valid));
            value.pushKV("time", GetTime());
            *log << value.write() << std::endl;
        }
    }
}

void FeeEstInput::processBlock(int height, const std::function<void(Block&)>& process_txs)
{
    Block block;
    estimator.processBlock(height, [&](CBlockPolicyEstimator::Block& est_block) {
        block.est_block = &est_block;
        process_txs(block);
    });
    if (log) {
        UniValue json(UniValue::VOBJ);
        UniValue json_block(UniValue::VOBJ);
        json_block.pushKV("height", height);
        json.pushKV("block", json_block);
        json.pushKV("txs", block.json_txs);
        json.pushKV("time", GetTime());
        *log << json.write() << std::endl;
    }
}

void FeeEstInput::removeTx(const uint256& hash, bool inBlock)
{
    estimator.removeTx(hash, inBlock);

    if (log) {
        UniValue value(UniValue::VOBJ);
        UniValue removeTx(UniValue::VOBJ);
        removeTx.pushKV("hash", hash.ToString());
        removeTx.pushKV("inBlock", UniValue(inBlock));
        value.pushKV("removeTx", removeTx);
        value.pushKV("time", GetTime());
        *log << value.write() << std::endl;
    }
}

void FeeEstInput::flushUnconfirmed()
{
    estimator.FlushUnconfirmed();
    if (log) {
        UniValue value(UniValue::VOBJ);
        UniValue flush(UniValue::VARR);
        value.pushKV("flush", flush);
        value.pushKV("time", GetTime());
        *log << value.write() << std::endl;
    }
}

bool FeeEstInput::writeData(const fs::path& filename)
{
    CAutoFile file(fsbridge::fopen(filename, "wb"), SER_DISK, CLIENT_VERSION);
    if (file.IsNull()) {
        LogPrintf("%s: Failed to write fee estimates to %s\n", __func__, filename.string());
        return false;
    }
    estimator.Write(file);
    return true;
}

bool FeeEstInput::readData(const fs::path& filename)
{
    CAutoFile file(fsbridge::fopen(filename, "rb"), SER_DISK, CLIENT_VERSION);
    if (file.IsNull() || !estimator.Read(file)) {
        return false;
    }
    if (log) {
        UniValue value(UniValue::VOBJ);
        std::ifstream file(filename.string(), std::ifstream::binary);
        value.pushKV(
            "read", HexStr(std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>())));
        value.pushKV("time", GetTime());
        *log << value.write() << std::endl;
    }
    return true;
}

bool FeeEstInput::writeLog(const std::string& filename)
{
    if (log) {
        UniValue value(UniValue::VOBJ);
        value.pushKV("stop", Params().NetworkIDString());
        value.pushKV("time", GetTime());
        *log << value.write() << std::endl;
    }

    if (filename.empty()) {
        log.reset();
    } else {
        log.reset(new std::ofstream(filename, std::ofstream::out | std::ofstream::app));
        if (!*log) {
            log.reset();
            return false;
        }
    }

    if (log) {
        UniValue value(UniValue::VOBJ);
        value.pushKV("start", Params().NetworkIDString());
        value.pushKV("time", GetTime());
        *log << value.write() << std::endl;
    }

    return true;
}

bool FeeEstInput::readLog(const std::string& filename, std::function<bool(UniValue&)> filter)
{
    std::ifstream file(filename);
    if (!file) {
        LogPrintf("%s: Failed to open log file %s\n", __func__, filename);
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        UniValue value;
        if (!value.read(line)) {
            throw std::runtime_error("Failed to parse fee estimate log line.");
        }

        if (filter && !filter(value)) {
            continue;
        }

        const UniValue& tx = value["tx"];
        if (tx.isObject()) {
            estimator.processTx(uint256S(tx["hash"].get_str()), tx["height"].get_int(), tx["fee"].get_int64(),
                tx["size"].get_int(), nullptr /* block */, value["valid"].get_bool());
            continue;
        }

        const UniValue& block = value["block"];
        if (block.isObject()) {
            int height = block["height"].get_int();
            estimator.processBlock(height, [&](CBlockPolicyEstimator::Block& est_block) {
                const auto& txs = value["txs"].getValues();
                for (const UniValue& tx : txs) {
                    estimator.processTx(uint256S(tx["hash"].get_str()), tx["height"].get_int(),
                        tx["fee"].get_int64(), tx["size"].get_int(), &est_block, true /* valid */);
                }
                return txs.size();
            });
            continue;
        }

        const UniValue& removeTx = value["removeTx"];
        if (removeTx.isObject()) {
            estimator.removeTx(uint256S(removeTx["hash"].get_str()), removeTx["inBlock"].get_bool());
            continue;
        }

        const UniValue& flush = value["flush"];
        if (flush.isArray()) {
            estimator.FlushUnconfirmed();
            continue;
        }

        const UniValue& read = value["read"];
        if (read.isStr()) {
            std::vector<unsigned char> data = ParseHex(read.get_str());
            uint16_t randv = 0;
            GetRandBytes((unsigned char*)&randv, sizeof(randv));
            fs::path filename = strprintf("fee_estimates.tmp.%04x", randv);
            CAutoFile(fsbridge::fopen(filename, "wb"), SER_DISK, CLIENT_VERSION)
                .write((const char*)data.data(), data.size());
            {
                CAutoFile file(fsbridge::fopen(filename, "rb"), SER_DISK, CLIENT_VERSION);
                estimator.Read(file);
            }
            fs::remove(filename);
            continue;
        }
    }

    if (file.bad()) {
        LogPrintf("%s: Failure reading log file %s\n", __func__, filename);
        return false;
    }

    return true;
}
