#include <init.h>
#include <interfaces/capnp/init-types.h>
#include <rpc/util.h>

namespace mp {

std::unique_ptr<interfaces::ChainClient>
ProxyServerMethodTraits<interfaces::capnp::messages::Init::MakeWalletClientParams>::invoke(Context& context,
    std::vector<std::string> wallet_filenames)
{
    // TODO in future PR: Refactor bitcoin startup code, dedup this with AppInitSanityChecks
    RandomInit();
    ECC_Start();
    static ECCVerifyHandle globalVerifyHandle;
    // TODO in future PR: Refactor bitcoin startup code, dedup this with InitSanityCheck
    if (!ECC_InitSanityCheck()) {
        throw std::runtime_error("Elliptic curve cryptography sanity check failure. Aborting.");
    }
    if (!Random_SanityCheck()) {
        throw std::runtime_error("OS cryptographic RNG sanity check failure. Aborting.");
    }
    // TODO in future PR: Refactor bitcoin startup code, dedup this with AppInit.
    SelectParams(interfaces::capnp::GlobalArgsNetwork());
    // TODO in future PR: Maybe add AppInitBasicSetup signal handler calls

    // FIXME pasted from InitLogging. This should be fixed by serializing
    // loginstance state and initializing members from parent process, not from
    // reparsing config again here.
    LogInstance().m_print_to_file = !gArgs.IsArgNegated("-debuglogfile");
    LogInstance().m_file_path = AbsPathForConfigVal(gArgs.GetArg("-debuglogfile", DEFAULT_DEBUGLOGFILE));
    if (interfaces::g_config.log_suffix && LogInstance().m_file_path.string() != "/dev/null") {
        LogInstance().m_file_path += interfaces::g_config.log_suffix;
    }
    LogInstance().m_print_to_console = gArgs.GetBoolArg("-printtoconsole", !gArgs.GetBoolArg("-daemon", false));
    LogInstance().m_log_timestamps = gArgs.GetBoolArg("-logtimestamps", DEFAULT_LOGTIMESTAMPS);
    LogInstance().m_log_time_micros = gArgs.GetBoolArg("-logtimemicros", DEFAULT_LOGTIMEMICROS);
    LogInstance().m_log_threadnames = gArgs.GetBoolArg("-logthreadnames", DEFAULT_LOGTHREADNAMES);

    // TODO in future PR: Refactor bitcoin startup code, dedup this with AppInitMain.
    if (!LogInstance().StartLogging()) {
        throw std::runtime_error(
            strprintf("Could not open wallet debug log file '%s'", LogInstance().m_file_path.string()));
    }

    auto params = context.call_context.getParams();
    auto interfaces = MakeUnique<InitInterfaces>();
    g_rpc_interfaces = interfaces.get();
    interfaces->chain = MakeUnique<ProxyClient<interfaces::capnp::messages::Chain>>(
        params.getChain(), context.proxy_server.m_connection, /* destroy_connection= */ false);
    auto client = context.proxy_server.m_impl->makeWalletClient(*interfaces->chain, std::move(wallet_filenames));
    client->addCloseHook(MakeUnique<interfaces::Deleter<std::unique_ptr<InitInterfaces>>>(std::move(interfaces)));
    return client;
}

} // namespace mp
