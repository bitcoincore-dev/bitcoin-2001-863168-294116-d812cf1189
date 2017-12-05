#include <interfaces/init.h>

#include <fs.h>
#include <interfaces/config.h>

#include <mp/util.h>

namespace interfaces {

namespace {

class IpcProcessImpl : public IpcProcess
{
public:
    IpcProcessImpl(int argc, char* argv[], const Config& config, Init& init)
        : m_argc(argc), m_argv(argv), m_config(config), m_init(init)
    {
    }
    bool serve(int& exit_status) override
    {
        if (m_argc != 3 || strcmp(m_argv[1], "-ipcfd") != 0) {
            return false;
        }
        try {
            int fd = -1;
            std::istringstream(m_argv[2]) >> fd;
            auto& protocol = *m_init.getProtocol();
            protocol.serve(fd);
            exit_status = EXIT_SUCCESS;
        } catch (std::exception& e) {
            std::cerr << e.what() << std::endl;
            exit_status = EXIT_FAILURE;
        }
        return true;
    }
    int spawn(const std::string& new_exe_name, int& pid) override
    {
        return mp::SpawnProcess(pid, [&](int fd) {
            fs::path path = m_argc > 0 ? m_argv[0] : "";
            path.remove_filename();
            path.append(new_exe_name);
            return std::vector<std::string>{path.string(), "-ipcfd", std::to_string(fd)};
        });
    }
    int wait(int pid) override { return mp::WaitProcess(pid); }

    int m_argc;
    char** m_argv;
    const Config& m_config;
    Init& m_init;
};
} // namespace

std::unique_ptr<IpcProcess> MakeIpcProcess(int argc, char* argv[], const Config& config, Init& init)
{
    return MakeUnique<IpcProcessImpl>(argc, argv, config, init);
}

} // namespace interfaces
