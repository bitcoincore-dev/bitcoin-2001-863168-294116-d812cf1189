#ifndef DATA_LOGGER_H
#define DATA_LOGGER_H

#include "core.h"
#include "util.h"
#include <string>

using namespace std;

class DataLogger {
private:
	CAutoFile transactionLog;
	CAutoFile blockLog;
	// Store the path where we're putting
	// all the data files, for log rotation
	// purposes
	boost::filesystem::path logdir;

	boost::gregorian::date logRotateDate;

	void InitAutoFile(CAutoFile &which, std::string prefix, std::string curdate);
	void RollDate();

public:
	DataLogger(string pathPrefix);
	~DataLogger();

	void OnNewTransaction(CTransaction &tx);
	void OnNewBlock(CBlock &block);
};

#endif
