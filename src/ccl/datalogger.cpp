#include "datalogger.h"
#include "cclutil.h"
#include "boost/date_time/gregorian/gregorian.hpp" //include all types plus i/o

using namespace boost::gregorian;
using namespace boost::posix_time;

DataLogger::DataLogger(string pathPrefix) : transactionLog(NULL, SER_DISK, CLIENT_VERSION), blockLog(NULL, SER_DISK, CLIENT_VERSION)
{
    if (pathPrefix == "") {
        logdir = GetDataDir();
    } else {
        logdir = pathPrefix;
    }

    RollDate();

    if (!transactionLog) {
        LogPrintf("DataLogger: Unable to create transaction log file, will proceed with no tx log\n");
    }
    if (!blockLog) {
        LogPrintf("DataLogger: Unable to create block log file, will proceed with no block log\n");
    }
}

DataLogger::~DataLogger() {}

void DataLogger::RollDate()
{
    LogPrintf("DataLogger: log files rolling to new date\n");
    if (transactionLog) {
        transactionLog.fclose();
    }
    if (blockLog) {
        blockLog.fclose();
    }
    date today(day_clock::local_day());

    InitAutoFile(transactionLog, "tx.", to_iso_string(today));
    InitAutoFile(blockLog, "block.", to_iso_string(today));

    logRotateDate = today + days(1);
}

void DataLogger::InitAutoFile(CAutoFile &which, std::string prefix, std::string curdate)
{
    std::string fullname = prefix + curdate;
    boost::filesystem::path thispath = logdir / fullname;

    if (!RotateFile(logdir, fullname)) {
        LogPrintf("DataLogger::InitAutoFile: Unable to rotate %s, check filesystem permissions\n",
            fullname);
    }
    which = fopen(thispath.string().c_str(), "ab"); // append to be safe!
}

void DataLogger::OnNewTransaction(CTransaction &tx)
{
    if (transactionLog) {
        if (day_clock::local_day() >= logRotateDate) {
            RollDate();
        }
        transactionLog << GetTimeMicros();
        transactionLog << tx;
    }
}

void DataLogger::OnNewBlock(CBlock &block)
{
    if (blockLog) {
        if (day_clock::local_day() >= logRotateDate) {
            RollDate();
        }
        blockLog << GetTimeMicros();
        blockLog << block;
    }
}