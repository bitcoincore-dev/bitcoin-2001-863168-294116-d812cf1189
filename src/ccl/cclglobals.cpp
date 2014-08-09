#include "cclglobals.h"
#include "init.h"
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

    // Simulation options
    strUsage += "  -simulation            " + _("Sim mode! Don't call add networking threads to threadgroup") + "\n";
    strUsage += "      -simdatadir=<dir>     " + _("For simulations: specify data directory (default: /data/)") + "\n";
    strUsage += "      -start=<YYYYMMDD>  " + _("For simulations: start date") + "\n";
    strUsage += "      -end=<YYYYMMDD>    " + _("For simulations: end date (defaults to start date)") + "\n";

}

bool CCLGlobals::Init(CTxMemPool *pool)
{
    mempool = pool;

    // DataLogger initialization
    if (mapArgs.count("-dlogdir")) {
	    this->dlog.reset(new DataLogger(mapArgs["-dlogdir"]));
    }

    // Simulation initialization
    std::string startdate, enddate, simdatadir="/data";
    if (mapArgs.count("-simulation")) {
        if (mapArgs.count("-start")) {
            startdate = mapArgs["-start"];
        } else {
            LogPrintf("CCLGlobals::Init: Must specify -start (date) for simulation\n");
            return false;
        }
        if (mapArgs.count("-end")) {
            enddate = mapArgs["-end"];
        } else {
            enddate = startdate;
        }
        if (mapArgs.count("-simdatadir")) {
            simdatadir = mapArgs["-simdatadir"];
        }
        simulation.reset(new 
            Simulation(boost::gregorian::from_undelimited_string(startdate),
                boost::gregorian::from_undelimited_string(enddate),
                simdatadir)
        );
    }
    return true;
}

bool CCLGlobals::Run(boost::thread_group &threadGroup)
{
    if (simulation.get() != NULL) {
        threadGroup.create_thread(boost::ref(*simulation.get()));
        return true;  // means don't use network
    } else {
        return false;
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
