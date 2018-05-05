// Copyright (c) 2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <atomic>
#include <thread>

#include <threadutil.h>

/*
 * TODO: using thread_local changes the abi in ways that may not play nice
 * when the c++ stdlib is linked dynamically. Disable it until thorough
 * testing has been done.
 *
 * mingw32's implementation of thread_local has also been shown to behave
 * erroneously under concurrent usage; see:
 *
 *   https://gist.github.com/jamesob/fe9a872051a88b2025b1aa37bfa98605
 */
#undef HAVE_THREAD_LOCAL

#ifdef HAVE_SYS_PRCTL_H
#include <sys/prctl.h> // For prctl, PR_SET_NAME, PR_GET_NAME
#endif

#if (defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__DragonFly__))
#include <pthread_np.h>
#endif

#if defined(HAVE_THREAD_LOCAL)

#elif defined(HAVE_PTHREAD)
#include <pthread.h>

#else
#include <mutex>
#include <unordered_map>
#endif

struct thread_data_type
{
    constexpr thread_data_type() = default;
    thread_data_type(long id, std::string name) : m_id(id), m_name(std::move(name)){}

    long m_id{0};
    std::string m_name{""};
};

void thread_util::SetProcessName(const char* name)
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

std::string thread_util::GetProcessName()
{
#if !defined(CAN_READ_PROCESS_NAME)
    return "";
#endif

    char threadname_buff[16];
    char* pthreadname_buff = (char*)(&threadname_buff);

#if defined(PR_GET_NAME)
    ::prctl(PR_GET_NAME, pthreadname_buff);
#elif defined(MAC_OSX)
    pthread_getname_np(pthread_self(), pthreadname_buff, sizeof(threadname_buff));
#endif
    return std::string(pthreadname_buff);
}

void thread_util::Rename(std::string name)
{
    SetProcessName(name.c_str());
    SetInternalName(std::move(name));
}

/*
 * What follows are three separate platform-dependent implementations of
 * *name and *id parts of the thread_utils interface. Each implementation
 * emulates thread_local storage.
 *
 * If we have thread_local, just keep thread ID and name in a thread_local
 * global.
 */
#if defined(HAVE_THREAD_LOCAL)

static thread_local thread_data_type g_thread_data;
std::string thread_util::GetInternalName()
{
    return g_thread_data.m_name;
}

bool thread_util::SetInternalName(std::string name)
{
    static std::atomic<long> id_accumulator{0};
    static thread_local thread_id{id_accumulator++};
    g_thread_data = {thread_id, std::move(name)};
    return true;
}

/*
 * Otherwise if we don't have use of thread_local, use the pthreads interface
 * to stash thread data in thread-specific key using getspecific/setspecific.
 */
#elif defined(HAVE_PTHREAD)

static pthread_key_t g_key;
static void destruct_data(void* data)
{
    delete static_cast<thread_data_type*>(data);
}

static void make_key()
{
    pthread_key_create(&g_key, destruct_data);
}

static bool EnsureKeyCreated()
{
    static pthread_once_t key_once = PTHREAD_ONCE_INIT;
    return !pthread_once(&key_once, make_key);
}

std::string thread_util::GetInternalName()
{
    if (EnsureKeyCreated()) {
        void* data = pthread_getspecific(g_key);
        if (data) {
            return static_cast<thread_data_type*>(data)->m_name;
        }
    }
    return "<unnamed>";
}

bool thread_util::SetInternalName(std::string name)
{
    static std::atomic<long> internal_id{0};
    if (!EnsureKeyCreated()) return false;

    void* data = pthread_getspecific(g_key);
    if (data) {
        static_cast<thread_data_type*>(data)->m_name = std::move(name);
        return true;
    }
    data = new thread_data_type{internal_id++, std::move(name)};
    return pthread_setspecific(g_key, data) == 0;
}

/*
 * As a final fallback, maintain thread data in a shared map guarded by a
 * mutex.
 */
#else

static std::unordered_map<std::thread::id, thread_data_type> m_thread_map;
static std::mutex m_map_mutex;

static inline thread_data_type GetThreadData()
{
    thread_data_type ret{-1, "<unnamed>"};
    std::thread::id thread_id(std::this_thread::get_id());
    {
        std::lock_guard<std::mutex> lock(m_map_mutex);
        auto it = m_thread_map.find(thread_id);
        if(it != m_thread_map.end()) {
            ret = it->second;
        }
    }
    return ret;
}

bool thread_util::SetInternalName(std::string name)
{
    static long internal_id{0};
    std::string name_copy(name);
    std::thread::id thread_id(std::this_thread::get_id());
    {
        std::lock_guard<std::mutex> lock(m_map_mutex);
        auto it_found = m_thread_map.find(thread_id);

        if (it_found != m_thread_map.end()) {
            // Data already exists in map; name has already been set.
            it_found->second.m_name = std::move(name_copy);
        } else {
            // Insert a new data entry into the map.
            m_thread_map.insert(std::pair(
                std::move(thread_id),
                thread_data_type{internal_id++, std::move(name)});
        }
    }
    return true;
}

std::string thread_util::GetInternalName()
{
    return GetThreadData().m_name;
}

#endif
