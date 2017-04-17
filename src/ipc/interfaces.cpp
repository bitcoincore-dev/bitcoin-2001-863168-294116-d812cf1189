#include "ipc/interfaces.h"

#include "ipc/local/interfaces.h"

namespace ipc {

std::unique_ptr<Node> MakeNode(Protocol protocol)
{
    if (protocol == LOCAL) {
        return local::MakeNode();
    }
    return {};
}

} // namespace ipc
