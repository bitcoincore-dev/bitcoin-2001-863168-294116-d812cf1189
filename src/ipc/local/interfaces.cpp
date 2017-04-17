#include <ipc/interfaces.h>

#include <chainparams.h>
#include <init.h>
#include <ipc/util.h>
#include <net.h>
#include <net_processing.h>
#include <netbase.h>
#include <rpc/server.h>
#include <scheduler.h>
#include <txmempool.h>
#include <ui_interface.h>
#include <util.h>
#include <validation.h>
#include <warnings.h>

#if defined(HAVE_CONFIG_H)
#include "config/bitcoin-config.h"
#endif
#ifdef ENABLE_WALLET
#include "wallet/wallet.h"
#endif

#include <boost/thread.hpp>
#include <univalue.h>

namespace ipc {
namespace local {
namespace {

#ifdef ENABLE_WALLET
#define CHECK_WALLET(x) x
#else
#define CHECK_WALLET(x) throw std::logic_error("Wallet function called in non-wallet build.")
#endif

class HandlerImpl : public Handler
{
public:
    HandlerImpl(boost::signals2::connection connection) : m_connection(std::move(connection)) {}

    void disconnect() override { m_connection.disconnect(); }

    boost::signals2::scoped_connection m_connection;
};

#ifdef ENABLE_WALLET
class WalletImpl : public Wallet
{
public:
    WalletImpl(CWallet& wallet) : m_wallet(wallet) {}

    std::unique_ptr<Handler> handleShowProgress(ShowProgressFn fn) override
    {
        return MakeUnique<HandlerImpl>(m_wallet.ShowProgress.connect(fn));
    }

