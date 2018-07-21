// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <base58.h>
#include <chain.h>
#include <coins.h>
#include <consensus/validation.h>
#include <core_io.h>
#include <init.h>
#include <keystore.h>
#include <validation.h>
#include <validationinterface.h>
#include <merkleblock.h>
#include <net.h>
#include <policy/policy.h>
#include <policy/rbf.h>
#include <primitives/transaction.h>
#include <rpc/safemode.h>
#include <rpc/server.h>
#include <rpc/util.h>
#include <script/script.h>
#include <script/script_error.h>
#include <script/sign.h>
#include <script/standard.h>
#include <txmempool.h>
#include <uint256.h>
#include <utilstrencodings.h>
#ifdef ENABLE_WALLET
#include <wallet/rpcwallet.h>
#include <wallet/wallet.h>
#endif

#include <future>
#include <stdint.h>

#include <univalue.h>


void TxToJSON(const CTransaction& tx, const uint256 hashBlock, UniValue& entry)
{
    // Call into TxToUniv() in bitcoin-common to decode the transaction hex.
    //
    // Blockchain contextual information (confirmations and blocktime) is not
    // available to code in bitcoin-common, so we query them here and push the
    // data into the returned UniValue.
    TxToUniv(tx, uint256(), entry, true, RPCSerializationFlags());

    if (!hashBlock.IsNull()) {
        entry.push_back(Pair("blockhash", hashBlock.GetHex()));
        BlockMap::iterator mi = mapBlockIndex.find(hashBlock);
        if (mi != mapBlockIndex.end() && (*mi).second) {
            CBlockIndex* pindex = (*mi).second;
            if (chainActive.Contains(pindex)) {
                entry.push_back(Pair("confirmations", 1 + chainActive.Height() - pindex->nHeight));
                entry.push_back(Pair("time", pindex->GetBlockTime()));
                entry.push_back(Pair("blocktime", pindex->GetBlockTime()));
            }
            else
                entry.push_back(Pair("confirmations", 0));
        }
    }
}

UniValue getrawtransaction(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 1 || request.params.size() > 3)
        throw std::runtime_error(
            "getrawtransaction \"txid\" ( verbose \"blockhash\" )\n"

            "\nNOTE: By default this function only works for mempool transactions. If the -txindex option is\n"
            "enabled, it also works for blockchain transactions. If the block which contains the transaction\n"
            "is known, its hash can be provided even for nodes without -txindex. Note that if a blockhash is\n"
            "provided, only that block will be searched and if the transaction is in the mempool or other\n"
            "blocks, or if this node does not have the given block available, the transaction will not be found.\n"
            "DEPRECATED: for now, it also works for transactions with unspent outputs.\n"

            "\nReturn the raw transaction data.\n"
            "\nIf verbose is 'true', returns an Object with information about 'txid'.\n"
            "If verbose is 'false' or omitted, returns a string that is serialized, hex-encoded data for 'txid'.\n"

            "\nArguments:\n"
            "1. \"txid\"      (string, required) The transaction id\n"
            "2. verbose     (bool, optional, default=false) If false, return a string, otherwise return a json object\n"
            "3. \"blockhash\" (string, optional) The block in which to look for the transaction\n"

            "\nResult (if verbose is not set or set to false):\n"
            "\"data\"      (string) The serialized, hex-encoded data for 'txid'\n"

            "\nResult (if verbose is set to true):\n"
            "{\n"
            "  \"in_active_chain\": b, (bool) Whether specified block is in the active chain or not (only present with explicit \"blockhash\" argument)\n"
            "  \"hex\" : \"data\",       (string) The serialized, hex-encoded data for 'txid'\n"
            "  \"txid\" : \"id\",        (string) The transaction id (same as provided)\n"
            "  \"hash\" : \"id\",        (string) The transaction hash (differs from txid for witness transactions)\n"
            "  \"size\" : n,             (numeric) The serialized transaction size\n"
            "  \"vsize\" : n,            (numeric) The virtual transaction size (differs from size for witness transactions)\n"
            "  \"weight\" : n,           (numeric) The transaction's weight (between vsize*4-3 and vsize*4)\n"
            "  \"version\" : n,          (numeric) The version\n"
            "  \"locktime\" : ttt,       (numeric) The lock time\n"
            "  \"vin\" : [               (array of json objects)\n"
            "     {\n"
            "       \"txid\": \"id\",    (string) The transaction id\n"
            "       \"vout\": n,         (numeric) \n"
            "       \"scriptSig\": {     (json object) The script\n"
            "         \"asm\": \"asm\",  (string) asm\n"
            "         \"hex\": \"hex\"   (string) hex\n"
            "       },\n"
            "       \"sequence\": n      (numeric) The script sequence number\n"
            "       \"txinwitness\": [\"hex\", ...] (array of string) hex-encoded witness data (if any)\n"
            "     }\n"
            "     ,...\n"
            "  ],\n"
            "  \"vout\" : [              (array of json objects)\n"
            "     {\n"
            "       \"value\" : x.xxx,            (numeric) The value in " + CURRENCY_UNIT + "\n"
            "       \"n\" : n,                    (numeric) index\n"
            "       \"scriptPubKey\" : {          (json object)\n"
            "         \"asm\" : \"asm\",          (string) the asm\n"
            "         \"hex\" : \"hex\",          (string) the hex\n"
            "         \"reqSigs\" : n,            (numeric) The required sigs\n"
            "         \"type\" : \"pubkeyhash\",  (string) The type, eg 'pubkeyhash'\n"
            "         \"addresses\" : [           (json array of string)\n"
            "           \"address\"        (string) bitcoin address\n"
            "           ,...\n"
            "         ]\n"
            "       }\n"
            "     }\n"
            "     ,...\n"
            "  ],\n"
            "  \"blockhash\" : \"hash\",   (string) the block hash\n"
            "  \"confirmations\" : n,      (numeric) The confirmations\n"
            "  \"time\" : ttt,             (numeric) The transaction time in seconds since epoch (Jan 1 1970 GMT)\n"
            "  \"blocktime\" : ttt         (numeric) The block time in seconds since epoch (Jan 1 1970 GMT)\n"
            "}\n"

            "\nExamples:\n"
            + HelpExampleCli("getrawtransaction", "\"mytxid\"")
            + HelpExampleCli("getrawtransaction", "\"mytxid\" true")
            + HelpExampleRpc("getrawtransaction", "\"mytxid\", true")
            + HelpExampleCli("getrawtransaction", "\"mytxid\" false \"myblockhash\"")
            + HelpExampleCli("getrawtransaction", "\"mytxid\" true \"myblockhash\"")
        );

    LOCK(cs_main);

    bool in_active_chain = true;
    uint256 hash = ParseHashV(request.params[0], "parameter 1");
    CBlockIndex* blockindex = nullptr;

    if (hash == Params().GenesisBlock().hashMerkleRoot) {
        // Special exception for the genesis block coinbase transaction
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "The genesis block coinbase is not considered an ordinary transaction and cannot be retrieved");
    }

    // Accept either a bool (true) or a num (>=1) to indicate verbose output.
    bool fVerbose = false;
    if (!request.params[1].isNull()) {
        fVerbose = request.params[1].isNum() ? (request.params[1].get_int() != 0) : request.params[1].get_bool();
    }

    if (!request.params[2].isNull()) {
        uint256 blockhash = ParseHashV(request.params[2], "parameter 3");
        BlockMap::iterator it = mapBlockIndex.find(blockhash);
        if (it == mapBlockIndex.end()) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block hash not found");
        }
        blockindex = it->second;
        in_active_chain = chainActive.Contains(blockindex);
    }

    CTransactionRef tx;
    uint256 hash_block;
    if (!GetTransaction(hash, tx, Params().GetConsensus(), hash_block, true, blockindex)) {
        std::string errmsg;
        if (blockindex) {
            if (!(blockindex->nStatus & BLOCK_HAVE_DATA)) {
                throw JSONRPCError(RPC_MISC_ERROR, "Block not available");
            }
            errmsg = "No such transaction found in the provided block";
        } else {
            errmsg = fTxIndex
              ? "No such mempool or blockchain transaction"
              : "No such mempool transaction. Use -txindex to enable blockchain transaction queries";
        }
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, errmsg + ". Use gettransaction for wallet transactions.");
    }

    if (!fVerbose) {
        return EncodeHexTx(*tx, RPCSerializationFlags());
    }

    UniValue result(UniValue::VOBJ);
    if (blockindex) result.push_back(Pair("in_active_chain", in_active_chain));
    TxToJSON(*tx, hash_block, result);
    return result;
}

