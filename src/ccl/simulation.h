#ifndef BITCOIN_SIMULATION_H
#define BITCOIN_SIMULATION_H

#include "txmempool.h"
#include "core.h"
#include "boost/date_time/gregorian/gregorian.hpp" //include all types plus i/o
#include "boost/filesystem.hpp"

#include <string>
#include <serialize.h>

using namespace boost::gregorian;

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

class Simulation {
    public:
        Simulation(date startdate, date enddate, std::string datadir, bool loadMempool);
        ~Simulation() {}

        void operator()();
        // Query the simulation for the current time (micros since epoch)
        int64_t Clock() { return timeInMicros; }

    private:
    	void LoadFiles(date d);
    	void InitAutoFile(CAutoFile &which, std::string fileprefix, date d);
        template<class T>
        bool ReadEvent(CAutoFile &input, T *event);

        CAutoFile blkfile;
        CAutoFile txfile;
        CAutoFile mempoolfile;

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
