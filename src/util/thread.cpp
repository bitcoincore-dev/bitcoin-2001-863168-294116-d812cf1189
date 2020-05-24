// Copyright (c) 2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <util/thread.h>

#include <logging.h>
#include <util/system.h>
#include <util/threadnames.h>

#include <exception>

#include <boost/thread/condition_variable.hpp>

void util::TraceThread(const char* thread_name, std::function<void()> thread_func)
{
    util::ThreadRename(thread_name);
    try {
        LogPrintf("%s thread start\n", thread_name);
        thread_func();
        LogPrintf("%s thread exit\n", thread_name);
    } catch (const boost::thread_interrupted&) {
        LogPrintf("%s thread interrupt\n", thread_name);
        throw;
    } catch (const std::exception& e) {
        PrintExceptionContinue(&e, thread_name);
        throw;
    } catch (...) {
        PrintExceptionContinue(nullptr, thread_name);
        throw;
    }
}
