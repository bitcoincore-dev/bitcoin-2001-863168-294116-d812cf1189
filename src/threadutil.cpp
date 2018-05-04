// Copyright (c) 2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <threadutil.h>

/*  TODO: using thread_local changes the abi in ways that may not play nice
    when the c++ stdlib is linked dynamically. Disable it until thorough
    testing has been done. */
#undef HAVE_THREAD_LOCAL

#ifdef HAVE_SYS_PRCTL_H
#include <sys/prctl.h> // For prctl, PR_SET_NAME, PR_GET_NAME
#endif

#if (defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__DragonFly__))
#include <pthread_np.h>
#endif

#if defined(HAVE_THREAD_LOCAL)
#include <atomic>
#include <thread>

#elif defined(HAVE_PTHREAD)
#include <atomic>
#include <pthread.h>

#else
#include <mutex>
#include <thread>
#include <unordered_map>
#endif

const std::string UNNAMED_THREAD = "<unnamed>";

struct thread_data_type
{
    constexpr thread_data_type() = default;
    thread_data_type(long id, std::string name) : m_id(id), m_name(std::move(name)){}

    long m_id{0};
    std::string m_name{""};
};

void thread_util::set_process_name(const char* name)
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

std::string thread_util::get_process_name()
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

static std::string try_set_internal_name_from_process()
{
    std::string procname = thread_util::get_process_name();
    if (procname.size()) thread_util::set_internal_name(procname);
    return procname;
}


bool thread_util::rename(std::string name)
{
    set_process_name(name.c_str());
    set_internal_name(name);
}


/*
 * What follows are three separate platform-dependent implementations of
 * *name and *id parts of the thread_utils interface.
 *
 * If we have thread_local, just keep thread ID and name in a thread_local
 * global.
 */
#if defined(HAVE_THREAD_LOCAL)

static thread_local thread_data_type g_thread_data;
std::string thread_util::get_internal_name()
{
    auto name = g_thread_data.m_name;

    if (g_thread_data.m_name.empty()) {
        try_set_internal_name_from_process();
    }
    return g_thread_data.m_name.size() ? g_thread_data.m_name : UNNAMED_THREAD;
}

long thread_util::get_internal_id()
{
    return g_thread_data.m_id;
}

bool thread_util::set_internal_name(std::string name)
{
    static std::atomic<long> internal_id{0};
    g_thread_data = {internal_id++, std::move(name)};
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

std::string thread_util::get_internal_name()
{
    void* data = pthread_getspecific(g_key);
    if (data) {
        return static_cast<thread_data_type*>(data)->m_name;
    }
    std::string fromproc = try_set_internal_name_from_process();
    return fromproc.size() ? fromproc : UNNAMED_THREAD;
}

long thread_util::get_internal_id()
{
    void* data = pthread_getspecific(g_key);
    if (data) {
        return static_cast<thread_data_type*>(data)->m_id;
    }
    return -1;
}

bool thread_util::set_internal_name(std::string name)
{
    static std::atomic<long> internal_id{0};
    static pthread_once_t key_once = PTHREAD_ONCE_INIT;
    if (pthread_once(&key_once, make_key)) {
        return false;
    }
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

static inline thread_data_type get_thread_data()
{
    thread_data_type ret{-1, ""};
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

bool thread_util::set_internal_name(std::string name)
{
    static long internal_id{0};
    std::string name_copy(name);
    std::thread::id thread_id(std::this_thread::get_id());
    {
        std::lock_guard<std::mutex> lock(m_map_mutex);
        auto it = m_thread_map.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(std::move(thread_id)),
            std::forward_as_tuple(internal_id++, std::move(name)));
        if (!it.second) {
            it.first->second.m_name = std::move(name_copy);
        }
    }
    return true;
}

long thread_util::get_internal_id()
{
    return get_thread_data().m_id;
}

std::string thread_util::get_internal_name()
{
    auto data = get_thread_data();

    if (!data.m_name.empty()) {
        return data.m_name;
    }
    std::string fromproc = try_set_internal_name_from_process();
    return fromproc.size() ? fromproc : UNNAMED_THREAD;
}

#endif
