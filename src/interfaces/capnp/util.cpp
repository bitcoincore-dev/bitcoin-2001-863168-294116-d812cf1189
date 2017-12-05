#include <util.h>

#include <prctl.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <string>

namespace interfaces {
namespace capnp {

std::string ThreadName(const char* exe_name)
{
    char thread_name[17] = {0};
    prctl(PR_GET_NAME, thread_name, 0L, 0L, 0L);
    return strprintf("%s-%i/%s-%i", exe_name ? exe_name : "", getpid(), thread_name, int(syscall(SYS_gettid)));
}

} // namespace capnp
} // namespace interfaces
