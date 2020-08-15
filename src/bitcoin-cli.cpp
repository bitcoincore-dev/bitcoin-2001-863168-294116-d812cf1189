// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <amount.h>
#include <chainparamsbase.h>
#include <clientversion.h>
#include <optional.h>
#include <rpc/client.h>
#include <rpc/protocol.h>
#include <rpc/request.h>
#include <util/strencodings.h>
#include <util/string.h>
#include <util/system.h>
#include <util/translation.h>

#include <functional>
#include <memory>
#include <stdio.h>
#include <tuple>

#include <event2/buffer.h>
#include <event2/keyvalq_struct.h>
#include <support/events.h>

#include <univalue.h>
#include <compat/stdin.h>

const std::function<std::string(const char*)> G_TRANSLATION_FUN = nullptr;

static const char DEFAULT_RPCCONNECT[] = "127.0.0.1";
static const int DEFAULT_HTTP_CLIENT_TIMEOUT=900;
static const bool DEFAULT_NAMED=false;
static const int CONTINUE_EXECUTION=-1;

static void SetupCliArgs()
{
    SetupHelpOptions(gArgs);

    const auto defaultBaseParams = CreateBaseChainParams(CBaseChainParams::MAIN);
    const auto testnetBaseParams = CreateBaseChainParams(CBaseChainParams::TESTNET);
    const auto regtestBaseParams = CreateBaseChainParams(CBaseChainParams::REGTEST);

    gArgs.AddArg("-version", "Print version and exit", ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    gArgs.AddArg("-conf=<file>", strprintf("Specify configuration file. Relative paths will be prefixed by datadir location. (default: %s)", BITCOIN_CONF_FILENAME), ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    gArgs.AddArg("-datadir=<dir>", "Specify data directory", ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    gArgs.AddArg("-getinfo", "Get general information from the remote server, including the total balance and the balances of each loaded wallet when in multiwallet mode. Note that -getinfo is the combined result of several RPCs (getnetworkinfo, getblockchaininfo, getwalletinfo, getbalances, and in multiwallet mode, listwallets), each with potentially different state.", ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    SetupChainParamsBaseOptions();
    gArgs.AddArg("-named", strprintf("Pass named instead of positional arguments (default: %s)", DEFAULT_NAMED), ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    gArgs.AddArg("-netinfo", "Get network peer connection information from the remote server. An optional boolean argument can be passed for a detailed peers listing (default: false).", ArgsManager::ALLOW_BOOL, OptionsCategory::OPTIONS);
    gArgs.AddArg("-rpcclienttimeout=<n>", strprintf("Timeout in seconds during HTTP requests, or 0 for no timeout. (default: %d)", DEFAULT_HTTP_CLIENT_TIMEOUT), ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    gArgs.AddArg("-rpcconnect=<ip>", strprintf("Send commands to node running on <ip> (default: %s)", DEFAULT_RPCCONNECT), ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    gArgs.AddArg("-rpccookiefile=<loc>", "Location of the auth cookie. Relative paths will be prefixed by a net-specific datadir location. (default: data dir)", ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    gArgs.AddArg("-rpcpassword=<pw>", "Password for JSON-RPC connections", ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    gArgs.AddArg("-rpcport=<port>", strprintf("Connect to JSON-RPC on <port> (default: %u, testnet: %u, regtest: %u)", defaultBaseParams->RPCPort(), testnetBaseParams->RPCPort(), regtestBaseParams->RPCPort()), ArgsManager::ALLOW_ANY | ArgsManager::NETWORK_ONLY, OptionsCategory::OPTIONS);
    gArgs.AddArg("-rpcuser=<user>", "Username for JSON-RPC connections", ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    gArgs.AddArg("-rpcwait", "Wait for RPC server to start", ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    gArgs.AddArg("-rpcwallet=<walletname>", "Send RPC for non-default wallet on RPC server (needs to exactly match corresponding -wallet option passed to bitcoind). This changes the RPC endpoint used, e.g. http://127.0.0.1:8332/wallet/<walletname>", ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    gArgs.AddArg("-stdin", "Read extra arguments from standard input, one per line until EOF/Ctrl-D (recommended for sensitive information such as passphrases). When combined with -stdinrpcpass, the first line from standard input is used for the RPC password.", ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    gArgs.AddArg("-stdinrpcpass", "Read RPC password from standard input as a single line. When combined with -stdin, the first line from standard input is used for the RPC password. When combined with -stdinwalletpassphrase, -stdinrpcpass consumes the first line, and -stdinwalletpassphrase consumes the second.", ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    gArgs.AddArg("-stdinwalletpassphrase", "Read wallet passphrase from standard input as a single line. When combined with -stdin, the first line from standard input is used for the wallet passphrase.", ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
}

/** libevent event log callback */
static void libevent_log_cb(int severity, const char *msg)
{
#ifndef EVENT_LOG_ERR // EVENT_LOG_ERR was added in 2.0.19; but before then _EVENT_LOG_ERR existed.
# define EVENT_LOG_ERR _EVENT_LOG_ERR
#endif
    // Ignore everything other than errors
    if (severity >= EVENT_LOG_ERR) {
        throw std::runtime_error(strprintf("libevent error: %s", msg));
    }
}

//////////////////////////////////////////////////////////////////////////////
//
// Start
//

//
// Exception thrown on connection error.  This error is used to determine
// when to wait if -rpcwait is given.
//
class CConnectionFailed : public std::runtime_error
{
public:

    explicit inline CConnectionFailed(const std::string& msg) :
        std::runtime_error(msg)
    {}

};

//
// This function returns either one of EXIT_ codes when it's expected to stop the process or
// CONTINUE_EXECUTION when it's expected to continue further.
//
static int AppInitRPC(int argc, char* argv[])
{
    //
    // Parameters
    //
    SetupCliArgs();
    std::string error;
    if (!gArgs.ParseParameters(argc, argv, error)) {
        tfm::format(std::cerr, "Error parsing command line arguments: %s\n", error);
        return EXIT_FAILURE;
    }
    if (argc < 2 || HelpRequested(gArgs) || gArgs.IsArgSet("-version")) {
        std::string strUsage = PACKAGE_NAME " RPC client version " + FormatFullVersion() + "\n";
        if (!gArgs.IsArgSet("-version")) {
            strUsage += "\n"
                "Usage:  bitcoin-cli [options] <command> [params]  Send command to " PACKAGE_NAME "\n"
                "or:     bitcoin-cli [options] -named <command> [name=value]...  Send command to " PACKAGE_NAME " (with named arguments)\n"
                "or:     bitcoin-cli [options] help                List commands\n"
                "or:     bitcoin-cli [options] help <command>      Get help for a command\n";
            strUsage += "\n" + gArgs.GetHelpMessage();
        }

        tfm::format(std::cout, "%s", strUsage);
        if (argc < 2) {
            tfm::format(std::cerr, "Error: too few parameters\n");
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    }
    if (!CheckDataDirOption()) {
        tfm::format(std::cerr, "Error: Specified data directory \"%s\" does not exist.\n", gArgs.GetArg("-datadir", ""));
        return EXIT_FAILURE;
    }
    if (!gArgs.ReadConfigFiles(error, true)) {
        tfm::format(std::cerr, "Error reading configuration file: %s\n", error);
        return EXIT_FAILURE;
    }
    // Check for -chain, -testnet or -regtest parameter (BaseParams() calls are only valid after this clause)
    try {
        SelectBaseParams(gArgs.GetChainName());
    } catch (const std::exception& e) {
        tfm::format(std::cerr, "Error: %s\n", e.what());
        return EXIT_FAILURE;
    }
    return CONTINUE_EXECUTION;
}


/** Reply structure for request_done to fill in */
struct HTTPReply
{
    HTTPReply(): status(0), error(-1) {}

    int status;
    int error;
    std::string body;
};

static const char *http_errorstring(int code)
{
    switch(code) {
#if LIBEVENT_VERSION_NUMBER >= 0x02010300
    case EVREQ_HTTP_TIMEOUT:
        return "timeout reached";
    case EVREQ_HTTP_EOF:
        return "EOF reached";
    case EVREQ_HTTP_INVALID_HEADER:
        return "error while reading header, or invalid header";
    case EVREQ_HTTP_BUFFER_ERROR:
        return "error encountered while reading or writing";
    case EVREQ_HTTP_REQUEST_CANCEL:
        return "request was canceled";
    case EVREQ_HTTP_DATA_TOO_LONG:
        return "response body is larger than allowed";
#endif
    default:
        return "unknown";
    }
}

static void http_request_done(struct evhttp_request *req, void *ctx)
{
    HTTPReply *reply = static_cast<HTTPReply*>(ctx);

    if (req == nullptr) {
        /* If req is nullptr, it means an error occurred while connecting: the
         * error code will have been passed to http_error_cb.
         */
        reply->status = 0;
        return;
    }

    reply->status = evhttp_request_get_response_code(req);

    struct evbuffer *buf = evhttp_request_get_input_buffer(req);
    if (buf)
    {
        size_t size = evbuffer_get_length(buf);
        const char *data = (const char*)evbuffer_pullup(buf, size);
        if (data)
            reply->body = std::string(data, size);
        evbuffer_drain(buf, size);
    }
}

#if LIBEVENT_VERSION_NUMBER >= 0x02010300
static void http_error_cb(enum evhttp_request_error err, void *ctx)
{
    HTTPReply *reply = static_cast<HTTPReply*>(ctx);
    reply->error = err;
}
#endif

/** Class that handles the conversion from a command-line to a JSON-RPC request,
 * as well as converting back to a JSON object that can be shown as result.
 */
class BaseRequestHandler
{
public:
    virtual ~BaseRequestHandler() {}
    virtual UniValue PrepareRequest(const std::string& method, const std::vector<std::string>& args) = 0;
    virtual UniValue ProcessReply(const UniValue &batch_in) = 0;
};

/** Process getinfo requests */
class GetinfoRequestHandler: public BaseRequestHandler
{
public:
    const int ID_NETWORKINFO = 0;
    const int ID_BLOCKCHAININFO = 1;
    const int ID_WALLETINFO = 2;
    const int ID_BALANCES = 3;

    /** Create a simulated `getinfo` request. */
    UniValue PrepareRequest(const std::string& method, const std::vector<std::string>& args) override
    {
        if (!args.empty()) {
            throw std::runtime_error("-getinfo takes no arguments");
        }
        UniValue result(UniValue::VARR);
        result.push_back(JSONRPCRequestObj("getnetworkinfo", NullUniValue, ID_NETWORKINFO));
        result.push_back(JSONRPCRequestObj("getblockchaininfo", NullUniValue, ID_BLOCKCHAININFO));
        result.push_back(JSONRPCRequestObj("getwalletinfo", NullUniValue, ID_WALLETINFO));
        result.push_back(JSONRPCRequestObj("getbalances", NullUniValue, ID_BALANCES));
        return result;
    }

    /** Collect values from the batch and form a simulated `getinfo` reply. */
    UniValue ProcessReply(const UniValue &batch_in) override
    {
        UniValue result(UniValue::VOBJ);
        std::vector<UniValue> batch = JSONRPCProcessBatchReply(batch_in, batch_in.size());
        // Errors in getnetworkinfo() and getblockchaininfo() are fatal, pass them on;
        // getwalletinfo() and getbalances() are allowed to fail if there is no wallet.
        if (!batch[ID_NETWORKINFO]["error"].isNull()) {
            return batch[ID_NETWORKINFO];
        }
        if (!batch[ID_BLOCKCHAININFO]["error"].isNull()) {
            return batch[ID_BLOCKCHAININFO];
        }
        result.pushKV("version", batch[ID_NETWORKINFO]["result"]["version"]);
        result.pushKV("blocks", batch[ID_BLOCKCHAININFO]["result"]["blocks"]);
        result.pushKV("headers", batch[ID_BLOCKCHAININFO]["result"]["headers"]);
        result.pushKV("verificationprogress", batch[ID_BLOCKCHAININFO]["result"]["verificationprogress"]);
        result.pushKV("timeoffset", batch[ID_NETWORKINFO]["result"]["timeoffset"]);

        UniValue connections(UniValue::VOBJ);
        connections.pushKV("in", batch[ID_NETWORKINFO]["result"]["connections_in"]);
        connections.pushKV("out", batch[ID_NETWORKINFO]["result"]["connections_out"]);
        connections.pushKV("total", batch[ID_NETWORKINFO]["result"]["connections"]);
        result.pushKV("connections", connections);

        result.pushKV("proxy", batch[ID_NETWORKINFO]["result"]["networks"][0]["proxy"]);
        result.pushKV("difficulty", batch[ID_BLOCKCHAININFO]["result"]["difficulty"]);
        result.pushKV("chain", UniValue(batch[ID_BLOCKCHAININFO]["result"]["chain"]));
        if (!batch[ID_WALLETINFO]["result"].isNull()) {
            result.pushKV("keypoolsize", batch[ID_WALLETINFO]["result"]["keypoolsize"]);
            if (!batch[ID_WALLETINFO]["result"]["unlocked_until"].isNull()) {
                result.pushKV("unlocked_until", batch[ID_WALLETINFO]["result"]["unlocked_until"]);
            }
            result.pushKV("paytxfee", batch[ID_WALLETINFO]["result"]["paytxfee"]);
        }
        if (!batch[ID_BALANCES]["result"].isNull()) {
            result.pushKV("balance", batch[ID_BALANCES]["result"]["mine"]["trusted"]);
        }
        result.pushKV("relayfee", batch[ID_NETWORKINFO]["result"]["relayfee"]);
        result.pushKV("warnings", batch[ID_NETWORKINFO]["result"]["warnings"]);
        return JSONRPCReplyObj(result, NullUniValue, 1);
    }
};

/** Process netinfo requests */
class NetinfoRequestHandler : public BaseRequestHandler
{
private:
    bool IsAddrIPv6(const std::string& addr) const
    {
        return addr.front() == '[';
    }
    bool IsInboundOnion(int mapped_as, const std::string& addr, const std::string& addr_local) const
    {
        return mapped_as == 0 && addr.find("127.0.0.1") == 0 && addr_local.find(".onion") != std::string::npos;
    }
    bool IsOutboundOnion(const std::string& addr) const
    {
        return addr.find(".onion") != std::string::npos;
    }
    bool m_verbose{false}; //!< Whether user requested verbose -netinfo report

    enum struct m_conn_type {
        ipv4,
        ipv6,
        onion,
    };

    struct m_peer {
        int id;
        int mapped_as;
        int version;
        int64_t conn_time;
        int64_t last_recv;
        int64_t last_send;
        double min_ping;
        double ping;
        std::string addr;
        std::string sub_version;
        m_conn_type conn_type;
        bool is_block_relay;
        bool is_outbound;
        bool operator<(const m_peer& rhs) const { return std::tie(is_outbound, min_ping) < std::tie(rhs.is_outbound, rhs.min_ping); }
    };

    std::string ConnTypeEnumToString(m_conn_type t)
    {
        switch (t) {
        case m_conn_type::ipv4: return "ipv4";
        case m_conn_type::ipv6: return "ipv6";
        case m_conn_type::onion: return "onion";
        } // no default case, so the compiler can warn about missing cases
        assert(false);
    }

public:
    const int ID_PEERINFO = 0;
    const int ID_NETWORKINFO = 1;

    UniValue PrepareRequest(const std::string& method, const std::vector<std::string>& args) override
    {
        if (!args.empty()) {
            const std::string arg{ToLower(args.at(0))};
            m_verbose = (arg == "true" || arg == "t");
        }
        UniValue result(UniValue::VARR);
        result.push_back(JSONRPCRequestObj("getpeerinfo", NullUniValue, ID_PEERINFO));
        result.push_back(JSONRPCRequestObj("getnetworkinfo", NullUniValue, ID_NETWORKINFO));
        return result;
    }

    UniValue ProcessReply(const UniValue& batch_in) override
    {
        const std::vector<UniValue> batch{JSONRPCProcessBatchReply(batch_in, batch_in.size())};
        if (!batch[ID_PEERINFO]["error"].isNull()) return batch[ID_PEERINFO];
        if (!batch[ID_NETWORKINFO]["error"].isNull()) return batch[ID_NETWORKINFO];
        // Count peer connection totals, and if m_verbose is true, store peer data in a vector of structs.
        int64_t time_now{GetSystemTimeInSeconds()};
        int ipv4_i{0}, ipv6_i{0}, onion_i{0}, block_relay_i{0}, total_i{0}; // inbound conn counters
        int ipv4_o{0}, ipv6_o{0}, onion_o{0}, block_relay_o{0}, total_o{0}; // outbound conn counters
        int max_peer_id_length{2}, max_version_length{1};
        bool is_asmap_on{false};
        std::vector<m_peer> peers;
        const UniValue& getpeerinfo{batch[ID_PEERINFO]["result"]};
        for (const UniValue& peer : getpeerinfo.getValues()) {
            const std::string addr{peer["addr"].get_str()};
            const std::string addr_local{peer["addrlocal"].isNull() ? "" : peer["addrlocal"].get_str()};
            const int mapped_as{peer["mapped_as"].isNull() ? 0 : peer["mapped_as"].get_int()};
            const bool is_block_relay{!peer["relaytxes"].get_bool()};
            const bool is_inbound{peer["inbound"].get_bool()};
            m_conn_type conn_type{m_conn_type::ipv4};
            if (is_inbound) {
                if (IsAddrIPv6(addr)) {
                    conn_type = m_conn_type::ipv6;
                    ipv6_i += 1;
                } else if (IsInboundOnion(mapped_as, addr, addr_local)) {
                    conn_type = m_conn_type::onion;
                    onion_i += 1;
                } else {
                    ipv4_i += 1;
                }
                if (is_block_relay) block_relay_i += 1;
            } else {
                if (IsAddrIPv6(addr)) {
                    conn_type = m_conn_type::ipv6;
                    ipv6_o += 1;
                } else if (IsOutboundOnion(addr)) {
                    conn_type = m_conn_type::onion;
                    onion_o += 1;
                } else {
                    ipv4_o += 1;
                }
                if (is_block_relay) block_relay_o += 1;
            }
            if (m_verbose) {
                // Push data for this peer to the peers vector.
                const int peer_id{peer["id"].get_int()};
                const int version{peer["version"].get_int()};
                const std::string sub_version{peer["subver"].get_str()};
                const int64_t conn_time{peer["conntime"].get_int64()};
                const int64_t last_recv{peer["lastrecv"].get_int64()};
                const int64_t last_send{peer["lastsend"].get_int64()};
                const double min_ping{peer["minping"].isNull() ? 0 : peer["minping"].get_real()};
                const double ping{peer["pingtime"].isNull() ? 0 : peer["pingtime"].get_real()};
                peers.push_back({peer_id, mapped_as, version, conn_time, last_recv, last_send, min_ping, ping, addr, sub_version, conn_type, is_block_relay, !is_inbound});

                is_asmap_on |= (mapped_as != 0);
                max_peer_id_length = std::max(int(ToString(peer_id).length()), max_peer_id_length);
                max_version_length = std::max(int((ToString(version) + sub_version).length()), max_version_length);
            }
        }
        // Generate reports.
        const UniValue& networkinfo{batch[ID_NETWORKINFO]["result"]};
        std::string result{strprintf("%s %s - %i%s\n\n", PACKAGE_NAME, FormatFullVersion(), networkinfo["protocolversion"].get_int(), networkinfo["subversion"].get_str())};

        // Report detailed peer connections list sorted by direction and minimum ping time.
        if (m_verbose) {
            std::sort(peers.begin(), peers.end());
            result += "Peer connections sorted by direction and min ping\n<-> relay  conn minping   ping lastsend lastrecv uptime ";
            if (is_asmap_on) result += " asmap ";
            result += strprintf("%*s %-*s address\n", max_peer_id_length, "id", max_version_length, "version");
            for (const m_peer& peer : peers) {
                result += strprintf(
                    "%3s %5s %5s%8d%7d %8s %8s%7s%*i %*s %-*s %s\n",
                    peer.is_outbound ? "out" : "in",
                    peer.is_block_relay ? "block" : "full",
                    ConnTypeEnumToString(peer.conn_type),
                    round(1000 * peer.min_ping),
                    round(1000 * peer.ping),
                    peer.last_send == 0 ? "" : ToString(time_now - peer.last_send),
                    peer.last_recv == 0 ? "" : ToString(time_now - peer.last_recv),
                    peer.conn_time == 0 ? "" : ToString((time_now - peer.conn_time) / 60),
                    is_asmap_on ? 7 : 0, // variable spacing
                    is_asmap_on && peer.mapped_as != 0 ? ToString(peer.mapped_as) : "",
                    max_peer_id_length, // variable spacing
                    peer.id,
                    max_version_length, // variable spacing
                    ToString(peer.version) + peer.sub_version,
                    peer.addr);
            }
            result += "                     ms     ms      sec      sec    min\n\n";
        }

        // Report peer connection totals by type.
        total_i = ipv4_i + ipv6_i + onion_i;
        total_o = ipv4_o + ipv6_o + onion_o;
        result += "Inbound and outbound peer connections\n";
        result += strprintf("in:  ipv4 %3i  |  ipv6 %3i  |  onion %3i  |  total %3i  (%i block-relay)\n", ipv4_i, ipv6_i, onion_i, total_i, block_relay_i);
        result += strprintf("out: ipv4 %3i  |  ipv6 %3i  |  onion %3i  |  total %3i  (%i block-relay)\n", ipv4_o, ipv6_o, onion_o, total_o, block_relay_o);
        result += strprintf("all: %i\n", total_i + total_o);

        // Report local addresses, ports, and scores.
        result += "\nLocal addresses";
        const UniValue& local_addrs{networkinfo["localaddresses"]};
        for (const UniValue& addr : local_addrs.getValues()) {
            result += strprintf("\n%-37i  |  port %5i  |  score %6i", addr["address"].get_str(), addr["port"].get_int(), addr["score"].get_int());
        }

        return JSONRPCReplyObj(UniValue{result}, NullUniValue, 1);
    }
};

/** Process default single requests */
class DefaultRequestHandler: public BaseRequestHandler {
public:
    UniValue PrepareRequest(const std::string& method, const std::vector<std::string>& args) override
    {
        UniValue params;
        if(gArgs.GetBoolArg("-named", DEFAULT_NAMED)) {
            params = RPCConvertNamedValues(method, args);
        } else {
            params = RPCConvertValues(method, args);
        }
        return JSONRPCRequestObj(method, params, 1);
    }

    UniValue ProcessReply(const UniValue &reply) override
    {
        return reply.get_obj();
    }
};

static UniValue CallRPC(BaseRequestHandler* rh, const std::string& strMethod, const std::vector<std::string>& args, const Optional<std::string>& rpcwallet = {})
{
    std::string host;
    // In preference order, we choose the following for the port:
    //     1. -rpcport
    //     2. port in -rpcconnect (ie following : in ipv4 or ]: in ipv6)
    //     3. default port for chain
    int port = BaseParams().RPCPort();
    SplitHostPort(gArgs.GetArg("-rpcconnect", DEFAULT_RPCCONNECT), port, host);
    port = gArgs.GetArg("-rpcport", port);

    // Obtain event base
    raii_event_base base = obtain_event_base();

    // Synchronously look up hostname
    raii_evhttp_connection evcon = obtain_evhttp_connection_base(base.get(), host, port);

    // Set connection timeout
    {
        const int timeout = gArgs.GetArg("-rpcclienttimeout", DEFAULT_HTTP_CLIENT_TIMEOUT);
        if (timeout > 0) {
            evhttp_connection_set_timeout(evcon.get(), timeout);
        } else {
            // Indefinite request timeouts are not possible in libevent-http, so we
            // set the timeout to a very long time period instead.

            constexpr int YEAR_IN_SECONDS = 31556952; // Average length of year in Gregorian calendar
            evhttp_connection_set_timeout(evcon.get(), 5 * YEAR_IN_SECONDS);
        }
    }

    HTTPReply response;
    raii_evhttp_request req = obtain_evhttp_request(http_request_done, (void*)&response);
    if (req == nullptr)
        throw std::runtime_error("create http request failed");
#if LIBEVENT_VERSION_NUMBER >= 0x02010300
    evhttp_request_set_error_cb(req.get(), http_error_cb);
#endif

    // Get credentials
    std::string strRPCUserColonPass;
    bool failedToGetAuthCookie = false;
    if (gArgs.GetArg("-rpcpassword", "") == "") {
        // Try fall back to cookie-based authentication if no password is provided
        if (!GetAuthCookie(&strRPCUserColonPass)) {
            failedToGetAuthCookie = true;
        }
    } else {
        strRPCUserColonPass = gArgs.GetArg("-rpcuser", "") + ":" + gArgs.GetArg("-rpcpassword", "");
    }

    struct evkeyvalq* output_headers = evhttp_request_get_output_headers(req.get());
    assert(output_headers);
    evhttp_add_header(output_headers, "Host", host.c_str());
    evhttp_add_header(output_headers, "Connection", "close");
    evhttp_add_header(output_headers, "Authorization", (std::string("Basic ") + EncodeBase64(strRPCUserColonPass)).c_str());

    // Attach request data
    std::string strRequest = rh->PrepareRequest(strMethod, args).write() + "\n";
    struct evbuffer* output_buffer = evhttp_request_get_output_buffer(req.get());
    assert(output_buffer);
    evbuffer_add(output_buffer, strRequest.data(), strRequest.size());

    // check if we should use a special wallet endpoint
    std::string endpoint = "/";
    if (rpcwallet) {
        char* encodedURI = evhttp_uriencode(rpcwallet->data(), rpcwallet->size(), false);
        if (encodedURI) {
            endpoint = "/wallet/"+ std::string(encodedURI);
            free(encodedURI);
        }
        else {
            throw CConnectionFailed("uri-encode failed");
        }
    }
    int r = evhttp_make_request(evcon.get(), req.get(), EVHTTP_REQ_POST, endpoint.c_str());
    req.release(); // ownership moved to evcon in above call
    if (r != 0) {
        throw CConnectionFailed("send http request failed");
    }

    event_base_dispatch(base.get());

    if (response.status == 0) {
        std::string responseErrorMessage;
        if (response.error != -1) {
            responseErrorMessage = strprintf(" (error code %d - \"%s\")", response.error, http_errorstring(response.error));
        }
        throw CConnectionFailed(strprintf("Could not connect to the server %s:%d%s\n\nMake sure the bitcoind server is running and that you are connecting to the correct RPC port.", host, port, responseErrorMessage));
    } else if (response.status == HTTP_UNAUTHORIZED) {
        if (failedToGetAuthCookie) {
            throw std::runtime_error(strprintf(
                "Could not locate RPC credentials. No authentication cookie could be found, and RPC password is not set.  See -rpcpassword and -stdinrpcpass.  Configuration file: (%s)",
                GetConfigFile(gArgs.GetArg("-conf", BITCOIN_CONF_FILENAME)).string()));
        } else {
            throw std::runtime_error("Authorization failed: Incorrect rpcuser or rpcpassword");
        }
    } else if (response.status == HTTP_SERVICE_UNAVAILABLE) {
        throw std::runtime_error(strprintf("Server response: %s", response.body));
    } else if (response.status >= 400 && response.status != HTTP_BAD_REQUEST && response.status != HTTP_NOT_FOUND && response.status != HTTP_INTERNAL_SERVER_ERROR)
        throw std::runtime_error(strprintf("server returned HTTP error %d", response.status));
    else if (response.body.empty())
        throw std::runtime_error("no response from server");

    // Parse reply
    UniValue valReply(UniValue::VSTR);
    if (!valReply.read(response.body))
        throw std::runtime_error("couldn't parse reply from server");
    const UniValue reply = rh->ProcessReply(valReply);
    if (reply.empty())
        throw std::runtime_error("expected reply to have result, error and id properties");

    return reply;
}

/**
 * ConnectAndCallRPC wraps CallRPC with -rpcwait and an exception handler.
 *
 * @param[in] rh         Pointer to RequestHandler.
 * @param[in] strMethod  Reference to const string method to forward to CallRPC.
 * @param[in] rpcwallet  Reference to const optional string wallet name to forward to CallRPC.
 * @returns the RPC response as a UniValue object.
 * @throws a CConnectionFailed std::runtime_error if connection failed or RPC server still in warmup.
 */
static UniValue ConnectAndCallRPC(BaseRequestHandler* rh, const std::string& strMethod, const std::vector<std::string>& args, const Optional<std::string>& rpcwallet = {})
{
    UniValue response(UniValue::VOBJ);
    // Execute and handle connection failures with -rpcwait.
    const bool fWait = gArgs.GetBoolArg("-rpcwait", false);
    do {
        try {
            response = CallRPC(rh, strMethod, args, rpcwallet);
            if (fWait) {
                const UniValue& error = find_value(response, "error");
                if (!error.isNull() && error["code"].get_int() == RPC_IN_WARMUP) {
                    throw CConnectionFailed("server in warmup");
                }
            }
            break; // Connection succeeded, no need to retry.
        } catch (const CConnectionFailed&) {
            if (fWait) {
                UninterruptibleSleep(std::chrono::milliseconds{1000});
            } else {
                throw;
            }
        }
    } while (fWait);
    return response;
}

static CAmount AmountFromValue(const UniValue& value)
{
    CAmount amount{0};
    if (!ParseFixedPoint(value.getValStr(), 8, &amount))
        throw std::runtime_error("Invalid amount");
    if (!MoneyRange(amount))
        throw std::runtime_error("Amount out of range");
    return amount;
}

static UniValue ValueFromAmount(const CAmount& amount)
{
    bool sign{amount < 0};
    int64_t n_abs{sign ? -amount : amount};
    int64_t quotient{n_abs / COIN};
    int64_t remainder{n_abs % COIN};
    return UniValue(UniValue::VNUM, strprintf("%s%d.%08d", sign ? "-" : "", quotient, remainder));
}

/**
 * GetWalletBalances calls listwallets; if more than one wallet is loaded, it then
 * fetches mine.trusted balances for each loaded wallet and pushes them to `result`,
 * preceded by the total balance.
 *
 * @param result  Reference to UniValue object the wallet names and balances are pushed to.
 */
static void GetWalletBalances(UniValue& result)
{
    std::unique_ptr<BaseRequestHandler> rh{MakeUnique<DefaultRequestHandler>()};
    const UniValue listwallets = ConnectAndCallRPC(rh.get(), "listwallets", /* args=*/{});
    if (!find_value(listwallets, "error").isNull()) return;
    const UniValue& wallets = find_value(listwallets, "result");
    if (wallets.size() <= 1) return;

    UniValue balances(UniValue::VOBJ);
    CAmount total_balance{0};
    for (const UniValue& wallet : wallets.getValues()) {
        const std::string wallet_name = wallet.get_str();
        const UniValue getbalances = ConnectAndCallRPC(rh.get(), "getbalances", /* args=*/{}, wallet_name);
        if (!find_value(getbalances, "error").isNull()) continue;
        const UniValue& balance = find_value(getbalances, "result")["mine"]["trusted"];
        total_balance += AmountFromValue(balance);
        balances.pushKV(wallet_name, balance);
    }
    result.pushKV("balances", balances);
    result.pushKV("total_balance", ValueFromAmount(total_balance));
}

static int CommandLineRPC(int argc, char *argv[])
{
    std::string strPrint;
    int nRet = 0;
    try {
        // Skip switches
        while (argc > 1 && IsSwitchChar(argv[1][0])) {
            argc--;
            argv++;
        }
        std::string rpcPass;
        if (gArgs.GetBoolArg("-stdinrpcpass", false)) {
            NO_STDIN_ECHO();
            if (!StdinReady()) {
                fputs("RPC password> ", stderr);
                fflush(stderr);
            }
            if (!std::getline(std::cin, rpcPass)) {
                throw std::runtime_error("-stdinrpcpass specified but failed to read from standard input");
            }
            if (StdinTerminal()) {
                fputc('\n', stdout);
            }
            gArgs.ForceSetArg("-rpcpassword", rpcPass);
        }
        std::vector<std::string> args = std::vector<std::string>(&argv[1], &argv[argc]);
        if (gArgs.GetBoolArg("-stdinwalletpassphrase", false)) {
            NO_STDIN_ECHO();
            std::string walletPass;
            if (args.size() < 1 || args[0].substr(0, 16) != "walletpassphrase") {
                throw std::runtime_error("-stdinwalletpassphrase is only applicable for walletpassphrase(change)");
            }
            if (!StdinReady()) {
                fputs("Wallet passphrase> ", stderr);
                fflush(stderr);
            }
            if (!std::getline(std::cin, walletPass)) {
                throw std::runtime_error("-stdinwalletpassphrase specified but failed to read from standard input");
            }
            if (StdinTerminal()) {
                fputc('\n', stdout);
            }
            args.insert(args.begin() + 1, walletPass);
        }
        if (gArgs.GetBoolArg("-stdin", false)) {
            // Read one arg per line from stdin and append
            std::string line;
            while (std::getline(std::cin, line)) {
                args.push_back(line);
            }
            if (StdinTerminal()) {
                fputc('\n', stdout);
            }
        }
        std::unique_ptr<BaseRequestHandler> rh;
        std::string method;
        if (gArgs.GetBoolArg("-getinfo", false)) {
            rh.reset(new GetinfoRequestHandler());
            method = "";
        } else if (gArgs.GetBoolArg("-netinfo", false)) {
            rh.reset(new NetinfoRequestHandler());
        } else {
            rh.reset(new DefaultRequestHandler());
            if (args.size() < 1) {
                throw std::runtime_error("too few parameters (need at least command)");
            }
            method = args[0];
            args.erase(args.begin()); // Remove trailing method name from arguments vector
        }
        Optional<std::string> wallet_name{};
        if (gArgs.IsArgSet("-rpcwallet")) wallet_name = gArgs.GetArg("-rpcwallet", "");
        const UniValue reply = ConnectAndCallRPC(rh.get(), method, args, wallet_name);

                // Parse reply
                UniValue result = find_value(reply, "result");
                const UniValue& error  = find_value(reply, "error");
                if (!error.isNull()) {
                    // Error
                    int code = error["code"].get_int();
                    strPrint = "error: " + error.write();
                    nRet = abs(code);
                    if (error.isObject())
                    {
                        UniValue errCode = find_value(error, "code");
                        UniValue errMsg  = find_value(error, "message");
                        strPrint = errCode.isNull() ? "" : "error code: "+errCode.getValStr()+"\n";

                        if (errMsg.isStr())
                            strPrint += "error message:\n"+errMsg.get_str();

                        if (errCode.isNum() && errCode.get_int() == RPC_WALLET_NOT_SPECIFIED) {
                            strPrint += "\nTry adding \"-rpcwallet=<filename>\" option to bitcoin-cli command line.";
                        }
                    }
                } else {
                    if (gArgs.GetBoolArg("-getinfo", false) && !gArgs.IsArgSet("-rpcwallet")) {
                        GetWalletBalances(result); // fetch multiwallet balances and append to result
                    }
                    // Result
                    if (result.isNull())
                        strPrint = "";
                    else if (result.isStr())
                        strPrint = result.get_str();
                    else
                        strPrint = result.write(2);
                }
    }
    catch (const std::exception& e) {
        strPrint = std::string("error: ") + e.what();
        nRet = EXIT_FAILURE;
    }
    catch (...) {
        PrintExceptionContinue(nullptr, "CommandLineRPC()");
        throw;
    }

    if (strPrint != "") {
        tfm::format(nRet == 0 ? std::cout : std::cerr, "%s\n", strPrint);
    }
    return nRet;
}

#ifdef WIN32
// Export main() and ensure working ASLR on Windows.
// Exporting a symbol will prevent the linker from stripping
// the .reloc section from the binary, which is a requirement
// for ASLR. This is a temporary workaround until a fixed
// version of binutils is used for releases.
__declspec(dllexport) int main(int argc, char* argv[])
{
    util::WinCmdLineArgs winArgs;
    std::tie(argc, argv) = winArgs.get();
#else
int main(int argc, char* argv[])
{
#endif
    SetupEnvironment();
    if (!SetupNetworking()) {
        tfm::format(std::cerr, "Error: Initializing networking failed\n");
        return EXIT_FAILURE;
    }
    event_set_log_callback(&libevent_log_cb);

    try {
        int ret = AppInitRPC(argc, argv);
        if (ret != CONTINUE_EXECUTION)
            return ret;
    }
    catch (const std::exception& e) {
        PrintExceptionContinue(&e, "AppInitRPC()");
        return EXIT_FAILURE;
    } catch (...) {
        PrintExceptionContinue(nullptr, "AppInitRPC()");
        return EXIT_FAILURE;
    }

    int ret = EXIT_FAILURE;
    try {
        ret = CommandLineRPC(argc, argv);
    }
    catch (const std::exception& e) {
        PrintExceptionContinue(&e, "CommandLineRPC()");
    } catch (...) {
        PrintExceptionContinue(nullptr, "CommandLineRPC()");
    }
    return ret;
}
