// Copyright (c) 2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <threadutil.h>
#include <test/test_bitcoin.h>

#include <thread>
#include <vector>
#include <set>
#include <mutex>

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(threadutil_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(threadutil_test_process_rename_serial)
{
    std::string original_process_name = thread_util::GetProcessName();

    thread_util::SetProcessName("bazzz");
#ifdef CAN_READ_PROCESS_NAME
    BOOST_CHECK_EQUAL(thread_util::GetProcessName(), "bazzz");
#else
    // Special case for platforms which don't support reading of process name.
    BOOST_CHECK_EQUAL(thread_util::GetProcessName(), "");
#endif

    thread_util::SetProcessName("barrr");
#ifdef CAN_READ_PROCESS_NAME
    BOOST_CHECK_EQUAL(thread_util::GetProcessName(), "barrr");
#else
    // Special case for platforms which don't support reading of process name.
    BOOST_CHECK_EQUAL(thread_util::GetProcessName(), "");
#endif

    thread_util::SetProcessName(original_process_name.c_str());
    BOOST_CHECK_EQUAL(thread_util::GetProcessName(), original_process_name);
}

std::string TEST_THREAD_NAME_BASE = "test_thread.";

/**
 * Run a bunch of threads to all call thread_util::rename.
 *
 * @return the set of name each thread has after attempted renaming.
 */
std::set<std::string> RenameEnMasse(int num_threads)
{
    std::vector<std::thread> threads;
    std::set<std::string> names;
    std::mutex lock;

    auto RenameThisThread = [&](int i) {
        thread_util::Rename(TEST_THREAD_NAME_BASE + std::to_string(i));
        std::lock_guard<std::mutex> guard(lock);
        names.insert(thread_util::GetInternalName());
    };

    for (int i = 0; i < num_threads; ++i) {
        threads.push_back(std::thread(RenameThisThread, i));
    }

    for (std::thread& thread : threads) thread.join();

    return names;
}

/**
 * Rename a bunch of threads with the same basename (expect_multiple=true), ensuring suffixes are
 * applied properly.
 */
BOOST_AUTO_TEST_CASE(threadutil_test_rename_threaded)
{
    BOOST_CHECK_EQUAL(thread_util::GetInternalName(), thread_util::UNNAMED_THREAD);

    std::set<std::string> names = RenameEnMasse(100);

    BOOST_CHECK_EQUAL(names.size(), 100);

    // Names "test_thread.[n]" should exist for n = [0, 99]
    for (int i = 0; i < 100; ++i) {
        BOOST_CHECK(names.find(TEST_THREAD_NAME_BASE + std::to_string(i)) != names.end());
    }

}

BOOST_AUTO_TEST_SUITE_END()