UniValue gettxoutproof(const JSONRPCRequest& request)
{
    if (request.fHelp || (request.params.size() != 1 && request.params.size() != 2))
        throw std::runtime_error(
            "gettxoutproof [\"txid\",...] ( blockhash )\n"
            "\nReturns a hex-encoded proof that \"txid\" was included in a block.\n"
            "\nNOTE: By default this function only works sometimes. This is when there is an\n"
            "unspent output in the utxo for this transaction. To make it always work,\n"
            "you need to maintain a transaction index, using the -txindex command line option or\n"
            "specify the block in which the transaction is included manually (by blockhash).\n"
            "\nArguments:\n"
            "1. \"txids\"       (string) A json array of txids to filter\n"
            "    [\n"
            "      \"txid\"     (string) A transaction hash\n"
            "      ,...\n"
            "    ]\n"
            "2. \"blockhash\"   (string, optional) If specified, looks for txid in the block with this hash\n"
            "\nResult:\n"
            "\"data\"           (string) A string that is a serialized, hex-encoded data for the proof.\n"
        );

    std::set<uint256> setTxids;
    uint256 oneTxid;
    UniValue txids = request.params[0].get_array();
    for (unsigned int idx = 0; idx < txids.size(); idx++) {
        const UniValue& txid = txids[idx];
        if (txid.get_str().length() != 64 || !IsHex(txid.get_str()))
            throw JSONRPCError(RPC_INVALID_PARAMETER, std::string("Invalid txid ")+txid.get_str());
        uint256 hash(uint256S(txid.get_str()));
        if (setTxids.count(hash))
            throw JSONRPCError(RPC_INVALID_PARAMETER, std::string("Invalid parameter, duplicated txid: ")+txid.get_str());
       setTxids.insert(hash);
       oneTxid = hash;
    }

    LOCK(cs_main);

    CBlockIndex* pblockindex = nullptr;

    uint256 hashBlock;
    if (!request.params[1].isNull())
    {
        hashBlock = uint256S(request.params[1].get_str());
        if (!mapBlockIndex.count(hashBlock))
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");
        pblockindex = mapBlockIndex[hashBlock];
    } else {
        // Loop through txids and try to find which block they're in. Exit loop once a block is found.
        for (const auto& tx : setTxids) {
            const Coin& coin = AccessByTxid(*pcoinsTip, tx);
            if (!coin.IsSpent()) {
                pblockindex = chainActive[coin.nHeight];
                break;
            }
        }
    }

    if (pblockindex == nullptr)
    {
        CTransactionRef tx;
        if (!GetTransaction(oneTxid, tx, Params().GetConsensus(), hashBlock, false) || hashBlock.IsNull())
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Transaction not yet in block");
        if (!mapBlockIndex.count(hashBlock))
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Transaction index corrupt");
        pblockindex = mapBlockIndex[hashBlock];
    }

    CBlock block;
    if(!ReadBlockFromDisk(block, pblockindex, Params().GetConsensus()))
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Can't read block from disk");

    unsigned int ntxFound = 0;
    for (const auto& tx : block.vtx)
        if (setTxids.count(tx->GetHash()))
            ntxFound++;
    if (ntxFound != setTxids.size())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Not all transactions found in specified or retrieved block");

    CDataStream ssMB(SER_NETWORK, PROTOCOL_VERSION | SERIALIZE_TRANSACTION_NO_WITNESS);
    CMerkleBlock mb(block, setTxids);
    ssMB << mb;
    std::string strHex = HexStr(ssMB.begin(), ssMB.end());
    return strHex;
}

UniValue verifytxoutproof(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
            "verifytxoutproof \"proof\"\n"
            "\nVerifies that a proof points to a transaction in a block, returning the transaction it commits to\n"
            "and throwing an RPC error if the block is not in our best chain\n"
            "\nArguments:\n"
            "1. \"proof\"    (string, required) The hex-encoded proof generated by gettxoutproof\n"
            "\nResult:\n"
            "[\"txid\"]      (array, strings) The txid(s) which the proof commits to, or empty array if the proof can not be validated.\n"
        );

    CDataStream ssMB(ParseHexV(request.params[0], "proof"), SER_NETWORK, PROTOCOL_VERSION | SERIALIZE_TRANSACTION_NO_WITNESS);
    CMerkleBlock merkleBlock;
    ssMB >> merkleBlock;

    UniValue res(UniValue::VARR);

    std::vector<uint256> vMatch;
    std::vector<unsigned int> vIndex;
    if (merkleBlock.txn.ExtractMatches(vMatch, vIndex) != merkleBlock.header.hashMerkleRoot)
        return res;

    LOCK(cs_main);

    if (!mapBlockIndex.count(merkleBlock.header.GetHash()) || !chainActive.Contains(mapBlockIndex[merkleBlock.header.GetHash()]) || mapBlockIndex[merkleBlock.header.GetHash()]->nTx == 0)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found in chain");

    // Check if proof is valid, only add results if so
    if (mapBlockIndex[merkleBlock.header.GetHash()]->nTx == merkleBlock.txn.GetNumTransactions()) {
        for (const uint256& hash : vMatch) {
            res.push_back(hash.GetHex());
        }
    }

    return res;
}

