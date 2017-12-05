#ifndef BITCOIN_INTERFACE_INIT_H
#define BITCOIN_INTERFACE_INIT_H

#include <interface/base.h>
#include <interface/chain.h>
#include <interface/init.h>

#include <memory>

namespace interface {

class Node;

//! Interface allowing multiprocess code to create other interfaces on startup.
class Init : public Base
{
public:
    virtual std::unique_ptr<Node> makeNode() = 0;
    virtual std::unique_ptr<Chain::Client> makeWalletClient(Chain& chain,
        std::vector<std::string> wallet_filenames) = 0;
};

//! Return implementation of Init interface.
//! @param exe_path should be current executable path (argv[0])
std::unique_ptr<Init> MakeInit(std::string exe_path);

//! Start IPC server if requested on command line.
bool StartServer(int argc, char* argv[], int& exit_status);

//! Handle incoming requests from socket by forwarding to provided Init
//! interface. This blocks until the socket is closed.
using ListenFn = void(int fd, Init& init);

//! Return Init interface that forwards requests over given socket descriptor.
using ConnectFn = std::unique_ptr<Init>(int fd);

} // namespace interface

#endif // BITCOIN_INTERFACE_INIT_H
