
#include <interfaces/capnp/node.capnp.proxy.h>
#include <interfaces/capnp/proxy-types.h>

namespace interfaces {
namespace capnp {

const Config g_config = {
    &MakeProxyClient<messages::Node, Node>,
    nullptr /* make_node_server */,
};

} // namespace capnp
} // namespace interfaces
