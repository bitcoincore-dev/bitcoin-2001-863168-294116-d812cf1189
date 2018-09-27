#ifndef BITCOIN_INTERFACES_CONFIG_H
#define BITCOIN_INTERFACES_CONFIG_H

#include <interfaces/chain.h>
#include <interfaces/ipc.h>
#include <interfaces/node.h>

namespace interfaces {

// Build options for current executable.
struct Config
{
    const char* exe_name;
    const char* log_suffix;
    MakeIpcProcessFn* make_ipc_process;
    MakeIpcProtocolFn* make_ipc_protocol;
    MakeNodeFn* make_node;
    MakeWalletClientFn* make_wallet_client;
};

extern const Config g_config;

} // namespace interfaces

#endif // BITCOIN_INTERFACES_CONFIG_H
