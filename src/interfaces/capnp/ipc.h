#ifndef BITCOIN_INTERFACES_CAPNP_IPC_H
#define BITCOIN_INTERFACES_CAPNP_IPC_H

#include <interfaces/ipc.h>

#include <logging.h>
#include <memory>
#include <string>
#include <tinyformat.h>

namespace interfaces {
namespace capnp {

MakeIpcProtocolFn MakeCapnpProtocol;

#define LogIpc(loop, format, ...) \
    LogPrint(::BCLog::IPC, "{%s} " format, LongThreadName((loop).m_exe_name), ##__VA_ARGS__)

} // namespace capnp
} // namespace interfaces

#endif // BITCOIN_INTERFACES_CAPNP_IPC_H
