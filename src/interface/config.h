#ifndef BITCOIN_INTERFACE_CONFIG_H
#define BITCOIN_INTERFACE_CONFIG_H

#include <interface/init.h>
#include <interface/node.h>
#include <interface/wallet.h>

namespace interface {

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

} // namespace interface

#endif // BITCOIN_INTERFACE_CONFIG_H
