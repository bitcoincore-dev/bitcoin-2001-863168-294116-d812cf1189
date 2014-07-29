#ifndef CCL_GLOBALS_H
#define CCL_GLOBALS_H

#include "ccl/datalogger.h"
#include <string>
#include <memory>

using namespace std;

class CCLGlobals {
	public:
		CCLGlobals();
		~CCLGlobals();

		void UpdateUsage(string &strUsage);
		void Init();

		auto_ptr<DataLogger> dlog;
};

extern CCLGlobals * cclGlobals;

#endif