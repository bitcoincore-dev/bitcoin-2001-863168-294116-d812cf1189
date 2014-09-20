#ifndef CCL_GLOBALS_H
#define CCL_GLOBALS_H

#include "txmempool.h"
#include "ccl/datalogger.h"
#include "ccl/simulation.h"
#include <boost/thread/thread.hpp>
#include <string>
#include <memory>

using namespace std;

class CCLGlobals {
    public:
        CCLGlobals();
        ~CCLGlobals();

        void UpdateUsage(string &strUsage);
        bool Init(CTxMemPool *pool);
        void Shutdown();

        void InitMemPool(CAutoFile &mempoolLog);
        bool Run(boost::thread_group &threadGroup);

        void WriteMempool(CAutoFile &logfile);

        auto_ptr<DataLogger> dlog;
	auto_ptr<Simulation> simulation;

        // Try to reduce reliance on global namespace
        CTxMemPool * mempool;

    private:
        bool writeMempoolAtShutdown;
        std::string outputFileName;
};

extern CCLGlobals * cclGlobals;

#endif