UniValue createrawtransaction(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 2 || request.params.size() > 4)
        throw std::runtime_error(
            "createrawtransaction [{\"txid\":\"id\",\"vout\":n},...] {\"address\":amount,\"data\":\"hex\",...} ( locktime ) ( replaceable )\n"
            "\nCreate a transaction spending the given inputs and creating new outputs.\n"
            "Outputs can be addresses or data.\n"
            "Returns hex-encoded raw transaction.\n"
            "Note that the transaction's inputs are not signed, and\n"
            "it is not stored in the wallet or transmitted to the network.\n"

            "\nArguments:\n"
            "1. \"inputs\"                (array, required) A json array of json objects\n"
            "     [\n"
            "       {\n"
            "         \"txid\":\"id\",    (string, required) The transaction id\n"
            "         \"vout\":n,         (numeric, required) The output number\n"
            "         \"sequence\":n      (numeric, optional) The sequence number\n"
            "       } \n"
            "       ,...\n"
            "     ]\n"
            "2. \"outputs\"               (object, required) a json object with outputs\n"
            "    {\n"
            "      \"address\": x.xxx,    (numeric or string, required) The key is the bitcoin address, the numeric value (can be string) is the " + CURRENCY_UNIT + " amount\n"
            "      \"data\": \"hex\"      (string, required) The key is \"data\", the value is hex encoded data\n"
            "      ,...\n"
            "    }\n"
            "3. locktime                  (numeric, optional, default=0) Raw locktime. Non-0 value also locktime-activates inputs\n"
            "4. replaceable               (boolean, optional, default=false) Marks this transaction as BIP125 replaceable.\n"
            "                             Allows this transaction to be replaced by a transaction with higher fees. If provided, it is an error if explicit sequence numbers are incompatible.\n"
            "\nResult:\n"
            "\"transaction\"              (string) hex string of the transaction\n"

            "\nExamples:\n"
            + HelpExampleCli("createrawtransaction", "\"[{\\\"txid\\\":\\\"myid\\\",\\\"vout\\\":0}]\" \"{\\\"address\\\":0.01}\"")
            + HelpExampleCli("createrawtransaction", "\"[{\\\"txid\\\":\\\"myid\\\",\\\"vout\\\":0}]\" \"{\\\"data\\\":\\\"00010203\\\"}\"")
            + HelpExampleRpc("createrawtransaction", "\"[{\\\"txid\\\":\\\"myid\\\",\\\"vout\\\":0}]\", \"{\\\"address\\\":0.01}\"")
            + HelpExampleRpc("createrawtransaction", "\"[{\\\"txid\\\":\\\"myid\\\",\\\"vout\\\":0}]\", \"{\\\"data\\\":\\\"00010203\\\"}\"")
        );

    RPCTypeCheck(request.params, {UniValue::VARR, UniValue::VOBJ, UniValue::VNUM, UniValue::VBOOL}, true);
    if (request.params[0].isNull() || request.params[1].isNull())
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, arguments 1 and 2 must be non-null");

    UniValue inputs = request.params[0].get_array();
    UniValue sendTo = request.params[1].get_obj();

    CMutableTransaction rawTx;

    if (!request.params[2].isNull()) {
        int64_t nLockTime = request.params[2].get_int64();
        if (nLockTime < 0 || nLockTime > std::numeric_limits<uint32_t>::max())
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, locktime out of range");
        rawTx.nLockTime = nLockTime;
    }

    bool rbfOptIn = request.params[3].isTrue();

    for (unsigned int idx = 0; idx < inputs.size(); idx++) {
        const UniValue& input = inputs[idx];
        const UniValue& o = input.get_obj();

        uint256 txid = ParseHashO(o, "txid");

        const UniValue& vout_v = find_value(o, "vout");
        if (!vout_v.isNum())
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, missing vout key");
        int nOutput = vout_v.get_int();
        if (nOutput < 0)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, vout must be positive");

        uint32_t nSequence;
        if (rbfOptIn) {
            nSequence = MAX_BIP125_RBF_SEQUENCE;
        } else if (rawTx.nLockTime) {
            nSequence = std::numeric_limits<uint32_t>::max() - 1;
        } else {
            nSequence = std::numeric_limits<uint32_t>::max();
        }

        // set the sequence number if passed in the parameters object
        const UniValue& sequenceObj = find_value(o, "sequence");
        if (sequenceObj.isNum()) {
            int64_t seqNr64 = sequenceObj.get_int64();
            if (seqNr64 < 0 || seqNr64 > std::numeric_limits<uint32_t>::max()) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, sequence number is out of range");
            } else {
                nSequence = (uint32_t)seqNr64;
            }
        }

        CTxIn in(COutPoint(txid, nOutput), CScript(), nSequence);

        rawTx.vin.push_back(in);
    }

    std::set<CTxDestination> destinations;
    std::vector<std::string> addrList = sendTo.getKeys();
    for (const std::string& name_ : addrList) {

        if (name_ == "data") {
            std::vector<unsigned char> data = ParseHexV(sendTo[name_].getValStr(),"Data");

            CTxOut out(0, CScript() << OP_RETURN << data);
            rawTx.vout.push_back(out);
        } else {
            CTxDestination destination = DecodeDestination(name_);
            if (!IsValidDestination(destination)) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, std::string("Invalid Bitcoin address: ") + name_);
            }

            if (!destinations.insert(destination).second) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, std::string("Invalid parameter, duplicated address: ") + name_);
            }

            CScript scriptPubKey = GetScriptForDestination(destination);
            CAmount nAmount = AmountFromValue(sendTo[name_]);

            CTxOut out(nAmount, scriptPubKey);
            rawTx.vout.push_back(out);
        }
    }

    if (!request.params[3].isNull() && rbfOptIn != SignalsOptInRBF(rawTx)) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter combination: Sequence number(s) contradict replaceable option");
    }

    return EncodeHexTx(rawTx);
}

UniValue decoderawtransaction(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 1 || request.params.size() > 2)
        throw std::runtime_error(
            "decoderawtransaction \"hexstring\" ( iswitness )\n"
            "\nReturn a JSON object representing the serialized, hex-encoded transaction.\n"

            "\nArguments:\n"
            "1. \"hexstring\"      (string, required) The transaction hex string\n"
            "2. iswitness          (boolean, optional) Whether the transaction hex is a serialized witness transaction\n"
            "                         If iswitness is not present, heuristic tests will be used in decoding\n"

            "\nResult:\n"
            "{\n"
            "  \"txid\" : \"id\",        (string) The transaction id\n"
            "  \"hash\" : \"id\",        (string) The transaction hash (differs from txid for witness transactions)\n"
            "  \"size\" : n,             (numeric) The transaction size\n"
            "  \"vsize\" : n,            (numeric) The virtual transaction size (differs from size for witness transactions)\n"
            "  \"weight\" : n,           (numeric) The transaction's weight (between vsize*4 - 3 and vsize*4)\n"
            "  \"version\" : n,          (numeric) The version\n"
            "  \"locktime\" : ttt,       (numeric) The lock time\n"
            "  \"vin\" : [               (array of json objects)\n"
            "     {\n"
            "       \"txid\": \"id\",    (string) The transaction id\n"
            "       \"vout\": n,         (numeric) The output number\n"
            "       \"scriptSig\": {     (json object) The script\n"
            "         \"asm\": \"asm\",  (string) asm\n"
            "         \"hex\": \"hex\"   (string) hex\n"
            "       },\n"
            "       \"txinwitness\": [\"hex\", ...] (array of string) hex-encoded witness data (if any)\n"
            "       \"sequence\": n     (numeric) The script sequence number\n"
            "     }\n"
            "     ,...\n"
            "  ],\n"
            "  \"vout\" : [             (array of json objects)\n"
            "     {\n"
            "       \"value\" : x.xxx,            (numeric) The value in " + CURRENCY_UNIT + "\n"
            "       \"n\" : n,                    (numeric) index\n"
            "       \"scriptPubKey\" : {          (json object)\n"
            "         \"asm\" : \"asm\",          (string) the asm\n"
            "         \"hex\" : \"hex\",          (string) the hex\n"
            "         \"reqSigs\" : n,            (numeric) The required sigs\n"
            "         \"type\" : \"pubkeyhash\",  (string) The type, eg 'pubkeyhash'\n"
            "         \"addresses\" : [           (json array of string)\n"
            "           \"12tvKAXCxZjSmdNbao16dKXC8tRWfcF5oc\"   (string) bitcoin address\n"
            "           ,...\n"
            "         ]\n"
            "       }\n"
            "     }\n"
            "     ,...\n"
            "  ],\n"
            "}\n"

            "\nExamples:\n"
            + HelpExampleCli("decoderawtransaction", "\"hexstring\"")
            + HelpExampleRpc("decoderawtransaction", "\"hexstring\"")
        );

    LOCK(cs_main);
    RPCTypeCheck(request.params, {UniValue::VSTR, UniValue::VBOOL});

    CMutableTransaction mtx;

    bool try_witness = request.params[1].isNull() ? true : request.params[1].get_bool();
    bool try_no_witness = request.params[1].isNull() ? true : !request.params[1].get_bool();

    if (!DecodeHexTx(mtx, request.params[0].get_str(), try_no_witness, try_witness)) {
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "TX decode failed");
    }

    UniValue result(UniValue::VOBJ);
    TxToUniv(CTransaction(std::move(mtx)), uint256(), result, false);

    return result;
}

