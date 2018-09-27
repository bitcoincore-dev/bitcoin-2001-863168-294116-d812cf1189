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
    virtual std::unique_ptr<Node> makeNode() = 0;
    virtual std::unique_ptr<ChainClient> makeWalletClient(Chain& chain, std::vector<std::string> wallet_filenames) = 0;
    virtual IpcProcess* getProcess() { return nullptr; };
    virtual IpcProtocol* getProtocol() { return nullptr; };
};

//! Return implementation of Init interface.
//! @param exe_path should be current executable path (argv[0])
std::unique_ptr<Init> MakeInit(int argc, char* argv[], const Config& config);

} // namespace interfaces

#endif // BITCOIN_INTERFACES_INIT_H
