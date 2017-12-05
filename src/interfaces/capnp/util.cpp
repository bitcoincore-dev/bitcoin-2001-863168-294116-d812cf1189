#include <util.h>

#include <pthread.h>
#include <string>
#include <sys/syscall.h>
#include <unistd.h>

namespace interfaces {
namespace capnp {

std::string ThreadName(const char* exe_name)
{
    char thread_name[17] = {0};
    pthread_getname_np(pthread_self(), thread_name, sizeof(thread_name));
    uint64_t tid = 0;
#if __linux__
    tid = syscall(SYS_gettid);
#else
    pthread_threadid_np(NULL, &tid);
#endif
    return strprintf("%s-%i/%s-%i", exe_name ? exe_name : "", getpid(), thread_name, tid);
}

} // namespace capnp
} // namespace interfaces
