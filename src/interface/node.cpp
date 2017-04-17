#include <interface/node.h>

#include <chainparams.h>
#include <init.h>
#include <interface/handler.h>
#include <interface/util.h>
#include <scheduler.h>
#include <ui_interface.h>
#include <util.h>
#include <warnings.h>

#include <boost/thread/thread.hpp>

namespace interface {
namespace {

class NodeImpl : public Node
{
public:
    void parseParameters(int argc, const char* const argv[]) override { gArgs.ParseParameters(argc, argv); }
    void readConfigFile(const std::string& conf_path) override { gArgs.ReadConfigFile(conf_path); }
    void selectParams(const std::string& network) override { SelectParams(network); }
    void initLogging() override { InitLogging(); }
    void initParameterInteraction() override { InitParameterInteraction(); }
    std::string getWarnings(const std::string& type) override { return GetWarnings(type); }
    bool baseInitialize() override
    {

        return AppInitBasicSetup() && AppInitParameterInteraction() && AppInitSanityChecks() &&
               AppInitLockDataDirectory();
    }
    bool appInitMain() override { return AppInitMain(m_thread_group, m_scheduler); }
    void appShutdown() override
    {
        Interrupt(m_thread_group);
        m_thread_group.join_all();
        Shutdown();
    }
    void startShutdown() override { StartShutdown(); }
    std::unique_ptr<Handler> handleInitMessage(InitMessageFn fn) override
    {
        return MakeHandler(::uiInterface.InitMessage.connect(fn));
    }

    boost::thread_group m_thread_group;
    ::CScheduler m_scheduler;
};

} // namespace

std::unique_ptr<Node> MakeNode() { return MakeUnique<NodeImpl>(); }

} // namespace interface
