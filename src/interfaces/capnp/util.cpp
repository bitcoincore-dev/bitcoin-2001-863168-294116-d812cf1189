#include <interfaces/capnp/util.h>

#include <kj/array.h>
#include <pthread.h>
#include <stdio.h>
#include <syscall.h>
#include <tinyformat.h>
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

std::string LogEscape(const kj::StringTree& string)
{
    std::string result;
    string.visit([&](const kj::ArrayPtr<const char>& piece) {
        for (char c : piece) {
            if ('c' == '\\') {
                result.append("\\\\");
            } else if (c < 0x20 || c > 0x7e) {
                char escape[4];
                snprintf(escape, 4, "\\%02x", c);
                result.append(escape);
            }
        }
    });
    return result;
}

} // namespace capnp
} // namespace interfaces