CScript ParseHexScript(const UniValue &v, std::string strName)
{
    if (v.get_str().size() > 0) {
        std::vector<unsigned char> scriptData(ParseHexV(v, strName));
        return CScript(scriptData.begin(), scriptData.end());
    } else {
        // Empty scripts are valid
    }
    return CScript();
}

UniValue decodescript(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
            "decodescript \"hexstring\"\n"
            "\nDecode a hex-encoded script.\n"
            "\nArguments:\n"
            "1. \"hexstring\"     (string) the hex encoded script\n"
            "\nResult:\n"
            "{\n"
            "  \"asm\":\"asm\",   (string) Script public key\n"
            "  \"hex\":\"hex\",   (string) hex encoded public key\n"
            "  \"type\":\"type\", (string) The output type\n"
            "  \"reqSigs\": n,    (numeric) The required signatures\n"
            "  \"addresses\": [   (json array of string)\n"
            "     \"address\"     (string) bitcoin address\n"
            "     ,...\n"
            "  ],\n"
            "  \"p2sh\",\"address\" (string) address of P2SH script wrapping this redeem script (not returned if the script is already a P2SH).\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("decodescript", "\"hexstring\"")
            + HelpExampleRpc("decodescript", "\"hexstring\"")
        );

    RPCTypeCheck(request.params, {UniValue::VSTR});

    UniValue r(UniValue::VOBJ);
    CScript script = ParseHexScript(request.params[0], "argument");
    ScriptPubKeyToUniv(script, r, false);

    UniValue type;
    type = find_value(r, "type");

    if (type.isStr() && type.get_str() != "scripthash") {
        // P2SH cannot be wrapped in a P2SH. If this script is already a P2SH,
        // don't return the address for a P2SH of the P2SH.
        r.push_back(Pair("p2sh", EncodeDestination(CScriptID(script))));
        // P2SH and witness programs cannot be wrapped in P2WSH, if this script
        // is a witness program, don't return addresses for a segwit programs.
        if (type.get_str() == "pubkey" || type.get_str() == "pubkeyhash" || type.get_str() == "multisig" || type.get_str() == "nonstandard") {
            txnouttype which_type;
            std::vector<std::vector<unsigned char>> solutions_data;
            Solver(script, which_type, solutions_data);
            // Uncompressed pubkeys cannot be used with segwit checksigs.
            // If the script contains an uncompressed pubkey, skip encoding of a segwit program.
            if ((which_type == TX_PUBKEY) || (which_type == TX_MULTISIG)) {
                for (const auto& solution : solutions_data) {
                    if ((solution.size() != 1) && !CPubKey(solution).IsCompressed()) {
                        return r;
                    }
                }
            }
            UniValue sr(UniValue::VOBJ);
            CScript segwitScr;
            if (which_type == TX_PUBKEY) {
                segwitScr = GetScriptForDestination(WitnessV0KeyHash(Hash160(solutions_data[0].begin(), solutions_data[0].end())));
            } else if (which_type == TX_PUBKEYHASH) {
                segwitScr = GetScriptForDestination(WitnessV0KeyHash(solutions_data[0]));
            } else {
                // Scripts that are not fit for P2WPKH are encoded as P2WSH.
                // Newer segwit program versions should be considered when then become available.
                uint256 scriptHash;
                CSHA256().Write(script.data(), script.size()).Finalize(scriptHash.begin());
                segwitScr = GetScriptForDestination(WitnessV0ScriptHash(scriptHash));
            }
            ScriptPubKeyToUniv(segwitScr, sr, true);
            sr.pushKV("p2sh-segwit", EncodeDestination(CScriptID(segwitScr)));
            r.pushKV("segwit", sr);
        }
    }

    return r;
}

class verifyscript_ScriptExecutionDebugger final : public ScriptExecutionDebugger {
private:
    UniValue result;
    UniValue current_script;
    UniValue current_steps;

    void CompleteScript() {
        if (!current_script.isNull()) {
            current_script.pushKV("steps", current_steps);
            result.push_back(current_script);
        }
    }

    static UniValue StackToUniValue(ScriptExecution::StackType& stack) {
        UniValue rv(UniValue::VARR);
        for (const auto elem : stack) {
            rv.push_back(HexStr(elem.begin(), elem.end()));
        }
        return rv;
    }

    static UniValue vfExecToUniValue(std::vector<bool>& vfExec) {
        UniValue rv(UniValue::VARR);
        for (const auto elem : vfExec) {
            bool elem_bool = elem;
            rv.push_back(elem_bool);
        }
        return rv;
    }

    static void PushExInfo(UniValue& step_info, ScriptExecution& ex, const CScript::const_iterator& pos) {
        step_info.pushKV("pos", (int64_t)(pos - ex.script.begin()));
        step_info.pushKV("pos_codehash", (int64_t)(ex.pbegincodehash - ex.script.begin()));
        step_info.pushKV("opcount", ex.nOpCount);
        step_info.pushKV("stack", StackToUniValue(ex.stack));
        step_info.pushKV("altstack", StackToUniValue(ex.altstack));
        step_info.pushKV("exec", vfExecToUniValue(ex.vfExec));
    }

public:

    verifyscript_ScriptExecutionDebugger() : result(UniValue::VARR) {}

    void ScriptBegin(ScriptExecution& ex) {
        CompleteScript();

        current_script = UniValue(UniValue::VOBJ);

        current_script.pushKV("context", ScriptExecution::ContextString(ex.context));

        UniValue script_info(UniValue::VOBJ);
        script_info.pushKV("asm", ScriptToAsmStr(ex.script));
        script_info.pushKV("hex", HexStr(ex.script.begin(), ex.script.end()));
        current_script.pushKV("script", script_info);

        current_script.pushKV("sigversion", SigVersionString(ex.sigversion));
    }

