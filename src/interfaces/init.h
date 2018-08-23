#ifndef BITCOIN_INTERFACES_INIT_H
#define BITCOIN_INTERFACES_INIT_H

#include <interfaces/base.h>

#include <memory>
#include <string>
#include <vector>

namespace interfaces {

class Chain;
class ChainClient;
class IpcProcess;
class IpcProtocol;
class Node;
struct Config;

//! Interface allowing multiprocess code to create other interfaces on startup.
class Init : public Base
{
public:
    /**
     * Return new Node pointer and already_running flag indicating if Node
     * pointer came from connecting to an already running external process that
     * doesn't need to be reinitialized.
     * FIXME: Should remove hacky new_node_process output parameter. Instead
     * makeNode should accept whatever informational or callback inputs are
     * needed so Node itself initialize without requiring client to check a
     * variable and take further action.
     */
    virtual std::unique_ptr<Node> makeNode(bool& new_node_process) = 0;
    virtual std::unique_ptr<ChainClient> makeWalletClient(Chain& chain, std::vector<std::string> wallet_filenames) = 0;
    virtual IpcProcess* getProcess() { return nullptr; };
    virtual IpcProtocol* getProtocol() { return nullptr; };
};

//! Return implementation of Init interface.
//! @param exe_path should be current executable path (argv[0])
std::unique_ptr<Init> MakeInit(int argc, char* argv[], const Config& config);

} // namespace interfaces

#endif // BITCOIN_INTERFACES_INIT_H
