#include <interfaces/init.h>

#include <fs.h>
#include <interfaces/config.h>

namespace interfaces {
namespace {
class InitImpl : public Init
{
public:
    InitImpl(int argc, char* argv[], const Config& config) : m_argc(argc), m_argv(argv), m_config(config) {}

    std::unique_ptr<Init> spawnOrConnect(const std::string& dest_exe_name, bool& spawned)
    {
        spawned = false;
        std::unique_ptr<Init> process;
        if (m_ipc_process && m_ipc_protocol) {
            int fd = -1;
            if (gArgs.IsArgSet("-ipcconnect") && !gArgs.IsArgNegated("-ipcconnect")) {
                std::string address = gArgs.GetArg("-ipcconnect", "");
                if (address.empty()) address = "unix";
                LogPrint(::BCLog::IPC, "Trying to connect to running process '%s' at address '%s'\n", dest_exe_name,
                    address);
                fd = m_ipc_process->connect(address, GetDataDir(), dest_exe_name);
            }
            if (fd < 0) {
                LogPrint(::BCLog::IPC, "Spawning subprocess process '%s'\n", dest_exe_name);
                fd = m_ipc_process->spawn(dest_exe_name);
                spawned = true;
            }
            if (fd < 0) {
                LogPrint(::BCLog::IPC, "Failed to spawn or connect to running process '%s'\n", dest_exe_name);
            } else {
                process = m_ipc_protocol->connect(fd);
            }
        }
        return process;
    }

    std::unique_ptr<Node> makeNode(bool& new_node_process) override
    {
        new_node_process = false;
        if (m_config.make_node) {
            return m_config.make_node(*this);
        } else if (m_ipc_process && m_ipc_protocol) {
            auto process = spawnOrConnect("bitcoin-node", new_node_process);
            bool ignore;
            auto node = process->makeNode(ignore);
            // Subprocess no longer needed after node client is discarded.
            node->addCloseHook(MakeUnique<Deleter<std::unique_ptr<Init>>>(std::move(process)));
            return node;
        }
        throw std::logic_error("Build configuration error. Init::makeNode called unexpectedly from executable not "
                               "built as a bitcoin GUI or other Node interface client.");
    }

    std::unique_ptr<ChainClient> makeWalletClient(Chain& chain, std::vector<std::string> wallet_filenames) override
    {
        if (m_config.make_wallet_client) {
            return m_config.make_wallet_client(chain, std::move(wallet_filenames));
        }
        if (m_ipc_process && m_ipc_protocol) {
            int fd = m_ipc_process->spawn("bitcoin-wallet");
            auto init = m_ipc_protocol->connect(fd);
            auto wallet = init->makeWalletClient(chain, std::move(wallet_filenames));
            // Subprocess no longer needed after wallet client is discarded.
            wallet->addCloseHook(MakeUnique<Deleter<std::unique_ptr<Init>>>(std::move(init)));
            return wallet;
        }
        throw std::logic_error("Build configuration error. Init::makeWalletClient called unexpectedly from executable "
                               "not built as a Chain interface server.");
    }

    IpcProcess* getProcess() override { return m_ipc_process.get(); }

    IpcProtocol* getProtocol() override { return m_ipc_protocol.get(); }

    int m_argc;
    char** m_argv;
    const Config& m_config;
    std::unique_ptr<IpcProcess> m_ipc_process =
        m_config.make_ipc_process ? m_config.make_ipc_process(m_argc, m_argv, m_config, *this) : nullptr;
    std::unique_ptr<IpcProtocol> m_ipc_protocol =
        m_config.make_ipc_protocol ? m_config.make_ipc_protocol(m_config.exe_name, *this) : nullptr;
};
} // namespace

std::unique_ptr<Init> MakeInit(int argc, char* argv[], const Config& config)
{
    return MakeUnique<InitImpl>(argc, argv, config);
}

} // namespace interfaces
