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

void CCLGlobals::Init()
{
	if (mapArgs.count("-dlogdir")) {
		this->dlog.reset(new DataLogger(mapArgs["-dlogdir"]));
	}
}
