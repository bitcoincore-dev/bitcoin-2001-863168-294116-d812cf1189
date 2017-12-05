#include <interfaces/capnp/common.h>

#include <clientversion.h>
#include <init.h>
#include <interfaces/capnp/chain.capnp.h>
#include <interfaces/capnp/chain.capnp.proxy-inl.h>
#include <interfaces/capnp/chain.capnp.proxy.h>
#include <interfaces/capnp/common.capnp.h>
#include <interfaces/capnp/common.capnp.proxy-inl.h>
#include <interfaces/capnp/common.capnp.proxy.h>
#include <interfaces/capnp/handler.capnp.h>
#include <interfaces/capnp/handler.capnp.proxy-inl.h>
#include <interfaces/capnp/handler.capnp.proxy.h>
#include <interfaces/capnp/init.capnp.h>
#include <interfaces/capnp/init.capnp.proxy-inl.h>
#include <interfaces/capnp/init.capnp.proxy.h>
#include <interfaces/capnp/node.capnp.h>
#include <interfaces/capnp/node.capnp.proxy-inl.h>
#include <interfaces/capnp/node.capnp.proxy.h>
#include <interfaces/capnp/proxy-inl.h>
#include <interfaces/capnp/wallet.capnp.h>
#include <interfaces/capnp/wallet.capnp.proxy-inl.h>
#include <interfaces/capnp/wallet.capnp.proxy.h>
#include <interfaces/config.h>
#include <interfaces/node.h>
#include <key.h>
#include <net.h>
#include <net_processing.h>
#include <netaddress.h>
#include <netbase.h>
#include <policy/feerate.h>
#include <policy/fees.h>
#include <primitives/transaction.h>
#include <protocol.h>
#include <pubkey.h>
#include <rpc/server.h>
#include <rpc/util.h>
#include <scheduler.h>
#include <script/ismine.h>
#include <script/script.h>
#include <streams.h>
#include <wallet/coincontrol.h>
#include <wallet/wallet.h>

#include <boost/core/explicit_operator_bool.hpp>
#include <boost/move/utility_core.hpp>
#include <boost/optional.hpp>
#include <boost/variant/get.hpp>
#include <capnp/blob.h>
#include <capnp/list.h>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <tuple>
#include <univalue.h>
#include <utility>

namespace interfaces {
namespace capnp {

void BuildGlobalArgs(InvokeContext& invoke_context, messages::GlobalArgs::Builder&& builder)
{

    const auto& args = static_cast<const GlobalArgs&>(::gArgs);
    LOCK(args.cs_args);
    BuildField(TypeList<GlobalArgs>(), invoke_context, Make<ValueField>(builder), args);
}

void ReadGlobalArgs(InvokeContext& invoke_context, const messages::GlobalArgs::Reader& reader)
{
    auto& args = static_cast<GlobalArgs&>(::gArgs);
    LOCK(args.cs_args);
    ReadFieldUpdate(TypeList<GlobalArgs>(), invoke_context, Make<ValueField>(reader), args);
}

std::string GlobalArgsNetwork()
{
    auto& args = static_cast<GlobalArgs&>(::gArgs);
    LOCK(args.cs_args);
    return args.m_network;
}

class RpcTimer : public ::RPCTimerBase
{
public:
    RpcTimer(EventLoop& loop, std::function<void(void)>& fn, int64_t millis)
        : m_fn(fn), m_promise(loop.m_io_context.provider->getTimer()
                                  .afterDelay(millis * kj::MILLISECONDS)
                                  .then([this]() { m_fn(); })
                                  .eagerlyEvaluate(nullptr))
    {
    }
    ~RpcTimer() noexcept override {}

    std::function<void(void)> m_fn;
    kj::Promise<void> m_promise;
};

class RpcTimerInterface : public ::RPCTimerInterface
{
public:
    RpcTimerInterface(EventLoop& loop) : m_loop(loop) {}
    const char* Name() override { return "Cap'n Proto"; }
    RPCTimerBase* NewTimer(std::function<void(void)>& fn, int64_t millis) override
    {
        RPCTimerBase* result;
        m_loop.sync([&] { result = new RpcTimer(m_loop, fn, millis); });
        return result;
    }
    EventLoop& m_loop;
};

std::unique_ptr<::RPCTimerInterface> MakeTimer(EventLoop& loop) { return MakeUnique<RpcTimerInterface>(loop); }

} // namespace capnp
} // namespace interfaces
