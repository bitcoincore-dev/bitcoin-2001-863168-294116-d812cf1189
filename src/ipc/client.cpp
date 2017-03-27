#include "ipc/client.h"

#include "bitcoin-config.h"

#pragma GCC diagnostic ignored "-Wshadow"
#include "ipc/messages.capnp.h"
#pragma GCC diagnostic pop

#include <capnp/rpc-twoparty.h>
#include <kj/debug.h>

#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace ipc
{

namespace
{

//! VatId for server side of IPC connection.
struct ServerVatId {
    capnp::word scratch[4]{};
    capnp::MallocMessageBuilder message{scratch};
    capnp::rpc::twoparty::VatId::Builder vatId{message.getRoot<capnp::rpc::twoparty::VatId>()};
    ServerVatId() { vatId.setSide(capnp::rpc::twoparty::Side::SERVER); }
};

//! Return highest possible file descriptor.
size_t MaxFd()
{
    struct rlimit nofile;
    if (getrlimit(RLIMIT_NOFILE, &nofile) == 0) {
        return nofile.rlim_cur - 1;
    } else {
        return 1023;
    }
}

//! Forwarder for handleInitMessage callback.
class InitMessageCallbackServer final : public messages::InitMessageCallback::Server
{
public:
    InitMessageCallbackServer(std::function<void(const std::string&)> callback_) : callback(std::move(callback_)) {}

    kj::Promise<void> call(CallContext context) override
    {
        callback(context.getParams().getMessage());
        return kj::READY_NOW;
    }

    std::function<void(const std::string&)> callback;
};

} // namespace

//! Friend factory class able to call private constructors of IPC objects.
class Factory
{
public:
    template <typename T, typename... Args>
    static std::unique_ptr<T> MakeUnique(Args&&... args)
    {
        return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
    }

    template <typename T, typename... Args>
    static std::unique_ptr<T> MakeImpl(Args&&... args)
    {
        return MakeUnique<T>(MakeUnique<typename T::Impl>(std::forward<Args>(args)...));
    }
};

//! Handler private member struct.
struct Handler::Impl {
    kj::AsyncIoContext& ioContext;
    messages::Handler::Client handlerClient;
    Impl(kj::AsyncIoContext& ioContext_, messages::Handler::Client handlerClient_) : ioContext(ioContext_), handlerClient(std::move(handlerClient_)) {}
};

Handler::Handler(std::unique_ptr<Impl> impl_) : impl(std::move(impl_)) {}

Handler::~Handler() {}

void Handler::disconnect() const
{
    auto request = impl->handlerClient.disconnectRequest();
    auto promise = request.send();
    promise.wait(impl->ioContext.waitScope);
}

//! Wallet private member struct.
struct Wallet::Impl {
    kj::AsyncIoContext& ioContext;
    messages::Wallet::Client walletClient;
    Impl(kj::AsyncIoContext& ioContext_, messages::Wallet::Client walletClient_) : ioContext(ioContext_), walletClient(std::move(walletClient_)) {}
};

Wallet::Wallet(std::unique_ptr<Impl> impl_) : impl(std::move(impl_)) {}

Wallet::~Wallet() {}

CAmount Wallet::getBalance() const
{
    auto request = impl->walletClient.getBalanceRequest();
    auto promise = request.send();
    auto response = promise.wait(impl->ioContext.waitScope);
    return response.getValue();
}

//! Node private member struct.
struct Node::Impl {
    kj::AsyncIoContext ioContext{kj::setupAsyncIo()};
    kj::Own<kj::AsyncIoStream> clientStream;
    capnp::TwoPartyVatNetwork clientNetwork{*clientStream, capnp::rpc::twoparty::Side::CLIENT, capnp::ReaderOptions()};
    capnp::RpcSystem<capnp::rpc::twoparty::VatId> rpcClient{capnp::makeRpcClient(clientNetwork)};
    messages::Node::Client nodeClient{rpcClient.bootstrap(ServerVatId().vatId).castAs<messages::Node>()};

    Impl(int fd) : clientStream(ioContext.lowLevelProvider->wrapSocketFd(fd, kj::LowLevelAsyncIoProvider::TAKE_OWNERSHIP)) {}
};

Node::Node(std::unique_ptr<Impl> impl_) : impl(std::move(impl_)) {}

Node::~Node() {}

std::string Node::helpMessage(HelpMessageMode mode) const
{
    auto request = impl->nodeClient.helpMessageRequest();
    request.setMode(mode);
    auto promise = request.send();
    auto response = promise.wait(impl->ioContext.waitScope);
    return response.getValue();
}

std::unique_ptr<Handler> Node::handleInitMessage(std::function<void(const std::string&)> callback) const
{
    auto request = impl->nodeClient.handleInitMessageRequest();
    request.setCallback(kj::heap<InitMessageCallbackServer>(std::move(callback)));
    auto promise = request.send();
    auto response = promise.wait(impl->ioContext.waitScope);
    return Factory::MakeImpl<Handler>(impl->ioContext, response.getHandler());
}

std::unique_ptr<Wallet> Node::wallet() const
{
    auto request = impl->nodeClient.walletRequest();
    auto promise = request.send();
    auto response = promise.wait(impl->ioContext.waitScope);
    return Factory::MakeImpl<Wallet>(impl->ioContext, response.getWallet());
}

std::unique_ptr<Node> StartClient()
{
    int fds[2];
    KJ_SYSCALL(socketpair(AF_UNIX, SOCK_STREAM, 0, fds));

    if (fork() == 0) {
        int maxFd = MaxFd();
        for (int fd = 3; fd < maxFd; ++fd) {
            if (fd != fds[0]) {
                close(fd);
            }
        }
        if (execlp(BITCOIN_DAEMON_NAME, BITCOIN_DAEMON_NAME, "-ipcfd", std::to_string(fds[0]).c_str(), nullptr) != 0) {
            perror("execlp '" BITCOIN_DAEMON_NAME "' failed");
            _exit(1);
        }
    }

    return Factory::MakeImpl<Node>(fds[1]);
}

} // namespace ipc
