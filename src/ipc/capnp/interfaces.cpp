#include <ipc/capnp/interfaces.h>

#include <bitcoin-config.h>
#include <chainparams.h>
#include <coins.h>
#include <ipc/capnp/messages.capnp.h>
#include <ipc/capnp/types.h>
#include <ipc/capnp/util.h>
#include <ipc/interfaces.h>
#include <ipc/util.h>
#include <net_processing.h>
#include <netbase.h>
#include <pubkey.h>
#include <wallet/wallet.h>

#include <capnp/rpc-twoparty.h>
#include <ipc/capnp/messages.capnp.proxy.h>
#include <ipc/capnp/types.h>
#include <kj/async-unix.h>
#include <kj/debug.h>
#include <univalue.h>

#include <future>
#include <mutex>
#include <thread>

namespace ipc {
namespace capnp {
namespace {

struct Connection
{
    Connection(EventLoop& loop, int fd)
        : m_stream(loop.m_io_context.lowLevelProvider->wrapSocketFd(fd, kj::LowLevelAsyncIoProvider::TAKE_OWNERSHIP)),
          m_network(*m_stream, ::capnp::rpc::twoparty::Side::CLIENT, ::capnp::ReaderOptions()),
          m_rpc_client(::capnp::makeRpcClient(m_network))
    {
    }

    kj::Own<kj::AsyncIoStream> m_stream;
    ::capnp::TwoPartyVatNetwork m_network;
    ::capnp::RpcSystem<::capnp::rpc::twoparty::VatId> m_rpc_client;
};

//! VatId for server side of IPC connection.
struct ServerVatId
{
    ::capnp::word scratch[4]{};
    ::capnp::MallocMessageBuilder message{scratch};
    ::capnp::rpc::twoparty::VatId::Builder vat_id{message.getRoot<::capnp::rpc::twoparty::VatId>()};
    ServerVatId() { vat_id.setSide(::capnp::rpc::twoparty::Side::SERVER); }
};

using PC = Proxy<messages::Node>::Client;


// FIXME move to types
class NodeClient : public PC
{
public:
    NodeClient(EventLoop& loop, messages::Node::Client client, kj::Own<Connection> connection)
        : PC(loop, std::move(client)), m_connection(kj::mv(connection))
    {
    }

    void detach() override
    {
        PC::detach();
        m_connection = nullptr;
    }

    ~NodeClient() noexcept override
    {
        assert(m_loop);
        shutdown();
        m_loop->shutdown();
        m_loop = nullptr;
    }

    void parseParameters(int argc, const char* const argv[]) override
    {
        gArgs.ParseParameters(argc, argv);
        return PC::parseParameters(argc, argv);
    }

    bool softSetArg(const std::string& arg, const std::string& value) override
    {
        gArgs.SoftSetArg(arg, value);
        return PC::softSetArg(arg, value);
    }

    bool softSetBoolArg(const std::string& arg, bool value) override
    {
        gArgs.SoftSetBoolArg(arg, value);
        return PC::softSetBoolArg(arg, value);
    }

    void readConfigFile(const std::string& conf_path) override
    {
        gArgs.ReadConfigFile(conf_path);
        return PC::readConfigFile(conf_path);
    }

    void selectParams(const std::string& network) override
    {
        ::SelectParams(network);
        return PC::selectParams(network);
    }

    kj::Own<Connection> m_connection;
};

} // namespace

std::unique_ptr<Node> MakeNode(int fd)
{
    std::promise<std::unique_ptr<Node>> node_promise;
    std::thread thread([&]() {
        EventLoop loop(std::move(thread));
        auto connection = kj::heap<Connection>(loop, fd);
        node_promise.set_value(MakeUnique<NodeClient>(loop,
            connection->m_rpc_client.bootstrap(ServerVatId().vat_id).castAs<messages::Node>(), kj::mv(connection)));
        loop.loop();
    });
    return node_promise.get_future().get();
}

} // namespace capnp
} // namespace ipc
