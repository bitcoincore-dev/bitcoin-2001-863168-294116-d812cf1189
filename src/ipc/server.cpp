#include <ipc/server.h>

#include <ipc/capnp/server.h>
#include <ipc/interfaces.h>

namespace ipc {

bool StartServer(int argc, char* argv[], int& exit_status)
{
    if (argc != 3 || strcmp(argv[1], "-ipcfd") != 0) {
        return false;
    }

    try {
        auto ipc_node = MakeNode(LOCAL);
        capnp::StartNodeServer(std::move(ipc_node), atoi(argv[2]));
        exit_status = EXIT_SUCCESS;
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        exit_status = EXIT_FAILURE;
    }

    return true;
}

} // namespace ipc
