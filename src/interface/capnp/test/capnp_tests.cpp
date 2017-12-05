#include <interface/base.h>
#include <interface/capnp/proxy-impl.h>
#include <interface/capnp/test/foo.capnp.proxy.h>

#include <boost/test/unit_test.hpp>
#include <future>
#include <kj/common.h>
#include <kj/memory.h>

namespace interface {
namespace capnp {
namespace test {

//! VatId for server side of IPC connection.
// FIXME: Duplicated from capnp/init.cpp
struct ServerVatId
{
    ::capnp::word scratch[4]{};
    ::capnp::MallocMessageBuilder message{scratch};
    ::capnp::rpc::twoparty::VatId::Builder vat_id{message.getRoot<::capnp::rpc::twoparty::VatId>()};
    ServerVatId() { vat_id.setSide(::capnp::rpc::twoparty::Side::SERVER); }
};

// FIXME: Duplicated from capnp/init.cpp
class ShutdownLoop : public CloseHook
{
public:
    ShutdownLoop(EventLoop& loop) : m_loop(loop) {}
    void onClose(Base& interface, bool remote) override
    {
        if (!remote) m_loop.shutdown();
    }
    EventLoop& m_loop;
};

BOOST_AUTO_TEST_SUITE(capnp_tests)

BOOST_AUTO_TEST_CASE(capnp_tests)
{
    std::promise<std::unique_ptr<ProxyClient<messages::FooInterface>>> foo_promise;
    std::thread thread([&]() {
        EventLoop loop(std::move(thread));
        auto pipe = loop.m_io_context.provider->newTwoWayPipe();
        auto network_server =
            ::capnp::TwoPartyVatNetwork(*pipe.ends[0], ::capnp::rpc::twoparty::Side::SERVER, ::capnp::ReaderOptions());
        auto network_client =
            ::capnp::TwoPartyVatNetwork(*pipe.ends[1], ::capnp::rpc::twoparty::Side::CLIENT, ::capnp::ReaderOptions());
        auto foo_server = kj::heap<ProxyServer<messages::FooInterface>>(MakeFooInterface(), loop);
        auto rpc_server = ::capnp::makeRpcServer(network_server, ::capnp::Capability::Client(kj::mv(foo_server)));
        auto rpc_client = ::capnp::makeRpcClient(network_client);
        auto foo_client = MakeUnique<ProxyClient<messages::FooInterface>>(
            rpc_client.bootstrap(ServerVatId().vat_id).castAs<messages::FooInterface>(), loop);

        // foo_client->addCloseHook(MakeUnique<ShutdownLoop>(loop));
        foo_promise.set_value(std::move(foo_client));
        loop.loop();
    });

    auto foo = foo_promise.get_future().get();
    BOOST_CHECK_EQUAL(foo->add(1, 2), 3);

    FooStruct in;
    in.name = "name";
    FooStruct out = foo->pass(in);
    BOOST_CHECK_EQUAL(in.name, out.name);
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace test
} // namespace capnp
} // namespace interface
