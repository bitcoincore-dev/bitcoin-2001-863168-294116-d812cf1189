// Copyright (c) 2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <atomic>
#include <thread>

#include <threadutil.h>

#ifdef HAVE_SYS_PRCTL_H
#include <sys/prctl.h> // For prctl, PR_SET_NAME, PR_GET_NAME
#endif

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

#if defined(PR_GET_NAME)
    ::prctl(PR_GET_NAME, threadname_buff);
#elif defined(MAC_OSX)
    pthread_getname_np(pthread_self(), threadname_buff, sizeof(threadname_buff));
#endif
    return std::string(threadname_buff);
}

void thread_util::Rename(std::string name)
{
    SetProcessName(name.c_str());
    SetInternalName(std::move(name));
}

/*
 * If we have thread_local, just keep thread ID and name in a thread_local
 * global.
 */
#if defined(HAVE_THREAD_LOCAL)

struct thread_data_type
{
    constexpr thread_data_type() = default;
    thread_data_type(long id, std::string name) : m_id(id), m_name(std::move(name)){}

    long m_id{0};
    std::string m_name{""};
};

static thread_local thread_data_type g_thread_data;
std::string thread_util::GetInternalName()
{
    return g_thread_data.m_name;
}

bool thread_util::SetInternalName(std::string name)
{
    static std::atomic<long> id_accumulator{0};
    static thread_local long thread_id{id_accumulator++};
    g_thread_data = {thread_id, std::move(name)};
    return true;
}

/**
 * Without thread_local available, don't handle internal name at all.
 */
#else

std::string thread_util::GetInternalName()
{
    return "";
}

bool thread_util::SetInternalName(std::string name)
{
    return false;
}

#endif
