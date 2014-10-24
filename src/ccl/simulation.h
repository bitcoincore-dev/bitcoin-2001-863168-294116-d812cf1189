#ifndef BITCOIN_SIMULATION_H
#define BITCOIN_SIMULATION_H

#include "txmempool.h"
#include "core.h"
#include "boost/date_time/gregorian/gregorian.hpp" //include all types plus i/o
#include "boost/filesystem.hpp"

#include <string>
#include <serialize.h>

#include <memory>

using namespace boost::gregorian;
using namespace std;

/**
 * CCLEvent and derived classes BlockEvent/TxEvent allow for
 * code deduplication; @see ReadEvent below.  BlockEvent and TxEvent
 * are wrappers around the Block and Transaction objects.
 */

struct CCLEvent {
    CCLEvent() { reset(); }
    virtual void reset() { timeMicros = INT64_MAX; valid = false; }
    int64_t timeMicros;
    bool valid;

    virtual bool operator<(CCLEvent &b) { return timeMicros < b.timeMicros; }
};

struct BlockEvent : public CCLEvent {
    BlockEvent() : CCLEvent() { }
    CBlock obj;
};

struct TxEvent : public CCLEvent {
    TxEvent() : CCLEvent() {}
    CTransaction obj;
};

/**
 * Simulation: plays historical data (@see DataLogger) back through bitcoind.
 *
 * Usage: Construct with dates to run the simulation, along with path to directory
 *        where data is stored, and whether to start with an empty or pre-
 *        populated mempool.
 * 
 * Currently only delivers events to bitcoind's main.cpp functions 
 * (ProcessBlock and a new ProcessTransaction that mirrors the code that
 * handles transactions coming in from the network).
 *
 * Should probably not use this code with the code that connects to peers
 * over the network; preventing that is handled by init.cpp.
 *
 * This only works if you have a bitcoin datadir that is setup with the 
 * blockindex and chainstate as of midnight on startdate.
 */

class Simulation {
public:
    Simulation(date startdate, date enddate, std::string datadir, bool loadMempool);
    ~Simulation() {}

    void operator()();
    // Query the simulation for the current time (micros since epoch)
    int64_t Clock() { return timeInMicros; }

private:
    void LoadFiles(date d);
    void InitAutoFile(auto_ptr<CAutoFile> &which, std::string fileprefix, date d);
    template<class T> bool ReadEvent(CAutoFile &input, T *event);

    auto_ptr<CAutoFile> blkfile;
    auto_ptr<CAutoFile> txfile;
    auto_ptr<CAutoFile> mempoolfile;

    boost::filesystem::path logdir;

    date begindate, enddate;
    bool loadMempoolAtStartup;

    int64_t timeInMicros; // microseconds since epoch
};

template<class T>
bool Simulation::ReadEvent(CAutoFile &input, T *event)
{
    try {
        input >> event->timeMicros;
        input >> event->obj;
        event->valid = true;
    } catch (std::ios_base::failure) {
        event->reset();
        return false;
    }
    return true;
}

#endif
