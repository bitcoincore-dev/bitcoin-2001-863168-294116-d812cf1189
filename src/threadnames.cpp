// Copyright (c) 2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <threadnames.h>
#include <tinyformat.h>

#include <atomic>
#include <cstdint>
#include <string>

#if !defined(HAVE_THREAD_LOCAL)
#include <boost/thread.hpp>
#endif

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h> // For HAVE_SYS_PRCTL_H
#endif

#if (defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__DragonFly__))
#include <pthread.h>
#include <pthread_np.h>
#endif

#ifdef HAVE_SYS_PRCTL_H
#include <sys/prctl.h> // For prctl, PR_SET_NAME
#endif

std::unique_ptr<ThreadNameRegistry> g_thread_names(new ThreadNameRegistry());

std::string ThreadNameRegistry::GetId()
{
#if !defined(HAVE_THREAD_LOCAL)
    // Fall back to boost when we don't have thread_local available.
    //
    // See: https://github.com/bitcoin/bitcoin/pull/11722#pullrequestreview-79322658
    //
    std::stringstream temp; // stringstream use necessary to get thread_id into a string.
    temp << boost::this_thread::get_id();
    return temp.str();
#else
    // Otherwise, an ID is computed once and cached per thread.
    //
    static std::atomic<std::uint32_t> uid{0};
    static thread_local std::string this_id = std::to_string(uid++);
    return this_id;
#endif
}

bool ThreadNameRegistry::Rename(std::string name, bool expect_multiple)
{
    std::lock_guard<std::mutex> guard(m_map_lock);
    std::string name_without_suffix = name;

    if (expect_multiple) {
        name = tfm::format("%s.%d", name_without_suffix, m_name_to_count[name_without_suffix]);
    }

    std::string thread_id = GetId();
    std::string process_name = name;

    auto it_name = m_name_to_count.find(name);
    auto it_id = m_id_to_name.find(thread_id);

    // Don't allow name collisions.
    if (it_name != m_name_to_count.end()) {
        LogPrintStr(tfm::format("Thread name '%s' already registered", name));
        return false;
    }
    // Warn on reregistration.
    else if (it_id != m_id_to_name.end()) {
        LogPrintStr(tfm::format(
            "Reregistering thread %s with name '%s'; previously was '%s'\n",
            thread_id, name, it_id->second));
    }

    RenameProcess(process_name.c_str());
    m_id_to_name[thread_id] = name;
    m_name_to_count[name_without_suffix]++;

    return true;
}

std::string ThreadNameRegistry::GetName()
{
    std::lock_guard<std::mutex> guard(m_map_lock);

    auto thread_id = GetId();
    auto found = m_id_to_name.find(thread_id);

    if (found != m_id_to_name.end()) {
        return found->second;
    }

    std::string pname = GetProcessName();
    ++m_name_to_count[pname];
    m_id_to_name[thread_id] = pname;
    return pname;
}

void ThreadNameRegistry::RenameProcess(const char* name)
{
#if defined(PR_SET_NAME)
    // Only the first 15 characters are used (16 - NUL terminator)
    ::prctl(PR_SET_NAME, name, 0, 0, 0);
#elif (defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__DragonFly__))
    pthread_set_name_np(pthread_self(), name);
#elif defined(MAC_OSX)
    pthread_setname_np(name);
#else
    // Prevent warnings for unused parameters...
    (void)name;
#endif
}

std::string ThreadNameRegistry::GetProcessName()
{
    char threadname_buff[16];
    char* pthreadname_buff = (char*)(&threadname_buff);

#if defined(PR_GET_NAME)
    ::prctl(PR_GET_NAME, pthreadname_buff);
#elif defined(MAC_OSX)
    pthread_getname_np(pthread_self(), pthreadname_buff, sizeof(threadname_buff));
#else
    return "<unknown>";
#endif
    return std::string(pthreadname_buff);
}
