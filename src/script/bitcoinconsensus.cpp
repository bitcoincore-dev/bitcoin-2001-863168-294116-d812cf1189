// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <script/bitcoinconsensus.h>

#include <primitives/transaction.h>
#include <pubkey.h>
#include <script/interpreter.h>
#include <version.h>

namespace {

/** A class that deserializes a single CTransaction one time. */
class TxInputStream
{
public:
    TxInputStream(int nTypeIn, int nVersionIn, const unsigned char *txTo, size_t txToLen) :
    m_type(nTypeIn),
    m_version(nVersionIn),
    m_data(txTo),
    m_remaining(txToLen)
    {}

    void read(char* pch, size_t nSize)
    {
        if (nSize > m_remaining)
            throw std::ios_base::failure(std::string(__func__) + ": end of data");

        if (pch == nullptr)
            throw std::ios_base::failure(std::string(__func__) + ": bad destination buffer");

        if (m_data == nullptr)
            throw std::ios_base::failure(std::string(__func__) + ": bad source buffer");

        memcpy(pch, m_data, nSize);
        m_remaining -= nSize;
        m_data += nSize;
    }

    template<typename T>
    TxInputStream& operator>>(T& obj)
    {
        ::Unserialize(*this, obj);
        return *this;
    }

    int GetVersion() const { return m_version; }
    int GetType() const { return m_type; }
private:
    const int m_type;
    const int m_version;
    const unsigned char* m_data;
    size_t m_remaining;
};

inline int set_error(bitcoinconsensus_error* ret, bitcoinconsensus_error serror)
{
    if (ret)
        *ret = serror;
    return 0;
}

struct ECCryptoClosure
{
    ECCVerifyHandle handle;
};

ECCryptoClosure instance_of_eccryptoclosure;
} // namespace

/** Check that all specified flags are part of the libconsensus interface. */
static bool verify_flags(uint64_t flags)
{
    return (flags & ~(bitcoinconsensus_SCRIPT_FLAGS_VERIFY_ALL)) == 0;
}

class ScriptExecutionDebuggerForCAPI final : public ScriptExecutionDebugger {
public:
    const bitcoinconsensus_script_debugger_callbacks* const c_debugger;

    ScriptExecutionDebuggerForCAPI(const bitcoinconsensus_script_debugger_callbacks* const c_debugger_in, void * const userdata_in) : c_debugger(c_debugger_in) {
        userdata = userdata_in;
    }

    void ScriptBegin(ScriptExecution& ex) {
        c_debugger->ScriptBegin(userdata, (struct bitcoinconsensus_script_execution*)&ex);
    }

    void ScriptPreStep(ScriptExecution& ex, const CScript::const_iterator& pos, opcodetype& opcode, ScriptExecution::StackElementType& pushdata) {
        auto c_opcode = (uint8_t)opcode;
        auto c_pushdata_sz = (size_t)pushdata.size();
        c_debugger->ScriptPreStep(userdata, (struct bitcoinconsensus_script_execution*)&ex, (pos - ex.script.begin()), &c_opcode, pushdata.data(), &c_pushdata_sz);
    }

    void ScriptEOF(ScriptExecution& ex, const CScript::const_iterator& pos) {
        c_debugger->ScriptEOF(userdata, (struct bitcoinconsensus_script_execution*)&ex, (pos - ex.script.begin()));
    }
};

static int verify_script(const unsigned char *scriptPubKey, unsigned int scriptPubKeyLen, CAmount amount,
                                    const unsigned char *txTo        , unsigned int txToLen,
                                    unsigned int nIn, uint64_t flags, bitcoinconsensus_error* err, const struct bitcoinconsensus_script_debugger_callbacks* const c_debugger = nullptr, void * const userdata = nullptr)
{
    if (!verify_flags(flags)) {
        return bitcoinconsensus_ERR_INVALID_FLAGS;
    }
    ScriptExecutionDebuggerForCAPI *cpp_debugger = nullptr;
    if (c_debugger) {
        cpp_debugger = new ScriptExecutionDebuggerForCAPI(c_debugger, userdata);
    }
    int rv;
    try {
        TxInputStream stream(SER_NETWORK, PROTOCOL_VERSION, txTo, txToLen);
        CTransaction tx(deserialize, stream);
        if (nIn >= tx.vin.size())
            return set_error(err, bitcoinconsensus_ERR_TX_INDEX);
        if (GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION) != txToLen)
            return set_error(err, bitcoinconsensus_ERR_TX_SIZE_MISMATCH);

        // Regardless of the verification result, the tx did not error.
        set_error(err, bitcoinconsensus_ERR_OK);

        PrecomputedTransactionData txdata(tx);
        rv = VerifyScript(tx.vin[nIn].scriptSig, CScript(scriptPubKey, scriptPubKey + scriptPubKeyLen), &tx.vin[nIn].scriptWitness, flags, TransactionSignatureChecker(&tx, nIn, amount, txdata), nullptr, cpp_debugger);
    } catch (const std::exception&) {
        rv = set_error(err, bitcoinconsensus_ERR_TX_DESERIALIZE); // Error deserializing
    }
    delete cpp_debugger;
    return rv;
}

int bitcoinconsensus_trace_script(const unsigned char *scriptPubKey, unsigned int scriptPubKeyLen, int64_t amount, const unsigned char *txTo, unsigned int txToLen, unsigned int nIn, uint64_t flags, bitcoinconsensus_error* err, const struct bitcoinconsensus_script_debugger_callbacks* c_debugger, void *userdata)
{
    if (amount < 0) {
        if (flags & bitcoinconsensus_SCRIPT_FLAGS_VERIFY_WITNESS) {
            return set_error(err, bitcoinconsensus_ERR_AMOUNT_REQUIRED);
        }
        amount = 0;
    }
    CAmount am(amount);
    return ::verify_script(scriptPubKey, scriptPubKeyLen, am, txTo, txToLen, nIn, flags, err, c_debugger, userdata);
}

int bitcoinconsensus_verify_script_with_amount(const unsigned char *scriptPubKey, unsigned int scriptPubKeyLen, int64_t amount,
                                    const unsigned char *txTo        , unsigned int txToLen,
                                    unsigned int nIn, unsigned int flags, bitcoinconsensus_error* err)
{
    CAmount am(amount);
    return ::verify_script(scriptPubKey, scriptPubKeyLen, am, txTo, txToLen, nIn, flags, err);
}


int bitcoinconsensus_verify_script(const unsigned char *scriptPubKey, unsigned int scriptPubKeyLen,
                                   const unsigned char *txTo        , unsigned int txToLen,
                                   unsigned int nIn, unsigned int flags, bitcoinconsensus_error* err)
{
    if (flags & bitcoinconsensus_SCRIPT_FLAGS_VERIFY_WITNESS) {
        return set_error(err, bitcoinconsensus_ERR_AMOUNT_REQUIRED);
    }

    CAmount am(0);
    return ::verify_script(scriptPubKey, scriptPubKeyLen, am, txTo, txToLen, nIn, flags, err);
}

unsigned int bitcoinconsensus_version()
{
    // Just use the API version for now
    return BITCOINCONSENSUS_API_VER;
}
