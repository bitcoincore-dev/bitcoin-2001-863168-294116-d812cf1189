#ifndef CCL_GLOBALS_H
#define CCL_GLOBALS_H

#include "txmempool.h"
#include "ccl/datalogger.h"
#include <string>
#include <memory>

using namespace std;

class CCLGlobals {
    public:
        CCLGlobals();
        ~CCLGlobals();

        void UpdateUsage(string &strUsage);
        void Init(CTxMemPool *pool);
        void Shutdown();

        void InitMemPool(CAutoFile &mempoolLog);

        auto_ptr<DataLogger> dlog;

        // Try to reduce reliance on global namespace
        CTxMemPool * mempool;
};

extern CCLGlobals * cclGlobals;

#endif