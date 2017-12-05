#include <interfaces/init.h>

#include <fs.h>
#include <interfaces/config.h>

namespace interfaces {

extern const Config g_config;

namespace {

class CloseInit : public CloseHook
{
public:
    CloseInit(std::unique_ptr<Init> init) : m_init(std::move(init)) {}
    void onClose(Base& interface, bool remote) override { m_init.reset(); }
    std::unique_ptr<Init> m_init;
};

class InitImpl : public Init
{
public:
    InitImpl(std::string exe_path) : m_exe_path(std::move(exe_path)) {}

    std::unique_ptr<Node> makeNode() override
    {
        if (g_config.make_node) {
            return g_config.make_node(*this);
        }
        if (g_config.process_spawn && g_config.socket_connect) {
            int fd = g_config.process_spawn(m_exe_path, "bitcoin-node");
            auto init = g_config.socket_connect(g_config.exe_name, fd);
            auto node = init->makeNode();
            // Subprocess no longer needed after node client is discarded.
            node->addCloseHook(MakeUnique<CloseInit>(std::move(init)));
            return node;
        }
        throw std::logic_error("Build configuration error. Init::makeNode called unexpectedly from executable not built as a bitcoin GUI or other Node interface client.");
    }

    std::unique_ptr<ChainClient> makeWalletClient(Chain& chain, std::vector<std::string> wallet_filenames) override
    {
        if (g_config.make_wallet_client) {
            return g_config.make_wallet_client(chain, std::move(wallet_filenames));
        }
        if (g_config.process_spawn && g_config.socket_connect) {
            int fd = g_config.process_spawn(m_exe_path, "bitcoin-wallet");
            auto init = g_config.socket_connect(g_config.exe_name, fd);
            auto wallet = init->makeWalletClient(chain, std::move(wallet_filenames));
            // Subprocess no longer needed after wallet client is discarded.
            wallet->addCloseHook(MakeUnique<CloseInit>(std::move(init)));
            return wallet;
        }
        throw std::logic_error("Build configuration error. Init::makeWalletClient called unexpectedly from executable not built as a Chain interface server.");
    }

    std::string m_exe_path;
};

} // namespace

//! Return implementation of Chain interface.
std::unique_ptr<Init> MakeInit(std::string exe_path) { return MakeUnique<InitImpl>(std::move(exe_path)); }

} // namespace interfaces
