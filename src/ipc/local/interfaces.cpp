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
    HandlerImpl(boost::signals2::connection connection) : connection(std::move(connection)) {}

    void disconnect() override { connection.disconnect(); }

    boost::signals2::scoped_connection connection;
};

class NodeImpl : public Node
{
public:
    void parseParameters(int argc, const char* const argv[]) override { ::ParseParameters(argc, argv); }
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
    std::unique_ptr<Handler> handleInitMessage(InitMessageFn fn) override
    {
        return MakeUnique<HandlerImpl>(uiInterface.InitMessage.connect(fn));
    }

    boost::thread_group threadGroup;
    ::CScheduler scheduler;
};

} // namespace

std::unique_ptr<Node> MakeNode() { return MakeUnique<NodeImpl>(); }

} // namespace local
} // namespace ipc