    void ScriptPreStep(ScriptExecution& ex, const CScript::const_iterator& pos, opcodetype& opcode, ScriptExecution::StackElementType& pushelem) {
        UniValue step_info(UniValue::VOBJ);
        PushExInfo(step_info, ex, pos);

        std::vector<uint8_t> opcode_arr;
        opcode_arr.push_back(opcode);
        step_info.pushKV("next_op", GetOpName(opcode));
        step_info.pushKV("next_opcode", HexStr(opcode_arr.begin(), opcode_arr.end()));
        step_info.pushKV("next_push", HexStr(pushelem.begin(), pushelem.end()));

        current_steps.push_back(step_info);
    }

    void ScriptEOF(ScriptExecution& ex, const CScript::const_iterator& pos) {
        UniValue step_info(UniValue::VOBJ);
        PushExInfo(step_info, ex, pos);
        current_steps.push_back(step_info);
    }

    const UniValue& GetResult() {
        CompleteScript();
        return result;
    }
};

UniValue verifyscript(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
            "verifyscript options\n"
            "\nExecute a script and return the result.\n"
            "\nArguments:\n"
            "1. options      (object, required)\n"
            "   {\n"
            "     \"input\": {  (object, required) Information on the input being spent.\n"
            "       \"scriptPubKey\": \"hex\",  (string, required) Hex encoded pubkey script.\n"
            "       \"amount\": value,        (numeric, optional) The amount spent. Required for VERIFY_WITNESS flag.\n"
            "       \"txid\": \"id\",           (string, this or \"transaction\" required) The input's transaction id.\n"
            "       \"transaction\": \"hex\",   (string, this or \"txid\" required) The input's raw transaction.\n"
            "       \"vout\": n               (numeric, required) The output number.\n"
            "     }\n"
            "     \"flags\": [  (array, optional) Zero or more of:"
            "       \"BIP16\", \"STRICTENC\", \"DERSIR\", \"LOW_S\", \"SIGPUSHONLY\", \"MINIMALDATA\", \"NULLDUMMY\",\"DISCOURAGE_UPGRADABLE_NOPS\", \"CLEANSTACK\", \"MINIMALIF\", \"NULLFAIL\", \"CHECKLOCKTIMEVERIFY\", \"CHECKSEQUENCEVERIFY\", \"WITNESS\", \"DISCOURAGE_UPGRADABLE_WITNESS_PROGRAM\", \"WITNESS_PUBKEYTYPE\",\n"
            "       \"NONE\",     Indicates explicitly that no flags should be enabled.\n"
            "       \"ALL\"       Indicates all supported flags should be enabled.\n"
            "     ]\n"
            "     \"trace\": true|false,  (boolean) Whether to trace execution and return \"trace\" in the result.\n"
            "   }\n"
            "\nResult:\n"
            "{\n"
            "  \"result\": true|false,  (boolean) Whether the spend is allowed.\n"
            "  \"error\": \"code\",       (string) What error, if any, caused the script to fail.\n"
            "  \"trace\": [\n"
            "    {\n"
            "      \"context\": \"Sig\"|\"PubKey\"|\"BIP16\"|\"Segwit\",  (string) Context the script is being executed in.\n"
            "      \"script\": {                                  (object) Script being executed.\n"
            "         \"asm\": \"asm\",  (string) asm\n"
            "         \"hex\": \"hex\"   (string) hex\n"
            "      },"
            "      \"sigversion\": \"Base\"|\"Witness_V0\",           (string) Signature hashing algorithm.\n"
            "      \"steps\": [\n"
            "        {\n"
            "          \"pos\": n,              (numeric) Execution position into script, in bytes.\n"
            "          \"pos_codehash\": n,     (numeric) Code hashing start position into script, in bytes.\n"
            "          \"opcount\": n,          (numeric) Number of sigops executed so far.\n"
            "          \"stack\": [             (array) Items on the stack.\n"
            "            \"hex\", ...           (string) Hex encoded data on the stack.\n"
            "          ],\n"
            "          \"altstack\": [          (array) Items on the alternate stack.\n"
            "            \"hex\", ...           (string) Hex encoded data on the stack.\n"
            "          ],\n"
            "          \"exec\": [              (array) State of active conditionals.\n"
            "            true|false, ...      (boolean) If any of these are false, the next instruction will be skipped.\n"
            "          ],\n"
            "          \"next_op\": \"asm\",      (string) Opcode to be processed next, as a single asm instruction.\n"
            "          \"next_opcode\": \"hex\",  (string) Hex encoded opcode to be processed next.\n"
            "          \"next_push\": \"hex\",    (string) Hex encoded data to be pushed with opcode.\n"
            "        }, ...\n"
            "      ]\n"
            "    }, ...\n"
            "  ]\n"
            "}\n"
        );

    RPCTypeCheck(request.params, {UniValue::VOBJ});

    const UniValue & options = request.params[0];

    const UniValue & flags_uv = find_value(options, "flags");
    unsigned int flags = 0;
    if (flags_uv.isArray()) {
        for (unsigned int i = flags_uv.size(); i-- > 0; ) {
            const UniValue & flag_uv = flags_uv[i];
            flags |= ParseScriptFlag(flag_uv.get_str());
        }
    } else if (!flags_uv.isNull()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "expected array for \"flags\"");
    }

    const UniValue & input_uv = find_value(options, "input");
    if (!input_uv.isObject()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "expected object for \"input\"");
    }
    const UniValue & sPK_uv = find_value(input_uv, "scriptPubKey");
    if (!sPK_uv.isStr()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "expected string for \"input\" \"scriptPubKey\"");
    }
    const CScript scriptPubKey = ParseHexScript(sPK_uv, "scriptPubKey");
    const UniValue & amount_uv = find_value(input_uv, "amount");
    CAmount amount = 0;
    if (amount_uv.isNum()) {
        amount = amount_uv.get_int64();
    } else if (amount_uv.isNull()) {
        if (flags & SCRIPT_VERIFY_WITNESS) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "\"input\" \"amount\" is required for VERIFY_WITNESS flag");
        }
    } else {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "expected number for \"input\" \"amount\"");
    }

    const UniValue & txid_uv = find_value(input_uv, "txid");
    const UniValue & rawtx_uv = find_value(input_uv, "transaction");
    CMutableTransaction mtx;
    CTransactionRef tx;
    if (rawtx_uv.isStr()) {
        if (!DecodeHexTx(mtx, rawtx_uv.get_str(), true)) {
            throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "TX decode failed");
        }
        if (txid_uv.isStr()) {
            const uint256 txid = ParseHashV(txid_uv, "txid");
            if (mtx.GetHash() != txid) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "\"input\" \"txid\" and \"transaction\" strings contradict");
            }
        }
        tx = MakeTransactionRef(std::move(mtx));
    } else if (txid_uv.isStr()) {
        const uint256 txid = ParseHashV(txid_uv, "txid");
        uint256 hashBlock;
        if (!GetTransaction(txid, tx, Params().GetConsensus(), hashBlock, true)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Cannot find transaction specified by \"txid\"");
        }
    } else {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "expected \"input\" with either \"txid\" or \"transaction\" string");
    }

    const UniValue & vout_uv = find_value(input_uv, "vout");
    if (!vout_uv.isNum()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "expected number for \"input\" \"vout\"");
    }
    unsigned int vout = vout_uv.get_int64();

    const bool do_trace = find_value(options, "trace").isTrue();
    verifyscript_ScriptExecutionDebugger debugger;

    PrecomputedTransactionData txdata(*tx);
    ScriptError err;
    const bool rv = VerifyScript(tx->vin[vout].scriptSig, scriptPubKey, &tx->vin[vout].scriptWitness, flags, TransactionSignatureChecker(&*tx, vout, amount, txdata), &err, (do_trace ? &debugger : nullptr));

    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("result", rv));

    if (!rv) {
        result.push_back(Pair("error", std::string(ScriptErrorString(err))));
    }

    if (do_trace) {
        result.pushKV("trace", debugger.GetResult());
    }

    return result;
}

