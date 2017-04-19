#include "ipc/server.h"

#include "ipc/capnp/server.h"
#include "ipc/interfaces.h"

namespace ipc {

bool StartServer(int argc, char* argv[], int& exitStatus)
{
    if (argc != 3 || strcmp(argv[1], "-ipcfd") != 0) {
        return false;
    }

    try {
        auto ipcNode = MakeNode(LOCAL);
        capnp::StartNodeServer(*ipcNode, atoi(argv[2]));
        exitStatus = EXIT_SUCCESS;
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        exitStatus = EXIT_FAILURE;
    }

    return true;
}

} // namespace ipc
