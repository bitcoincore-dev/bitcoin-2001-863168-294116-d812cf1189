#include <interfaces/init.h>

#include <fs.h>
#include <interfaces/config.h>

#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace interfaces {

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

} // namespace

//! Spawn process and return file descriptor for communicating with it.
int ProcessSpawn(const std::string& cur_exe_path, const std::string& new_exe_name)
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

bool ProcessServe(int argc, char* argv[], int& exit_status, Init& init)
{
    if (argc != 3 || strcmp(argv[1], "-ipcfd") != 0) {
        return false;
    }

    if (!g_config.socket_listen) {
        throw std::runtime_error(strprintf("Usage error: -ipcfd option not supported by %s", g_config.exe_name));
    }

    try {
        int fd = -1;
        std::istringstream(argv[2]) >> fd;
        g_config.socket_listen(g_config.exe_name, fd, init);
        exit_status = EXIT_SUCCESS;
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        exit_status = EXIT_FAILURE;
    }

    return true;
}

} // namespace interfaces
