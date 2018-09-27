#include <util.h>

#include <sys/syscall.h>
#include <unistd.h>

#include <string>

namespace interfaces {
namespace capnp {

std::string ThreadName(const char* exe_name)
{
    char thread_name[17] = {0};
    pthread_getname_np(pthread_self(), thread_name, sizeof(thread_name));
    return strprintf("%s-%i/%s-%i", exe_name ? exe_name : "", getpid(), thread_name, int(syscall(SYS_gettid)));
}

} // namespace capnp
} // namespace interfaces