/** Pushes a JSON object for script verification or signing errors to vErrorsRet. */
static void TxInErrorToJSON(const CTxIn& txin, UniValue& vErrorsRet, const std::string& strMessage)
{
    UniValue entry(UniValue::VOBJ);
    entry.push_back(Pair("txid", txin.prevout.hash.ToString()));
    entry.push_back(Pair("vout", (uint64_t)txin.prevout.n));
    UniValue witness(UniValue::VARR);
    for (unsigned int i = 0; i < txin.scriptWitness.stack.size(); i++) {
        witness.push_back(HexStr(txin.scriptWitness.stack[i].begin(), txin.scriptWitness.stack[i].end()));
    }
    entry.push_back(Pair("witness", witness));
    entry.push_back(Pair("scriptSig", HexStr(txin.scriptSig.begin(), txin.scriptSig.end())));
    entry.push_back(Pair("sequence", (uint64_t)txin.nSequence));
    entry.push_back(Pair("error", strMessage));
    vErrorsRet.push_back(entry);
}

UniValue combinerawtransaction(const JSONRPCRequest& request)
{

    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
            "combinerawtransaction [\"hexstring\",...]\n"
            "\nCombine multiple partially signed transactions into one transaction.\n"
            "The combined transaction may be another partially signed transaction or a \n"
            "fully signed transaction."

            "\nArguments:\n"
            "1. \"txs\"         (string) A json array of hex strings of partially signed transactions\n"
            "    [\n"
            "      \"hexstring\"     (string) A transaction hash\n"
            "      ,...\n"
            "    ]\n"

            "\nResult:\n"
            "\"hex\"            (string) The hex-encoded raw transaction with signature(s)\n"

            "\nExamples:\n"
            + HelpExampleCli("combinerawtransaction", "[\"myhex1\", \"myhex2\", \"myhex3\"]")
        );


    UniValue txs = request.params[0].get_array();
    std::vector<CMutableTransaction> txVariants(txs.size());

    for (unsigned int idx = 0; idx < txs.size(); idx++) {
        if (!DecodeHexTx(txVariants[idx], txs[idx].get_str(), true)) {
            throw JSONRPCError(RPC_DESERIALIZATION_ERROR, strprintf("TX decode failed for tx %d", idx));
        }
    }

    if (txVariants.empty()) {
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Missing transactions");
    }

    // mergedTx will end up with all the signatures; it
    // starts as a clone of the rawtx:
    CMutableTransaction mergedTx(txVariants[0]);

    // Fetch previous transactions (inputs):
    CCoinsView viewDummy;
    CCoinsViewCache view(&viewDummy);
    {
        LOCK(cs_main);
        LOCK(mempool.cs);
        CCoinsViewCache &viewChain = *pcoinsTip;
        CCoinsViewMemPool viewMempool(&viewChain, mempool);
        view.SetBackend(viewMempool); // temporarily switch cache backend to db+mempool view

        for (const CTxIn& txin : mergedTx.vin) {
            view.AccessCoin(txin.prevout); // Load entries from viewChain into view; can fail.
        }

        view.SetBackend(viewDummy); // switch back to avoid locking mempool for too long
    }

    // Use CTransaction for the constant parts of the
    // transaction to avoid rehashing.
    const CTransaction txConst(mergedTx);
    // Sign what we can:
    for (unsigned int i = 0; i < mergedTx.vin.size(); i++) {
        CTxIn& txin = mergedTx.vin[i];
        const Coin& coin = view.AccessCoin(txin.prevout);
        if (coin.IsSpent()) {
            throw JSONRPCError(RPC_VERIFY_ERROR, "Input not found or already spent");
        }
        const CScript& prevPubKey = coin.out.scriptPubKey;
        const CAmount& amount = coin.out.nValue;

        SignatureData sigdata;

        // ... and merge in other signatures:
        for (const CMutableTransaction& txv : txVariants) {
            if (txv.vin.size() > i) {
                sigdata = CombineSignatures(prevPubKey, TransactionSignatureChecker(&txConst, i, amount), sigdata, DataFromTransaction(txv, i));
            }
        }

        UpdateTransaction(mergedTx, i, sigdata);
    }

    return EncodeHexTx(mergedTx);
}

