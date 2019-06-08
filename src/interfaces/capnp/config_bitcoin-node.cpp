
#include <interfaces/capnp/node.capnp.proxy.h>
#include <interfaces/capnp/proxy-types.h>

namespace interfaces {
namespace capnp {

const Config g_config = {
    nullptr /* make_node_client */,
    &MakeProxyServer<messages::Node, Node>,
};

} // namespace capnp
} // namespace interfaces
