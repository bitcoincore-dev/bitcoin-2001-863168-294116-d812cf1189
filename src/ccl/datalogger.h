#ifndef DATA_LOGGER_H
#define DATA_LOGGER_H

#include "core.h"
#include "txmempool.h"
#include "util.h"
#include <string>

using namespace std;

/**
 * DataLogger: log block and tx data as it arrives.
 *
 * Usage: Pass in directory name to hold the log files in to the constructor.
 *        Will create block.<date> and tx.<date> files that store blocks
 *        received and transactions received on that date.  Also creates
 *        mempool.<date> representing the state of the mempool at the beginning
 *        of the date (midnight NYC time).
 *        Will load up the previous mempool at startup.
 *        Requires bitcoind to call OnNewTransaction and OnNewBlock methods.
 */

class DataLogger {
private:
    CAutoFile transactionLog;
    CAutoFile blockLog;
    CAutoFile mempoolLog;

    // Store the path where we're putting
    // all the data files, for log rotation
    // purposes
    boost::filesystem::path logdir;

    boost::gregorian::date logRotateDate;

    void InitAutoFile(CAutoFile &which, std::string prefix, std::string curdate);
    void RollDate();
    void LoadOldMempool();

public:
    void Shutdown();

public:
    DataLogger(string pathPrefix);
    ~DataLogger();

    void OnNewTransaction(CTransaction &tx);
    void OnNewBlock(CBlock &block);
};

#endif
