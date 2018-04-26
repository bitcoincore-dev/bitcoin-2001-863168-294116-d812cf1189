// Copyright (c) 2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <threadnames.h>
#include <tinyformat.h>

#include <boost/thread.hpp>

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


extern int LogPrintStr(const std::string &str);

std::unique_ptr<ThreadNameRegistry> g_thread_names(new ThreadNameRegistry());

namespace {

/**
 * @return true if `to_check` is prefixed with `prefix`.
 */
bool HasPrefix(const std::string& to_check, const std::string& prefix) {
    return to_check.compare(0, prefix.size(), prefix) == 0;
}

/**
 * Count the number of entries in a map with a given prefix.
 *
 * @return The number of occurrences of keys with the given prefix.
 */
template <class T>
int CountMapPrefixes(T map, const std::string& prefix) {
    auto it = map.lower_bound(prefix);
    int count = 0;

    while (it != map.end() && HasPrefix(it->first, prefix)) {
        ++it; ++count;
    }

    return count;
}

} // namespace

bool ThreadNameRegistry::Rename(std::string name, bool expect_multiple)
{
    std::lock_guard<std::mutex> guard(m_map_lock);

    if (expect_multiple) {
        name = tfm::format("%s.%d", name, CountMapPrefixes(m_name_to_id, name));
    }

    std::string process_name(name);

    /*
     * Uncomment if we want to retain the `bitcoin-` system thread name prefix.
     *
    const std::string process_prefix("bitcoin-");
    if (!HasPrefix(process_name, process_prefix)) {
        process_name = process_prefix + process_name;
    }
    */

    RenameProcess(process_name.c_str());

    auto thread_id = boost::this_thread::get_id();
    auto it_name = m_name_to_id.find(name);
    auto it_id = m_id_to_name.find(thread_id);

    // Don't allow name collisions.
    if (it_name != m_name_to_id.end() && it_name->second != thread_id) {
        std::stringstream errmsg; // stringstream use necessary to get thread_id into a string.
        errmsg << "Thread name '" << name << "' already registered (id: " << it_name->second << ")\n";
        LogPrintStr(errmsg.str());
        return false;
    }
    // Warn on reregistration.
    else if (it_id != m_id_to_name.end()) {
        std::stringstream warnmsg;
        warnmsg << "Reregistering thread " << thread_id << " with name '%s'; previously was '%s'\n";
        LogPrintStr(tfm::format(warnmsg.str().c_str(), name, it_id->second));
    }

    m_id_to_name[thread_id] = name;
    m_name_to_id[name] = thread_id;
    return true;
}

std::string ThreadNameRegistry::GetName()
{
    std::lock_guard<std::mutex> guard(m_map_lock);

    auto thread_id = boost::this_thread::get_id();
    auto found = m_id_to_name.find(thread_id);

    if (found != m_id_to_name.end()) {
        return found->second;
    }

    std::string pname = GetProcessName();
    m_name_to_id[pname] = thread_id;
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
#elif (defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__DragonFly__))
    pthread_get_name_np(pthread_self(), pthreadname_buff);
#elif defined(MAC_OSX)
    pthread_getname_np(pthread_self(), pthreadname_buff, sizeof(threadname_buff));
#else
    return "<unknown>";
#endif
    return std::string(pthreadname_buff);
}
