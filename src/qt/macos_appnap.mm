// Copyright (c) 2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "macos_appnap.h"

#include <AvailabilityMacros.h>
#include <Foundation/NSProcessInfo.h>
#include <Foundation/Foundation.h>

class CAppNapInhibitor::Private
{
public:
    NSObject* activityId;
};

CAppNapInhibitor::CAppNapInhibitor(const char* strReason) : d(new Private)
{
#if defined(MAC_OS_X_VERSION_MAX_ALLOWED) &&  MAC_OS_X_VERSION_MAX_ALLOWED >= 1090
    const NSActivityOptions activityOptions =
    NSActivityUserInitiatedAllowingIdleSystemSleep &
    ~(NSActivitySuddenTerminationDisabled |
      NSActivityAutomaticTerminationDisabled);

    id processInfo = [NSProcessInfo processInfo];
    if ([processInfo respondsToSelector:@selector(beginActivityWithOptions:reason:)]) {
        @autoreleasepool {
            d->activityId = [processInfo beginActivityWithOptions: activityOptions reason:@(strReason)];
            [d->activityId retain];
        }
    }
#endif
}

CAppNapInhibitor::~CAppNapInhibitor()
{
#if defined(MAC_OS_X_VERSION_MAX_ALLOWED) &&  MAC_OS_X_VERSION_MAX_ALLOWED >= 1090
    @autoreleasepool {
        id processInfo = [NSProcessInfo processInfo];
        if (d->activityId && [processInfo respondsToSelector:@selector(endActivity:)]) {
            [processInfo endActivity:d->activityId];
        }
        [d->activityId release];
    }
#endif
    delete d;
}
