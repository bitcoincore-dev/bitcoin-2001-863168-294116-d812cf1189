#include <ipc/interfaces.h>

#include <chainparams.h>
#include <init.h>
#include <ipc/util.h>
#include <scheduler.h>
#include <ui_interface.h>
#include <util.h>
#include <validation.h>

#include <boost/thread.hpp>

namespace ipc {
namespace local {
namespace {

class HandlerImpl : public Handler
{
public:
    HandlerImpl(boost::signals2::connection connection) : m_connection(std::move(connection)) {}

    void disconnect() override { m_connection.disconnect(); }

    boost::signals2::scoped_connection m_connection;
};

class NodeImpl : public Node
{
public:
    void parseParameters(int argc, const char* const argv[]) override { ::ParseParameters(argc, argv); }
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
    std::unique_ptr<Handler> handleInitMessage(InitMessageFn fn) override
    {
        return MakeUnique<HandlerImpl>(::uiInterface.InitMessage.connect(fn));
    }

    boost::thread_group m_thread_group;
    ::CScheduler m_scheduler;
};

} // namespace

std::unique_ptr<Node> MakeNode() { return MakeUnique<NodeImpl>(); }

} // namespace local
} // namespace ipc
