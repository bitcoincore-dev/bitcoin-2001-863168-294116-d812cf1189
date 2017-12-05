#include <interface/init.h>

#include <fs.h>
#include <interface/config.h>
#include <interface/init.h>

#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace interface {

extern const Config g_config;

namespace {

//! Return highest possible file descriptor.
size_t MaxFd()
{
    struct rlimit nofile;
    if (getrlimit(RLIMIT_NOFILE, &nofile) == 0) {
        return nofile.rlim_cur - 1;
    } else {
        return 1023;
    }
}

//! Spawn process and return file descriptor for communicating with it.
int SpawnProcess(const std::string& cur_exe_path, const std::string& new_exe_name)
{
    int fds[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0) {
        throw std::system_error(errno, std::system_category());
    }

    if (fork() == 0) {
        int maxFd = MaxFd();
        for (int fd = 3; fd < maxFd; ++fd) {
            if (fd != fds[0]) {
                close(fd);
            }
        }
        fs::path path = cur_exe_path;
        path.remove_filename();
        path.append(new_exe_name);
        if (execlp(path.c_str(), path.c_str(), "-ipcfd", std::to_string(fds[0]).c_str(), nullptr) != 0) {
            perror("execlp failed");
            _exit(1);
        }
    }
    return fds[1];
}

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
        if (g_config.make_node) return g_config.make_node();
        int fd = SpawnProcess(m_exe_path, "bitcoin-node");
        auto init = g_config.connect(fd);
        auto node = init->makeNode();
        node->addCloseHook(MakeUnique<CloseInit>(std::move(init)));
        return node;
    }

    std::unique_ptr<Chain::Client> makeWalletClient(Chain& chain, std::vector<std::string> wallet_filenames) override
    {
        if (g_config.make_wallet_client) return g_config.make_wallet_client(chain, std::move(wallet_filenames));
        int fd = SpawnProcess(m_exe_path, "bitcoin-wallet");
        auto init = g_config.connect(fd);
        auto wallet = init->makeWalletClient(chain, std::move(wallet_filenames));
        wallet->addCloseHook(MakeUnique<CloseInit>(std::move(init)));
        return wallet;
    }

    std::string m_exe_path;
};

} // namespace

//! Return implementation of Chain interface.
std::unique_ptr<Init> MakeInit(std::string exe_path) { return MakeUnique<InitImpl>(std::move(exe_path)); }

bool StartServer(int argc, char* argv[], int& exit_status)
{
    if (argc != 3 || strcmp(argv[1], "-ipcfd") != 0) {
        return false;
    }

    if (!g_config.listen) {
        throw std::runtime_error(strprintf("Usage error: -ipcfd option not supported by %s", g_config.exe_name));
    }

    try {
        int fd = atoi(argv[2]);
        auto init = MakeInit(argc > 0 ? argv[0] : "");
        g_config.listen(fd, *init);
        exit_status = EXIT_SUCCESS;
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        exit_status = EXIT_FAILURE;
    }

    return true;
}

} // namespace interface