UniValue signrawtransaction(const JSONRPCRequest& request)
{
#ifdef ENABLE_WALLET
    CWallet * const pwallet = GetWalletForJSONRPCRequest(request);
#endif

    if (request.fHelp || request.params.size() < 1 || request.params.size() > 4)
        throw std::runtime_error(
            "signrawtransaction \"hexstring\" ( [{\"txid\":\"id\",\"vout\":n,\"scriptPubKey\":\"hex\",\"redeemScript\":\"hex\"},...] [\"privatekey1\",...] sighashtype )\n"
            "\nSign inputs for raw transaction (serialized, hex-encoded).\n"
            "The second optional argument (may be null) is an array of previous transaction outputs that\n"
            "this transaction depends on but may not yet be in the block chain.\n"
            "The third optional argument (may be null) is an array of base58-encoded private\n"
            "keys that, if given, will be the only keys used to sign the transaction.\n"
#ifdef ENABLE_WALLET
            + HelpRequiringPassphrase(pwallet) + "\n"
#endif

            "\nArguments:\n"
            "1. \"hexstring\"     (string, required) The transaction hex string\n"
            "2. \"prevtxs\"       (string, optional) An json array of previous dependent transaction outputs\n"
            "     [               (json array of json objects, or 'null' if none provided)\n"
            "       {\n"
            "         \"txid\":\"id\",             (string, required) The transaction id\n"
            "         \"vout\":n,                  (numeric, required) The output number\n"
            "         \"scriptPubKey\": \"hex\",   (string, required) script key\n"
            "         \"redeemScript\": \"hex\",   (string, required for P2SH or P2WSH) redeem script\n"
            "         \"amount\": value            (numeric, required) The amount spent\n"
            "       }\n"
            "       ,...\n"
            "    ]\n"
            "3. \"privkeys\"     (string, optional) A json array of base58-encoded private keys for signing\n"
            "    [                  (json array of strings, or 'null' if none provided)\n"
            "      \"privatekey\"   (string) private key in base58-encoding\n"
            "      ,...\n"
            "    ]\n"
            "4. \"sighashtype\"     (string, optional, default=ALL) The signature hash type. Must be one of\n"
            "       \"ALL\"\n"
            "       \"NONE\"\n"
            "       \"SINGLE\"\n"
            "       \"ALL|ANYONECANPAY\"\n"
            "       \"NONE|ANYONECANPAY\"\n"
            "       \"SINGLE|ANYONECANPAY\"\n"

            "\nResult:\n"
            "{\n"
            "  \"hex\" : \"value\",           (string) The hex-encoded raw transaction with signature(s)\n"
            "  \"complete\" : true|false,   (boolean) If the transaction has a complete set of signatures\n"
            "  \"fee\" : n,                 (numeric) The fee (input amounts minus output amounts), if known\n"
            "  \"feerate\" : x.x,           (numeric) The fee rate (in " + CURRENCY_UNIT + "/kB), if fee is known\n"
            "  \"errors\" : [                 (json array of objects) Script verification errors (if there are any)\n"
            "    {\n"
            "      \"txid\" : \"hash\",           (string) The hash of the referenced, previous transaction\n"
            "      \"vout\" : n,                (numeric) The index of the output to spent and used as input\n"
            "      \"scriptSig\" : \"hex\",       (string) The hex-encoded signature script\n"
            "      \"sequence\" : n,            (numeric) Script sequence number\n"
            "      \"error\" : \"text\"           (string) Verification or signing error related to the input\n"
            "    }\n"
            "    ,...\n"
            "  ]\n"
            "}\n"

            "\nExamples:\n"
            + HelpExampleCli("signrawtransaction", "\"myhex\"")
            + HelpExampleRpc("signrawtransaction", "\"myhex\"")
        );

    ObserveSafeMode();
#ifdef ENABLE_WALLET
    LOCK2(cs_main, pwallet ? &pwallet->cs_wallet : nullptr);
#else
    LOCK(cs_main);
#endif
    RPCTypeCheck(request.params, {UniValue::VSTR, UniValue::VARR, UniValue::VARR, UniValue::VSTR}, true);

    CMutableTransaction mtx;
    if (!DecodeHexTx(mtx, request.params[0].get_str(), true))
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "TX decode failed");

    // Fetch previous transactions (inputs):
    CCoinsView viewDummy;
    CCoinsViewCache view(&viewDummy);
    {
        LOCK(mempool.cs);
        CCoinsViewCache &viewChain = *pcoinsTip;
        CCoinsViewMemPool viewMempool(&viewChain, mempool);
        view.SetBackend(viewMempool); // temporarily switch cache backend to db+mempool view

        for (const CTxIn& txin : mtx.vin) {
            view.AccessCoin(txin.prevout); // Load entries from viewChain into view; can fail.
        }

        view.SetBackend(viewDummy); // switch back to avoid locking mempool for too long
    }

    bool fGivenKeys = false;
    CBasicKeyStore tempKeystore;
    if (!request.params[2].isNull()) {
        fGivenKeys = true;
        UniValue keys = request.params[2].get_array();
        for (unsigned int idx = 0; idx < keys.size(); idx++) {
            UniValue k = keys[idx];
            CKey key;
            ParseWIFPrivKey(k.get_str(), key, nullptr);
            tempKeystore.AddKey(key);
        }
    }
#ifdef ENABLE_WALLET
    else if (pwallet) {
        EnsureWalletIsUnlocked(pwallet);
    }
#endif

    // Add previous txouts given in the RPC call:
    if (!request.params[1].isNull()) {
        UniValue prevTxs = request.params[1].get_array();
        for (unsigned int idx = 0; idx < prevTxs.size(); idx++) {
            const UniValue& p = prevTxs[idx];
            if (!p.isObject())
                throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "expected object with {\"txid'\",\"vout\",\"scriptPubKey\"}");

            UniValue prevOut = p.get_obj();

            RPCTypeCheckObj(prevOut,
                {
                    {"txid", UniValueType(UniValue::VSTR)},
                    {"vout", UniValueType(UniValue::VNUM)},
                    {"scriptPubKey", UniValueType(UniValue::VSTR)},
                });

            uint256 txid = ParseHashO(prevOut, "txid");

            int nOut = find_value(prevOut, "vout").get_int();
            if (nOut < 0)
                throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "vout must be positive");

            COutPoint out(txid, nOut);
            std::vector<unsigned char> pkData(ParseHexO(prevOut, "scriptPubKey"));
            CScript scriptPubKey(pkData.begin(), pkData.end());

            {
                const Coin& coin = view.AccessCoin(out);
                if (!coin.IsSpent() && coin.out.scriptPubKey != scriptPubKey) {
                    std::string err("Previous output scriptPubKey mismatch:\n");
                    err = err + ScriptToAsmStr(coin.out.scriptPubKey) + "\nvs:\n"+
                        ScriptToAsmStr(scriptPubKey);
                    throw JSONRPCError(RPC_DESERIALIZATION_ERROR, err);
                }
                Coin newcoin;
                newcoin.out.scriptPubKey = scriptPubKey;
                newcoin.out.nValue = 0;
                if (prevOut.exists("amount")) {
                    newcoin.out.nValue = AmountFromValue(find_value(prevOut, "amount"));
                }
                newcoin.nHeight = 1;
                view.AddCoin(out, std::move(newcoin), true);
            }

            // if redeemScript given and not using the local wallet (private keys
            // given), add redeemScript to the tempKeystore so it can be signed:
            if (fGivenKeys && (scriptPubKey.IsPayToScriptHash() || scriptPubKey.IsPayToWitnessScriptHash())) {
                RPCTypeCheckObj(prevOut,
                    {
                        {"txid", UniValueType(UniValue::VSTR)},
                        {"vout", UniValueType(UniValue::VNUM)},
                        {"scriptPubKey", UniValueType(UniValue::VSTR)},
                        {"redeemScript", UniValueType(UniValue::VSTR)},
                    });
                UniValue v = find_value(prevOut, "redeemScript");
                if (!v.isNull()) {
                    std::vector<unsigned char> rsData(ParseHexV(v, "redeemScript"));
                    CScript redeemScript(rsData.begin(), rsData.end());
                    tempKeystore.AddCScript(redeemScript);
                    // Automatically also add the P2WSH wrapped version of the script (to deal with P2SH-P2WSH).
                    tempKeystore.AddCScript(GetScriptForWitness(redeemScript));
                }
            }
        }
    }

#ifdef ENABLE_WALLET
    const CKeyStore& keystore = ((fGivenKeys || !pwallet) ? tempKeystore : *pwallet);
#else
    const CKeyStore& keystore = tempKeystore;
