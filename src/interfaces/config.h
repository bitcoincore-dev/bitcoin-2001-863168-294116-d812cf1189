#ifndef BITCOIN_INTERFACES_CONFIG_H
#define BITCOIN_INTERFACES_CONFIG_H

#include <interfaces/init.h>
#include <interfaces/node.h>
#include <interfaces/wallet.h>

namespace interfaces {

// Build options for current executable.
struct Config
{
    const char* exe_name;
    ListenFn* listen;
    ConnectFn* connect;
    MakeNodeFn* make_node;
    MakeWalletClientFn* make_wallet_client;
};

extern const Config g_config;

} // namespace interfaces

#endif // BITCOIN_INTERFACES_CONFIG_H
