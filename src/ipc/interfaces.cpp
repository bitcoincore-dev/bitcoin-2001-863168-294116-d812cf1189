#include <ipc/interfaces.h>

#include <config/bitcoin-config.h>
#include <fs.h>
#include <ipc/capnp/interfaces.h>
#include <ipc/local/interfaces.h>

#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace ipc {
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

//! Spawn bitcoind process and return file descriptor for communicating with it.
int SpawnBitcoind(const ProtocolOptions& options)
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
        fs::path path = options.exe_path;
        path.remove_filename();
        path.append(BITCOIN_DAEMON_NAME);
        if (execlp(path.c_str(), path.c_str(), "-ipcfd", std::to_string(fds[0]).c_str(), nullptr) != 0) {
            perror("execlp '" BITCOIN_DAEMON_NAME "' failed");
            _exit(1);
        }
    }
    return fds[1];
}

} // namespace

std::unique_ptr<Node> MakeNode(const ProtocolOptions& options)
{
    if (options.protocol == LOCAL) {
        return local::MakeNode();
    } else if (options.protocol == CAPNP) {
        return capnp::MakeNode(SpawnBitcoind(options));
    }
    return {};
}

} // namespace ipc
