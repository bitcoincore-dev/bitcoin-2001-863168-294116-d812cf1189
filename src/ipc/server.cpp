#include "ipc/server.h"

#include "init.h"
#include "ui_interface.h"
#include "wallet/wallet.h"

#pragma GCC diagnostic ignored "-Wshadow"
#include "ipc/messages.capnp.h"
#pragma GCC diagnostic pop

#include <capnp/rpc-twoparty.h>
#include <kj/async-io.h>

namespace ipc
{

namespace
{

class HandlerServer final : public messages::Handler::Server
{
public:
    HandlerServer(kj::AsyncIoContext& ioContext_, messages::InitMessageCallback::Client callback_)
        : ioContext(ioContext_),
          callback(std::move(callback_)),
          connection(uiInterface.InitMessage.connect(
              [this](const std::string& message) {
                  auto request = callback.callRequest();
                  request.setMessage(message);
                  auto promise = request.send();
                  promise.wait(ioContext.waitScope);
              })) {}

    kj::Promise<void> disconnect(DisconnectContext context) override
    {
        connection.disconnect();
        return kj::READY_NOW;
    }

    kj::AsyncIoContext& ioContext;

    messages::InitMessageCallback::Client callback;
    boost::signals2::scoped_connection connection;
};

class WalletServer final : public messages::Wallet::Server
{
public:
    WalletServer(CWallet& wallet_) : wallet(wallet_) {}

    kj::Promise<void> getBalance(GetBalanceContext context) override
    {
        context.getResults().setValue(wallet.GetBalance());
        return kj::READY_NOW;
    }

    CWallet& wallet;
};

class NodeServer final : public messages::Node::Server
{
public:
    NodeServer(kj::AsyncIoContext& ioContext_) : ioContext(ioContext_) {}
    kj::Promise<void> helpMessage(HelpMessageContext context) override
    {
        context.getResults().setValue(HelpMessage(HelpMessageMode(context.getParams().getMode())));
        return kj::READY_NOW;
    }

    kj::Promise<void> handleInitMessage(HandleInitMessageContext context) override
    {
        context.getResults().setHandler(ipc::messages::Handler::Client(kj::heap<HandlerServer>(ioContext, context.getParams().getCallback())));
        return kj::READY_NOW;
    }

    kj::Promise<void> wallet(WalletContext context) override
    {
        context.getResults().setWallet(ipc::messages::Wallet::Client(kj::heap<WalletServer>(*pwalletMain)));
        return kj::READY_NOW;
    }

    kj::AsyncIoContext& ioContext;
};

} // namespace

bool StartServer(int argc, char* argv[], int& exitStatus)
{
    if (argc != 3 || strcmp(argv[1], "-ipcfd") != 0) {
        return false;
    }

    auto ioContext = kj::setupAsyncIo();
    auto serverStream = ioContext.lowLevelProvider->wrapSocketFd(atoi(argv[2]), kj::LowLevelAsyncIoProvider::TAKE_OWNERSHIP);
    auto serverNetwork = capnp::TwoPartyVatNetwork(*serverStream, capnp::rpc::twoparty::Side::SERVER, capnp::ReaderOptions());
    auto rpcServer = capnp::makeRpcServer(serverNetwork, capnp::Capability::Client(kj::heap<NodeServer>(ioContext)));
    kj::NEVER_DONE.wait(ioContext.waitScope);
    exitStatus = EXIT_SUCCESS;
    return true;
}

} // namespace ipc
