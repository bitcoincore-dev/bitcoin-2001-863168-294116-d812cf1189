#include <ipc/interfaces.h>

#include <chainparams.h>
#include <init.h>
#include <ipc/util.h>
#include <net.h>
#include <netbase.h>
#include <scheduler.h>
#include <txmempool.h>
#include <ui_interface.h>
#include <util.h>
#include <validation.h>

#if defined(HAVE_CONFIG_H)
#include "config/bitcoin-config.h"
#endif
#ifdef ENABLE_WALLET
#include "wallet/wallet.h"
#endif

#include <boost/thread.hpp>

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
    HandlerImpl(boost::signals2::connection connection) : connection(std::move(connection)) {}

    void disconnect() override { connection.disconnect(); }

    boost::signals2::scoped_connection connection;
};

#ifdef ENABLE_WALLET
class WalletImpl : public Wallet
{
public:
    WalletImpl(CWallet& wallet) : wallet(wallet) {}

    std::unique_ptr<Handler> handleShowProgress(ShowProgressFn fn) override
    {
        return MakeUnique<HandlerImpl>(wallet.ShowProgress.connect(fn));
    }

    CWallet& wallet;
};
#endif

class NodeImpl : public Node
{
public:
    void parseParameters(int argc, const char* const argv[]) override { ::ParseParameters(argc, argv); }
    bool softSetArg(const std::string& arg, const std::string& value) override { return ::SoftSetArg(arg, value); }
    bool softSetBoolArg(const std::string& arg, bool value) override { return ::SoftSetBoolArg(arg, value); }
    void readConfigFile(const std::string& confPath) override { ::ReadConfigFile(confPath); }
    void selectParams(const std::string& network) override { ::SelectParams(network); }
    void initLogging() override { ::InitLogging(); }
    void initParameterInteraction() override { ::InitParameterInteraction(); }
    std::string getWarnings(const std::string& type) override { return ::GetWarnings(type); }
    bool appInit() override
    {
        return ::AppInitBasicSetup() && ::AppInitParameterInteraction() && ::AppInitSanityChecks() &&
               ::AppInitMain(threadGroup, scheduler);
    }
    void appShutdown() override
    {
        ::Interrupt(threadGroup);
        threadGroup.join_all();
        ::Shutdown();
    }
    void startShutdown() override { ::StartShutdown(); }
    bool shutdownRequested() override { return ::ShutdownRequested(); }
    std::string helpMessage(HelpMessageMode mode) override { return ::HelpMessage(mode); }
    void mapPort(bool useUPnP) override { ::MapPort(useUPnP); }
    bool getProxy(Network net, proxyType& proxyInfo) override { return ::GetProxy(net, proxyInfo); }
    size_t getNodeCount(CConnman::NumConnections flags) override
    {
        return g_connman ? g_connman->GetNodeCount(flags) : 0;
    }
    int64_t getTotalBytesRecv() override { return g_connman ? g_connman->GetTotalBytesRecv() : 0; }
    int64_t getTotalBytesSent() override { return g_connman ? g_connman->GetTotalBytesSent() : 0; }
    size_t getMempoolSize() override { return mempool.size(); }
    size_t getMempoolDynamicUsage() override { return mempool.DynamicMemoryUsage(); }
    bool getHeaderTip(int& height, int64_t& blockTime) override
    {
        LOCK(cs_main);
        if (pindexBestHeader) {
            height = pindexBestHeader->nHeight;
            blockTime = pindexBestHeader->GetBlockTime();
            return true;
        }
        return false;
    }
    int getNumBlocks() override
    {
        LOCK(cs_main);
        return chainActive.Height();
    }
    int64_t getLastBlockTime() override
    {
        LOCK(cs_main);
        if (chainActive.Tip()) {
            return chainActive.Tip()->GetBlockTime();
        }
        return Params().GenesisBlock().GetBlockTime(); // Genesis block's time of current network
    }
    double getVerificationProgress() override
    {
        const CBlockIndex* tip;
        {
            LOCK(cs_main);
            tip = chainActive.Tip();
        }
        return ::GuessVerificationProgress(Params().TxData(), tip);
    }
    bool isInitialBlockDownload() override { return ::IsInitialBlockDownload(); }
    bool getReindex() override { return ::fReindex; }
    bool getImporting() override { return ::fImporting; }
    void setNetworkActive(bool active) override
    {
        if (g_connman) {
            g_connman->SetNetworkActive(active);
        }
    }
    bool getNetworkActive() override { return g_connman && g_connman->GetNetworkActive(); }
    std::unique_ptr<Handler> handleInitMessage(InitMessageFn fn) override
    {
        return MakeUnique<HandlerImpl>(uiInterface.InitMessage.connect(fn));
    }
    std::unique_ptr<Handler> handleMessageBox(MessageBoxFn fn) override
    {
        return MakeUnique<HandlerImpl>(uiInterface.ThreadSafeMessageBox.connect(fn));
    }
    std::unique_ptr<Handler> handleQuestion(QuestionFn fn) override
    {
        return MakeUnique<HandlerImpl>(uiInterface.ThreadSafeQuestion.connect(fn));
    }
    std::unique_ptr<Handler> handleShowProgress(ShowProgressFn fn) override
    {
        return MakeUnique<HandlerImpl>(uiInterface.ShowProgress.connect(fn));
    }
    std::unique_ptr<Handler> handleLoadWallet(LoadWalletFn fn) override
    {
        CHECK_WALLET(return MakeUnique<HandlerImpl>(
            uiInterface.LoadWallet.connect([fn](CWallet* wallet) { fn(MakeUnique<WalletImpl>(*wallet)); })));
    }
    std::unique_ptr<Handler> handleNotifyNumConnectionsChanged(NotifyNumConnectionsChangedFn fn) override
    {
        return MakeUnique<HandlerImpl>(uiInterface.NotifyNumConnectionsChanged.connect(fn));
    }
    std::unique_ptr<Handler> handleNotifyNetworkActiveChanged(NotifyNetworkActiveChangedFn fn) override
    {
        return MakeUnique<HandlerImpl>(uiInterface.NotifyNetworkActiveChanged.connect(fn));
    }
    std::unique_ptr<Handler> handleNotifyAlertChanged(NotifyAlertChangedFn fn) override
    {
        return MakeUnique<HandlerImpl>(uiInterface.NotifyAlertChanged.connect(fn));
    }
    std::unique_ptr<Handler> handleBannedListChanged(BannedListChangedFn fn) override
    {
        return MakeUnique<HandlerImpl>(uiInterface.BannedListChanged.connect(fn));
    }
    std::unique_ptr<Handler> handleNotifyBlockTip(NotifyBlockTipFn fn) override
    {
        return MakeUnique<HandlerImpl>(
            uiInterface.NotifyBlockTip.connect([fn](bool initialDownload, const CBlockIndex* block) {
                fn(initialDownload, block->nHeight, block->GetBlockTime(),
                    GuessVerificationProgress(Params().TxData(), block));
            }));
    }
    std::unique_ptr<Handler> handleNotifyHeaderTip(NotifyHeaderTipFn fn) override
    {
        return MakeUnique<HandlerImpl>(
            uiInterface.NotifyHeaderTip.connect([fn](bool initialDownload, const CBlockIndex* block) {
                fn(initialDownload, block->nHeight, block->GetBlockTime(),
                    GuessVerificationProgress(Params().TxData(), block));
            }));
    }

    boost::thread_group threadGroup;
    ::CScheduler scheduler;
};

} // namespace

std::unique_ptr<Node> MakeNode() { return MakeUnique<NodeImpl>(); }

} // namespace local
} // namespace ipc