#endif

    int nHashType = SIGHASH_ALL;
    if (!request.params[3].isNull()) {
        nHashType = ParseSigHash(request.params[3].get_str(), "sighashtype");
    }

    bool fHashSingle = ((nHashType & ~SIGHASH_ANYONECANPAY) == SIGHASH_SINGLE);

    // Script verification errors
    UniValue vErrors(UniValue::VARR);

    // Use CTransaction for the constant parts of the
    // transaction to avoid rehashing.
    const CTransaction txConst(mtx);
    // Sign what we can:
    CAmount inout_amount = 0;
    bool known_inputs = true;
    for (unsigned int i = 0; i < mtx.vin.size(); i++) {
        CTxIn& txin = mtx.vin[i];
        const Coin& coin = view.AccessCoin(txin.prevout);
        if (coin.IsSpent()) {
            known_inputs = false;
            TxInErrorToJSON(txin, vErrors, "Input not found or already spent");
            continue;
        }
        const CScript& prevPubKey = coin.out.scriptPubKey;
        const CAmount& amount = coin.out.nValue;
        inout_amount += amount;
        known_inputs &= amount > 0;

        SignatureData sigdata;
        // Only sign SIGHASH_SINGLE if there's a corresponding output:
        if (!fHashSingle || (i < mtx.vout.size()))
            ProduceSignature(MutableTransactionSignatureCreator(&keystore, &mtx, i, amount, nHashType), prevPubKey, sigdata);
        sigdata = CombineSignatures(prevPubKey, TransactionSignatureChecker(&txConst, i, amount), sigdata, DataFromTransaction(mtx, i));

        UpdateTransaction(mtx, i, sigdata);

        ScriptError serror = SCRIPT_ERR_OK;
        if (!VerifyScript(txin.scriptSig, prevPubKey, &txin.scriptWitness, STANDARD_SCRIPT_VERIFY_FLAGS, TransactionSignatureChecker(&txConst, i, amount), &serror)) {
            if (serror == SCRIPT_ERR_INVALID_STACK_OPERATION) {
                // Unable to sign input and verification failed (possible attempt to partially sign).
                TxInErrorToJSON(txin, vErrors, "Unable to sign input, invalid stack size (possibly missing key)");
            } else {
                TxInErrorToJSON(txin, vErrors, ScriptErrorString(serror));
            }
        }
    }
    bool fComplete = vErrors.empty();

    UniValue result(UniValue::VOBJ);
    std::string hex = EncodeHexTx(mtx);
    result.pushKV("hex", hex);
    result.push_back(Pair("complete", fComplete));
    if (known_inputs) {
        for (const CTxOut& txout : mtx.vout) {
            inout_amount -= txout.nValue;
        }
        result.pushKV("fee", ValueFromAmount(inout_amount));
        result.pushKV("feerate", ValueFromAmount(inout_amount*1000/(hex.length()>>1)));
    }
    if (!vErrors.empty()) {
        result.push_back(Pair("errors", vErrors));
    }

    return result;
}

UniValue sendrawtransaction(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 1 || request.params.size() > 2)
        throw std::runtime_error(
            "sendrawtransaction \"hexstring\" ( allowhighfees )\n"
            "\nSubmits raw transaction (serialized, hex-encoded) to local node and network.\n"
            "\nAlso see createrawtransaction and signrawtransaction calls.\n"
            "\nArguments:\n"
            "1. \"hexstring\"    (string, required) The hex string of the raw transaction)\n"
            "2. allowhighfees    (boolean, optional, default=false) Allow high fees\n"
            "\nResult:\n"
            "\"hex\"             (string) The transaction hash in hex\n"
            "\nExamples:\n"
            "\nCreate a transaction\n"
            + HelpExampleCli("createrawtransaction", "\"[{\\\"txid\\\" : \\\"mytxid\\\",\\\"vout\\\":0}]\" \"{\\\"myaddress\\\":0.01}\"") +
            "Sign the transaction, and get back the hex\n"
            + HelpExampleCli("signrawtransaction", "\"myhex\"") +
            "\nSend the transaction (signed hex)\n"
            + HelpExampleCli("sendrawtransaction", "\"signedhex\"") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("sendrawtransaction", "\"signedhex\"")
        );

    ObserveSafeMode();

    std::promise<void> promise;

    RPCTypeCheck(request.params, {UniValue::VSTR, UniValue::VBOOL});

    // parse hex string from parameter
    CMutableTransaction mtx;
    if (!DecodeHexTx(mtx, request.params[0].get_str()))
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "TX decode failed");
    CTransactionRef tx(MakeTransactionRef(std::move(mtx)));
    const uint256& hashTx = tx->GetHash();

    CAmount nMaxRawTxFee = maxTxFee;
    if (!request.params[1].isNull() && request.params[1].get_bool())
        nMaxRawTxFee = 0;

    { // cs_main scope
    LOCK(cs_main);
    CCoinsViewCache &view = *pcoinsTip;
    bool fHaveChain = false;
    for (size_t o = 0; !fHaveChain && o < tx->vout.size(); o++) {
        const Coin& existingCoin = view.AccessCoin(COutPoint(hashTx, o));
        fHaveChain = !existingCoin.IsSpent();
    }
    bool fHaveMempool = mempool.exists(hashTx);
    if (!fHaveMempool && !fHaveChain) {
        // push to local node and sync with wallets
        CValidationState state;
        bool fMissingInputs;
        if (!AcceptToMemoryPool(mempool, state, std::move(tx), &fMissingInputs,
                                nullptr /* plTxnReplaced */, false /* bypass_limits */, nMaxRawTxFee)) {
            if (state.IsInvalid()) {
                throw JSONRPCError(RPC_TRANSACTION_REJECTED, strprintf("%i: %s", state.GetRejectCode(), state.GetRejectReason()));
            } else {
                if (fMissingInputs) {
                    throw JSONRPCError(RPC_TRANSACTION_ERROR, "Missing inputs");
                }
                throw JSONRPCError(RPC_TRANSACTION_ERROR, state.GetRejectReason());
            }
        } else {
            // If wallet is enabled, ensure that the wallet has been made aware
            // of the new transaction prior to returning. This prevents a race
            // where a user might call sendrawtransaction with a transaction
            // to/from their wallet, immediately call some wallet RPC, and get
            // a stale result because callbacks have not yet been processed.
            CallFunctionInValidationInterfaceQueue([&promise] {
                promise.set_value();
            });
        }
    } else if (fHaveChain) {
        throw JSONRPCError(RPC_TRANSACTION_ALREADY_IN_CHAIN, "transaction already in block chain");
    } else {
        // Make sure we don't block forever if re-sending
        // a transaction already in mempool.
        promise.set_value();
    }

    } // cs_main

    promise.get_future().wait();

    if(!g_connman)
        throw JSONRPCError(RPC_CLIENT_P2P_DISABLED, "Error: Peer-to-peer functionality missing or disabled");

    CInv inv(MSG_TX, hashTx);
    g_connman->ForEachNode([&inv](CNode* pnode)
    {
        pnode->PushInventory(inv);
    });

    return hashTx.GetHex();
}

static const CRPCCommand commands[] =
{ //  category              name                      actor (function)         argNames
  //  --------------------- ------------------------  -----------------------  ----------
    { "rawtransactions",    "getrawtransaction",      &getrawtransaction,      {"txid","verbose","blockhash"} },
    { "rawtransactions",    "createrawtransaction",   &createrawtransaction,   {"inputs","outputs","locktime","replaceable"} },
    { "rawtransactions",    "decoderawtransaction",   &decoderawtransaction,   {"hexstring","iswitness"} },
    { "rawtransactions",    "decodescript",           &decodescript,           {"hexstring"} },
    { "rawtransactions",    "verifyscript",           &verifyscript,           {"options"} },
    { "rawtransactions",    "sendrawtransaction",     &sendrawtransaction,     {"hexstring","allowhighfees"} },
    { "rawtransactions",    "combinerawtransaction",  &combinerawtransaction,  {"txs"} },
    { "rawtransactions",    "signrawtransaction",     &signrawtransaction,     {"hexstring","prevtxs","privkeys","sighashtype"} }, /* uses wallet if enabled */

    { "blockchain",         "gettxoutproof",          &gettxoutproof,          {"txids", "blockhash"} },
    { "blockchain",         "verifytxoutproof",       &verifytxoutproof,       {"proof"} },
};

void RegisterRawTransactionRPCCommands(CRPCTable &t)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        t.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
