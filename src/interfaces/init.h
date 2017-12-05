#ifndef BITCOIN_INTERFACES_INIT_H
#define BITCOIN_INTERFACES_INIT_H

#include <interfaces/base.h>
#include <interfaces/chain.h>
#include <interfaces/init.h>

#include <memory>

namespace interfaces {

class Node;

//! Interface allowing multiprocess code to create other interfaces on startup.
class Init : public Base
{
public:
    virtual std::unique_ptr<Node> makeNode() = 0;
    virtual std::unique_ptr<ChainClient> makeWalletClient(Chain& chain, std::vector<std::string> wallet_filenames) = 0;
};

//! Return implementation of Init interface.
//! @param exe_path should be current executable path (argv[0])
std::unique_ptr<Init> MakeInit(std::string exe_path);

//! Start IPC server if requested on command line.
bool StartServer(int argc, char* argv[], int& exit_status, Init& init);

//! Handle incoming requests from socket by forwarding to provided Init
//! interface. This blocks until the socket is closed.
using ListenFn = void(const char* exe_name, int fd, Init& init);

//! Return Init interface that forwards requests over given socket descriptor.
using ConnectFn = std::unique_ptr<Init>(const char* exe_name, int fd);

} // namespace interfaces

#endif // BITCOIN_INTERFACES_INIT_H
