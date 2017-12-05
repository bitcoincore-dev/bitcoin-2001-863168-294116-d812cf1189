#include <interfaces/init.h>

#include <fs.h>
#include <interfaces/config.h>

#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

namespace interfaces {

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

class IpcProcessImpl : public IpcProcess
{
public:
    IpcProcessImpl(int argc, char* argv[], const Config& config, Init& init)
        : m_argc(argc), m_argv(argv), m_config(config), m_init(init)
    {
    }

    bool serve(int& exit_status) override;
    int spawn(const std::string& new_exe_name, int& pid) override;
    int wait(int pid) override;

    int m_argc;
    char** m_argv;
    const Config& m_config;
    Init& m_init;
};

int IpcProcessImpl::spawn(const std::string& new_exe_name, int& pid)
{
    int fds[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0) {
        throw std::system_error(errno, std::system_category());
    }

    pid = fork();
    if (close(fds[pid ? 0 : 1]) != 0) {
        throw std::system_error(errno, std::system_category());
    }
    if (!pid) {
        int maxFd = MaxFd();
        for (int fd = 3; fd < maxFd; ++fd) {
            if (fd != fds[0]) {
                close(fd);
            }
        }
        fs::path path = m_argc > 0 ? m_argv[0] : "";
        path.remove_filename();
        path.append(new_exe_name);
        if (execlp(path.c_str(), path.c_str(), "-ipcfd", std::to_string(fds[0]).c_str(), nullptr) != 0) {
            perror("execlp failed");
            _exit(1);
        }
    }
    return fds[1];
}

int IpcProcessImpl::wait(int pid)
{
    int status;
    if (::waitpid(pid, &status, 0 /* options */) != pid) {
        throw std::system_error(errno, std::system_category());
    }
    return status;
}

bool IpcProcessImpl::serve(int& exit_status)
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

} // namespace

std::unique_ptr<IpcProcess> MakeIpcProcess(int argc, char* argv[], const Config& config, Init& init)
{
    return MakeUnique<IpcProcessImpl>(argc, argv, config, init);
}

} // namespace interfaces
