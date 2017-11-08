// Copyright (c) 2012-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "sync.h"
#include "test/test_bitcoin.h"

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(sync_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(potential_deadlock_detected)
{
    CCriticalSection mutex1, mutex2;
    {
        LOCK2(mutex1, mutex2);
    }
    bool error_thrown = false;
    try {
        LOCK2(mutex2, mutex1);
    } catch (const std::logic_error& e) {
        BOOST_CHECK_EQUAL(e.what(), "potential deadlock detected");
        error_thrown = true;
    }
    #ifdef DEBUG_LOCKORDER
    BOOST_CHECK(error_thrown);
    #endif
}

BOOST_AUTO_TEST_SUITE_END()
