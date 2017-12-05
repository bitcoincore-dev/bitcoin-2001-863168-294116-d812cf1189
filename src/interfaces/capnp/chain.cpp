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
#include <rpc/server.h>

namespace interfaces {
namespace capnp {

std::unique_ptr<Handler> ProxyServerMethodTraits<messages::Chain::HandleNotificationsParams>::invoke(Context& context)
{
    auto params = context.call_context.getParams();
    auto notifications = MakeUnique<ProxyClient<messages::ChainNotifications>>(
        params.getNotifications(), *context.proxy_server.m_connection);
    auto handler = context.proxy_server.m_impl->handleNotifications(*notifications);
    handler->addCloseHook(MakeUnique<Deleter<decltype(notifications)>>(std::move(notifications)));
    return handler;
}

void ProxyServerMethodTraits<messages::Chain::RequestMempoolTransactionsParams>::invoke(Context& context)
{
    auto params = context.call_context.getParams();
    auto notifications = MakeUnique<ProxyClient<messages::ChainNotifications>>(
        params.getNotifications(), *context.proxy_server.m_connection);
    context.proxy_server.m_impl->requestMempoolTransactions(*notifications);
}

std::unique_ptr<Handler> ProxyServerMethodTraits<messages::Chain::HandleRpcParams>::invoke(Context& context)
{
    auto params = context.call_context.getParams();
    auto command = params.getCommand();

    CRPCCommand::Actor actor;
    ReadFieldUpdate(TypeList<decltype(actor)>(), context, Make<ValueField>(command.getActor()), actor);
    std::vector<std::string> args;
    ReadFieldUpdate(TypeList<decltype(args)>(), context, Make<ValueField>(command.getArgNames()), args);

    auto rpc_command = MakeUnique<CRPCCommand>(
        command.getCategory(), command.getName(), std::move(actor), std::move(args), command.getUniqueId());
    auto handler = context.proxy_server.m_impl->handleRpc(*rpc_command);
    handler->addCloseHook(MakeUnique<Deleter<decltype(rpc_command)>>(std::move(rpc_command)));
    return handler;
}

ProxyServerCustom<messages::ChainClient, ChainClient>::ProxyServerCustom(ChainClient* impl,
    bool owned,
    Connection& connection)
    : ProxyServerBase(impl, owned, connection)
{
}

void ProxyServerCustom<messages::ChainClient, ChainClient>::invokeDestroy()
{
    if (m_scheduler) {
        m_scheduler->stop();
        m_result.get();
        m_scheduler.reset();
    }
    ProxyServerBase::invokeDestroy();
}

void ProxyServerMethodTraits<messages::ChainClient::StartParams>::invoke(Context& context)
{
    if (!context.proxy_server.m_scheduler) {
        context.proxy_server.m_scheduler = MakeUnique<CScheduler>();
        CScheduler* scheduler = context.proxy_server.m_scheduler.get();
        context.proxy_server.m_result = std::async([scheduler]() {
            util::ThreadRename("schedqueue");
            scheduler->serviceQueue();
        });
    }
    context.proxy_server.m_impl->start(*context.proxy_server.m_scheduler);
}

void ProxyClientCustom<messages::ChainNotifications, Chain::Notifications>::TransactionAddedToMempool(
    const CTransactionRef& tx)
{
    self().transactionAddedToMempool(tx);
}
void ProxyClientCustom<messages::ChainNotifications, Chain::Notifications>::TransactionRemovedFromMempool(
    const CTransactionRef& ptx)
{
    self().transactionRemovedFromMempool(ptx);
}
void ProxyClientCustom<messages::ChainNotifications, Chain::Notifications>::BlockConnected(const CBlock& block,
    const std::vector<CTransactionRef>& tx_conflicted)
{
    self().blockConnected(block, tx_conflicted);
}
void ProxyClientCustom<messages::ChainNotifications, Chain::Notifications>::BlockDisconnected(const CBlock& block)
{
    self().blockDisconnected(block);
}
void ProxyClientCustom<messages::ChainNotifications, Chain::Notifications>::ChainStateFlushed(
    const CBlockLocator& locator)
{
    self().chainStateFlushed(locator);
}

} // namespace capnp
} // namespace interfaces
