#include "cclglobals.h"
#include "ui_interface.h" // Defines the _() function!
#include "util.h"

#include <string>

CCLGlobals *cclGlobals = new CCLGlobals;

CCLGlobals::CCLGlobals() {}

CCLGlobals::~CCLGlobals() 
{
	LogPrintf("CCLGlobals: destructor\n");
}

void CCLGlobals::UpdateUsage(std::string &strUsage)
{
	strUsage += "\n" + _("CCL Options:") + "\n";

	// DataLogger options
	strUsage += "  -dlogdir=<dirname>      " + _("Turn on data logging to specified output directory") + "\n";	
}

void CCLGlobals::Init(CTxMemPool *pool)
{
    mempool = pool;
	if (mapArgs.count("-dlogdir")) {
		this->dlog.reset(new DataLogger(mapArgs["-dlogdir"]));
	}
}

void CCLGlobals::Shutdown()
{
    if (dlog.get()) dlog->WriteMempool();
}

void CCLGlobals::InitMemPool(CAutoFile &filein)
{
    int counter=0;
    LOCK(mempool->cs);

    try {
        while (1) {
            CTransaction tx;
            int64_t nFee;
            int64_t nTime;
            double dPriority;
            unsigned int nHeight;

            filein >> tx;
            filein >> nFee;
            filein >> nTime;
            filein >> dPriority;
            filein >> nHeight;

            CTxMemPoolEntry e(tx, nFee, nTime, dPriority, nHeight);
            mempool->addUnchecked(tx.GetHash(), e);
            ++counter;
        }
    } catch (std::ios_base::failure e) {

    }
    LogPrintf("CCLGlobals: added %d entries to mempool\n", counter);
}
