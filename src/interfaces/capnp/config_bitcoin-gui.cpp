#include <interfaces/capnp/config.h>

#include <interfaces/capnp/node.capnp.proxy.h>
#include <interfaces/capnp/node.h>
#include <interfaces/capnp/proxy-inl.h>
#include <interfaces/capnp/proxy.h>

namespace interfaces {
namespace capnp {

const Config g_config = {
    &MakeProxyClient<messages::Node, Node>,
    nullptr /* make_node_server */,
};

} // namespace capnp
} // namespace interfaces