    CWallet& m_wallet;
};
#endif

class NodeImpl : public Node
{
public:
    void parseParameters(int argc, const char* const argv[]) override { ::ParseParameters(argc, argv); }
    bool softSetArg(const std::string& arg, const std::string& value) override { return ::SoftSetArg(arg, value); }
    bool softSetBoolArg(const std::string& arg, bool value) override { return ::SoftSetBoolArg(arg, value); }
    void readConfigFile(const std::string& conf_path) override { ::ReadConfigFile(conf_path); }
    void selectParams(const std::string& network) override { ::SelectParams(network); }
    void initLogging() override { ::InitLogging(); }
    void initParameterInteraction() override { ::InitParameterInteraction(); }
    std::string getWarnings(const std::string& type) override { return ::GetWarnings(type); }
    bool baseInitialize() override
    {
        return ::AppInitBasicSetup() && ::AppInitParameterInteraction() && ::AppInitSanityChecks() &&
               ::AppInitLockDataDirectory();
    }
    bool appInitMain() override { return ::AppInitMain(m_thread_group, m_scheduler); }
    void appShutdown() override
    {
        ::Interrupt(m_thread_group);
        m_thread_group.join_all();
        ::Shutdown();
    }
    void startShutdown() override { ::StartShutdown(); }
    bool shutdownRequested() override { return ::ShutdownRequested(); }
    bool interruptInit() override
    {
        std::function<void(void)> action;
        {
            LOCK(m_break_action_lock);
            action = m_break_action;
        }
        if (action) {
            action();
            return true;
        }
        return false;
    }
    std::string helpMessage(HelpMessageMode mode) override { return ::HelpMessage(mode); }
    void mapPort(bool use_upnp) override { ::MapPort(use_upnp); }
    bool getProxy(Network net, proxyType& proxy_info) override { return ::GetProxy(net, proxy_info); }
    size_t getNodeCount(CConnman::NumConnections flags) override
    {
        return ::g_connman ? ::g_connman->GetNodeCount(flags) : 0;
    }
    bool getNodesStats(NodesStats& stats) override
    {
        stats.clear();

        if (::g_connman) {
            std::vector<CNodeStats> stats_temp;
            ::g_connman->GetNodeStats(stats_temp);

            stats.reserve(stats_temp.size());
            for (auto& node_stats_temp : stats_temp) {
                stats.emplace_back(std::move(node_stats_temp), false, CNodeStateStats());
            }

            // Try to retrieve the CNodeStateStats for each node.
            TRY_LOCK(::cs_main, lockMain);
            if (lockMain) {
                for (auto& node_stats : stats) {
                    std::get<1>(node_stats) =
                        GetNodeStateStats(std::get<0>(node_stats).nodeid, std::get<2>(node_stats));
                }
            }
            return true;
        }
        return false;
    }
    bool getBanned(banmap_t& banmap)
    {
        if (::g_connman) {
            ::g_connman->GetBanned(banmap);
            return true;
        }
        return false;
    }
    bool ban(const CNetAddr& net_addr, BanReason reason, int64_t ban_time_offset) override
    {
        if (::g_connman) {
            ::g_connman->Ban(net_addr, reason, ban_time_offset);
            return true;
        }
        return false;
    }
    bool unban(const CSubNet& ip) override
    {
        if (::g_connman) {
            ::g_connman->Unban(ip);
            return true;
        }
        return false;
    }
    bool disconnect(NodeId id) override
    {
        if (::g_connman) {
            return ::g_connman->DisconnectNode(id);
        }
        return false;
    }
    int64_t getTotalBytesRecv() override { return ::g_connman ? ::g_connman->GetTotalBytesRecv() : 0; }
    int64_t getTotalBytesSent() override { return ::g_connman ? ::g_connman->GetTotalBytesSent() : 0; }
    size_t getMempoolSize() override { return ::mempool.size(); }
    size_t getMempoolDynamicUsage() override { return ::mempool.DynamicMemoryUsage(); }
    bool getHeaderTip(int& height, int64_t& block_time) override
    {
        LOCK(::cs_main);
        if (::pindexBestHeader) {
            height = ::pindexBestHeader->nHeight;
            block_time = ::pindexBestHeader->GetBlockTime();
            return true;
        }
        return false;
    }
    int getNumBlocks() override
    {
        LOCK(::cs_main);
        return ::chainActive.Height();
    }
    int64_t getLastBlockTime() override
    {
        LOCK(::cs_main);
        if (::chainActive.Tip()) {
            return ::chainActive.Tip()->GetBlockTime();
        }
        return ::Params().GenesisBlock().GetBlockTime(); // Genesis block's time of current network
    }
    double getVerificationProgress() override
    {
        const CBlockIndex* tip;
        {
            LOCK(::cs_main);
            tip = ::chainActive.Tip();
        }
        return ::GuessVerificationProgress(::Params().TxData(), tip);
    }
    bool isInitialBlockDownload() override { return ::IsInitialBlockDownload(); }
    bool getReindex() override { return ::fReindex; }
    bool getImporting() override { return ::fImporting; }
    void setNetworkActive(bool active) override
    {
        if (::g_connman) {
            ::g_connman->SetNetworkActive(active);
        }
    }
    bool getNetworkActive() override { return ::g_connman && ::g_connman->GetNetworkActive(); }
    UniValue executeRpc(const std::string& command, const UniValue& params) override
    {
        JSONRPCRequest req;
        req.params = params;
        req.strMethod = command;
        return ::tableRPC.execute(req);
    }
    std::vector<std::string> listRpcCommands() override { return ::tableRPC.listCommands(); }
    void rpcSetTimerInterfaceIfUnset(RPCTimerInterface* iface) override { ::RPCSetTimerInterfaceIfUnset(iface); }
    void rpcUnsetTimerInterface(RPCTimerInterface* iface) override { ::RPCUnsetTimerInterface(iface); }
    std::unique_ptr<Handler> handleInitMessage(InitMessageFn fn) override
    {
        return MakeUnique<HandlerImpl>(::uiInterface.InitMessage.connect(fn));
    }
    std::unique_ptr<Handler> handleMessageBox(MessageBoxFn fn) override
    {
        return MakeUnique<HandlerImpl>(::uiInterface.ThreadSafeMessageBox.connect(fn));
    }
    std::unique_ptr<Handler> handleQuestion(QuestionFn fn) override
    {
        return MakeUnique<HandlerImpl>(::uiInterface.ThreadSafeQuestion.connect(fn));
    }
    std::unique_ptr<Handler> handleShowProgress(ShowProgressFn fn) override
    {
        return MakeUnique<HandlerImpl>(::uiInterface.ShowProgress.connect(fn));
    }
    std::unique_ptr<Handler> handleLoadWallet(LoadWalletFn fn) override
    {
        CHECK_WALLET(return MakeUnique<HandlerImpl>(
            ::uiInterface.LoadWallet.connect([fn](CWallet* wallet) { fn(MakeUnique<WalletImpl>(*wallet)); })));
    }
    std::unique_ptr<Handler> handleNotifyNumConnectionsChanged(NotifyNumConnectionsChangedFn fn) override
    {
        return MakeUnique<HandlerImpl>(::uiInterface.NotifyNumConnectionsChanged.connect(fn));
    }
    std::unique_ptr<Handler> handleNotifyNetworkActiveChanged(NotifyNetworkActiveChangedFn fn) override
    {
        return MakeUnique<HandlerImpl>(::uiInterface.NotifyNetworkActiveChanged.connect(fn));
    }
    std::unique_ptr<Handler> handleNotifyAlertChanged(NotifyAlertChangedFn fn) override
    {
        return MakeUnique<HandlerImpl>(::uiInterface.NotifyAlertChanged.connect(fn));
    }
    std::unique_ptr<Handler> handleBannedListChanged(BannedListChangedFn fn) override
    {
        return MakeUnique<HandlerImpl>(::uiInterface.BannedListChanged.connect(fn));
    }
    std::unique_ptr<Handler> handleNotifyBlockTip(NotifyBlockTipFn fn) override
    {
        return MakeUnique<HandlerImpl>(
            ::uiInterface.NotifyBlockTip.connect([fn](bool initial_download, const CBlockIndex* block) {
                fn(initial_download, block->nHeight, block->GetBlockTime(),
                    ::GuessVerificationProgress(::Params().TxData(), block));
            }));
    }
    std::unique_ptr<Handler> handleNotifyHeaderTip(NotifyHeaderTipFn fn) override
    {
        return MakeUnique<HandlerImpl>(
            ::uiInterface.NotifyHeaderTip.connect([fn](bool initial_download, const CBlockIndex* block) {
                fn(initial_download, block->nHeight, block->GetBlockTime(),
                    ::GuessVerificationProgress(::Params().TxData(), block));
            }));
    }

    boost::thread_group m_thread_group;
    ::CScheduler m_scheduler;
    std::function<void(void)> m_break_action;
    CCriticalSection m_break_action_lock;
    boost::signals2::scoped_connection m_break_action_connection{
        ::uiInterface.SetProgressBreakAction.connect([this](std::function<void(void)> action) {
            LOCK(m_break_action_lock);
            m_break_action = std::move(action);
        })};
};

} // namespace

std::unique_ptr<Node> MakeNode() { return MakeUnique<NodeImpl>(); }

} // namespace local
} // namespace ipc
