#ifndef BITCOIN_IPC_CLIENT_H
#define BITCOIN_IPC_CLIENT_H

#include "amount.h"
#include "init.h"

#include <memory>

namespace ipc
{

class Node;
class Wallet;
class Handler;

//! Start IPC client and return top level Node object.
std::unique_ptr<Node> StartClient();

//! Top level IPC object for communicating with bitcoin node.
class Node
{
public:
    //! Get help message string.
    std::string helpMessage(HelpMessageMode mode) const;

    //! Register handler for node init messages.
    std::unique_ptr<Handler> handleInitMessage(std::function<void(const std::string&)>) const;

    //! Return IPC object for accessing the wallet.
    std::unique_ptr<Wallet> wallet() const;

    ~Node();

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
    Node(std::unique_ptr<Impl>);
    friend class Factory;
};

// IPC object for calling wallet methods.
class Wallet
{
public:
    //! Return wallet balance.
    CAmount getBalance() const;

    ~Wallet();

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
    Wallet(std::unique_ptr<Impl>);
    friend class Factory;
};

// IPC object for managing a registered handler.
class Handler
{
public:
    //! Disconnect the handler.
    void disconnect() const;

    ~Handler();

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
    Handler(std::unique_ptr<Impl>);
    friend class Factory;
};

} // namespace ipc

#define ENABLE_IPC 1
#if ENABLE_IPC
#define FIXME_IMPLEMENT_IPC(x)
#define FIXME_IMPLEMENT_IPC_VALUE(x) (*(decltype(x)*)(0))
#else
#define FIXME_IMPLEMENT_IPC(x) x
#define FIXME_IMPLEMENT_IPC_VALUE(x) x
#endif

#endif // BITCOIN_IPC_CLIENT_H
