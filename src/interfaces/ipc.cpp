#include <interfaces/init.h>

#include <errno.h>
#include <fs.h>
#include <interfaces/config.h>

#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
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
    int spawn(const std::string& new_exe_name) override;
    int connect(const std::string& address, const fs::path& data_dir, const std::string& dest_exe_name) override;
    int bind(const std::string& address, const fs::path& data_dir) override;
    int connectUnix(const fs::path& socket_path, bool can_fail);

    int m_argc;
    char** m_argv;
    const Config& m_config;
    Init& m_init;
};

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
        protocol.loop();
        exit_status = EXIT_SUCCESS;
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        exit_status = EXIT_FAILURE;
    }

    return true;
}

int IpcProcessImpl::spawn(const std::string& new_exe_name)
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

bool ParseAddress(const std::string& address,
    const fs::path& data_dir,
    const std::string& dest_exe_name,
    struct sockaddr_un& addr)
{
    if (address.compare(0, 4, "unix") == 0 && (address.size() == 4 || address[4] == ':')) {
        fs::path socket_dir = data_dir / "sockets";
        fs::path path;
        if (address.size() <= 5) {
            path = socket_dir / strprintf("%s.sock", dest_exe_name);
        } else {
            path = fs::absolute(address.substr(5), socket_dir);
        }
        std::string path_str = path.string();
        if (path_str.size() < sizeof(addr.sun_path) - 1) {
            memset(&addr, 0, sizeof(addr));
            addr.sun_family = AF_UNIX;
            strncpy(addr.sun_path, path.string().c_str(), sizeof(addr.sun_path));
            return true;
        }
    }

    return false;
}

int IpcProcessImpl::connect(const std::string& address, const fs::path& data_dir, const std::string& dest_exe_name)
{
    struct sockaddr_un addr;
    if (!ParseAddress(address, data_dir, dest_exe_name, addr)) {
        LogPrintf("Error: Invalid IPC connect address '%s'\n", address);
        return -1;
    }

    int fd;
    if ((fd = ::socket(addr.sun_family, SOCK_STREAM, 0)) == -1) {
        throw std::system_error(errno, std::system_category());
    }
    if (::connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
        return fd;
    }
    int connect_error = errno;
    if (::close(fd) != 0) {
        LogPrintf("Error closing file descriptor %i: %s\n", fd, strerror(errno));
    }
    throw std::system_error(connect_error, std::system_category());
}

int IpcProcessImpl::bind(const std::string& address, const fs::path& data_dir)
{
    struct sockaddr_un addr;
    if (!ParseAddress(address, data_dir, m_config.exe_name, addr)) {
        LogPrintf("Error: Invalid IPC bind address '%s'\n", address);
        return -1;
    }

    if (addr.sun_family == AF_UNIX) {
        fs::path path = addr.sun_path;
        fs::create_directories(path.parent_path());
        if (fs::symlink_status(path).type() == fs::socket_file) {
            fs::remove(path);
        }
    }

    int fd;
    if ((fd = ::socket(addr.sun_family, SOCK_STREAM, 0)) == -1) {
        throw std::system_error(errno, std::system_category());
    }

    if (::bind(fd, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
        return fd;
    }
    int bind_error = errno;
    if (::close(fd) != 0) {
        LogPrintf("Error closing file descriptor %i: %s\n", fd, strerror(errno));
    }
    throw std::system_error(bind_error, std::system_category());
}

} // namespace

std::unique_ptr<IpcProcess> MakeIpcProcess(int argc, char* argv[], const Config& config, Init& init)
{
    return MakeUnique<IpcProcessImpl>(argc, argv, config, init);
}

} // namespace interfaces
