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
    virtual IpcProcess* getProcess() { return nullptr; }
    virtual IpcProtocol* getProtocol() { return nullptr; }
};

//! Return implementation of Init interface.
std::unique_ptr<Init> MakeInit(int argc, char* argv[], const Config& config);

//! Proxy client / server create hooks.
//!
//! These hooks exist to allow a bitcoin-node, bitcoin-wallet, bitcoin-gui
//! executables all to share a single implementation of Init proxy client and
//! proxy server classes without those classes linking against symbols from Node
//! client and server classes and requiring bitcoin-wallet to be linked against
//! libbitcoin_server.
//!
//! These use named types to avoid the need to use casts and sacrifice type
//! checking, but the types are opaque so they are not tied to a particular
//! protocol or proxy object implementation.
template <typename Param>
void MakeProxy(Param&);
struct NodeClientParam;
struct NodeServerParam;
using MakeNodeClientFn = void(NodeClientParam&);
using MakeNodeServerFn = void(NodeServerParam&);

} // namespace interfaces

#endif // BITCOIN_INTERFACES_INIT_H
