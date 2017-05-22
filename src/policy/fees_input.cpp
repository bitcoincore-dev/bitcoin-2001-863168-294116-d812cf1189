#include "policy/fees_input.h"

#include <chainparams.h>
#include <clientversion.h>
#include <policy/fees.h>
#include <streams.h>
#include <txmempool.h>
#include <util.h>
#include <utilstrencodings.h>
#include <utiltime.h>

#include <univalue.h>

namespace {
UniValue TxValue(const CTxMemPoolEntry& entry)
{
    UniValue value(UniValue::VOBJ);
    value.pushKV("hash", entry.GetTx().GetHash().ToString());
    value.pushKV("height", int(entry.GetHeight()));
    value.pushKV("fee", entry.GetFee());
    value.pushKV("size", int(entry.GetTxSize()));
    return value;
}

} // namespace

CBlockPolicyInput::CBlockPolicyInput(CBlockPolicyEstimator& estimator) : estimator(estimator) {}

void CBlockPolicyInput::processTx(const CTxMemPoolEntry& entry, bool validFeeEstimate)
{
    estimator.processTx(
        entry.GetTx().GetHash(), entry.GetHeight(), entry.GetFee(), entry.GetTxSize(), validFeeEstimate);

    if (log) {
        UniValue value(UniValue::VOBJ);
        value.pushKV("tx", TxValue(entry));
        value.pushKV("valid", UniValue(validFeeEstimate));
        value.pushKV("time", GetTime());
        *log << value.write() << std::endl;
    }
}

void CBlockPolicyInput::processBlock(unsigned int nBlockHeight, std::vector<const CTxMemPoolEntry*>& entries)
{
    estimator.processBlock(nBlockHeight, [&](unsigned int& countedTxs) {
        countedTxs = 0;
        for (const auto& entry : entries) {
            if (estimator.processBlockTx(
                    entry->GetTx().GetHash(), entry->GetHeight(), entry->GetFee(), entry->GetTxSize(), nBlockHeight)) {
                countedTxs++;
            }
        }
        return entries.size();
    });

    if (log) {
        UniValue value(UniValue::VOBJ);
        UniValue block(UniValue::VOBJ);
        block.pushKV("height", int(nBlockHeight));
        value.pushKV("block", block);
        UniValue txs(UniValue::VARR);
        for (const auto& entry : entries) {
            txs.push_back(TxValue(*entry));
        }
        value.pushKV("txs", txs);
        value.pushKV("time", GetTime());
        *log << value.write() << std::endl;
    }
}

void CBlockPolicyInput::removeTx(const uint256& hash, bool inBlock)
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

void CBlockPolicyInput::flushUnconfirmed()
{
    estimator.flushUnconfirmed();
    if (log) {
        UniValue value(UniValue::VOBJ);
        UniValue flush(UniValue::VARR);
        value.pushKV("flush", flush);
        value.pushKV("time", GetTime());
        *log << value.write() << std::endl;
    }
}

bool CBlockPolicyInput::writeData(const fs::path& filename)
{
    CAutoFile file(fsbridge::fopen(filename, "wb"), SER_DISK, CLIENT_VERSION);
    if (file.IsNull()) {
        LogPrintf("%s: Failed to write fee estimates to %s\n", __func__, filename.string());
        return false;
    }
    estimator.writeData(file);
    return true;
}

bool CBlockPolicyInput::readData(const fs::path& filename)
{
    CAutoFile file(fsbridge::fopen(filename, "rb"), SER_DISK, CLIENT_VERSION);
    if (file.IsNull() || !estimator.readData(file)) {
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

bool CBlockPolicyInput::writeLog(const std::string& filename)
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

bool CBlockPolicyInput::readLog(const std::string& filename, std::function<bool(UniValue&)> filter)
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
                tx["size"].get_int(), value["valid"].get_bool());
            continue;
        }

        const UniValue& block = value["block"];
        if (block.isObject()) {
            int blockHeight = block["height"].get_int();
            estimator.processBlock(blockHeight, [&](unsigned int& countedTxs) {
                countedTxs = 0;
                const auto& txs = value["txs"].getValues();
                for (const UniValue& tx : txs) {
                    if (estimator.processBlockTx(uint256S(tx["hash"].get_str()), tx["height"].get_int(),
                            tx["fee"].get_int64(), tx["size"].get_int(), blockHeight)) {
                        countedTxs++;
                    }
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
            estimator.flushUnconfirmed();
            continue;
        }

        const UniValue& read = value["read"];
        if (read.isStr()) {
            std::vector<unsigned char> data = ParseHex(read.get_str());
            unsigned short randv = 0;
            GetRandBytes((unsigned char*)&randv, sizeof(randv));
            fs::path filename = strprintf("fee_estimates.tmp.%04x", randv);
            CAutoFile(fsbridge::fopen(filename, "wb"), SER_DISK, CLIENT_VERSION)
                .write((const char*)data.data(), data.size());
            {
                CAutoFile file(fsbridge::fopen(filename, "rb"), SER_DISK, CLIENT_VERSION);
                estimator.readData(file);
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
