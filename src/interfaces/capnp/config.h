#ifndef BITCOIN_INTERFACES_CAPNP_CONFIG_H
#define BITCOIN_INTERFACES_CAPNP_CONFIG_H

#include <interfaces/capnp/node.capnp.h>
#include <memory>

namespace interfaces {
class Node;
namespace capnp {
struct InvokeContext;

using MakeNodeClientFn = std::unique_ptr<Node>(InvokeContext& context, messages::Node::Client&& client);
using MakeNodeServerFn = kj::Own<messages::Node::Server>(InvokeContext& context, std::unique_ptr<Node>&& impl);

// Build options for current executable.
struct Config
{
    MakeNodeClientFn* make_node_client;
    MakeNodeServerFn* make_node_server;
};

extern const Config g_config;

} // namespace capnp
} // namespace interfaces

#endif // BITCOIN_INTERFACES_CAPNP_CONFIG_H
