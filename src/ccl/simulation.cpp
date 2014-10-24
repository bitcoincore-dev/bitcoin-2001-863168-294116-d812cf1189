#include "simulation.h"
#include "init.h"
#include "main.h"
#include "util.h"
#include "ccl/cclglobals.h"

#include <string>
#include <boost/interprocess/sync/file_lock.hpp>

using namespace boost;
using namespace std;

Simulation::Simulation(date sdate, date edate, string datadir, bool loadMempool)
 : logdir(datadir),
   begindate(sdate), enddate(edate),
   loadMempoolAtStartup(loadMempool)
{
    LoadFiles(begindate);
    if (blkfile->IsNull()) {
        LogPrintf("Simulation: can't open block file, continuing without\n");
    }
    if (txfile->IsNull()) {
        LogPrintf("Simulation: can't open tx file, continuing without\n");
    }

    // Actually, this should error if the right date can't be found...
    if (loadMempoolAtStartup) {
        InitAutoFile(mempoolfile, "mempool.", begindate);
        if (mempoolfile->IsNull()) {
            LogPrintf("Simulation: can't open mempool file, continuing without\n");
        }
    }
}

void Simulation::LoadFiles(date d)
{
    InitAutoFile(txfile, "tx.", d);
    InitAutoFile(blkfile, "block.", d);
}

void Simulation::InitAutoFile(auto_ptr<CAutoFile> &which, std::string fileprefix, date d)
{
    for (date s=d; s<= enddate; s += days(1)) {
        string filename = fileprefix + boost::gregorian::to_iso_string(s);
        boost::filesystem::path fullpath = logdir / filename; 
        which.reset(new CAutoFile(fopen(fullpath.string().c_str(), "rb"),
                    SER_DISK, CLIENT_VERSION));
        if (!which->IsNull()) {
            LogPrintf("Simulation: InitAutoFile opened %s\n", fullpath.string().c_str());
            break;
        }
    }
}


void Simulation::operator()()
{
    LogPrintf("Simulation starting\n");
    
    date curdate = begindate;
    if (loadMempoolAtStartup) {
        // Start up with beginning mempool
        cclGlobals->InitMemPool(*mempoolfile);
    } else {
        LogPrintf("Simulation: not loading mempool\n");
    }
    while (curdate <= enddate) {
        bool txEOF = false;
        bool blkEOF = false;

        BlockEvent blockEvent;
        TxEvent txEvent;

        while (!txEOF || !blkEOF) {
            if (!txEOF && !txEvent.valid) {
                txEOF = !ReadEvent(*txfile, &txEvent);
            }
            // TODO: look at LoadExternalBlockFile (in main.cpp) and try
            // to understand why there's so much more code there.  Might
            // need to beef this up...
            if (!blkEOF && !blockEvent.valid) {
                blkEOF = !ReadEvent(*blkfile, &blockEvent);
            }

            if (!txEvent.valid && !blockEvent.valid) 
                break;
            else if (!txEvent.valid || (blockEvent.valid && blockEvent < txEvent)) {
                timeInMicros = blockEvent.timeMicros;
                SetMockTime(timeInMicros / 1000000);
                CValidationState state;
                ProcessBlock(state, NULL, &blockEvent.obj);
                blockEvent.reset();
            } else /* either no blocks or curTxTime <= curBlkTime */ {
                timeInMicros = txEvent.timeMicros;;
                SetMockTime(timeInMicros / 1000000);
                ProcessTransaction(txEvent.obj);
                txEvent.reset();
            }
            // LogPrintf("Simulation current time: %s.%d\n", 
            //     DateTimeStrFormat("%Y%m%d %H:%M:%S", timeInMicros/1000000).c_str(),
            //     timeInMicros%1000000);
        }
        curdate += days(1);
        LoadFiles(curdate);
    }
    LogPrintf("Simulation exiting, mempool size: %lu\n", cclGlobals->mempool->size());
    StartShutdown();
}
