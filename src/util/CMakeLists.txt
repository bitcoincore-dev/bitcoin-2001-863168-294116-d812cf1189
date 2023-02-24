# Copyright (c) 2023 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

# Check for posix_fallocate().
check_cxx_source_compiles("
  // same as in src/util/system.cpp
  #ifdef __linux__
  #ifdef _POSIX_C_SOURCE
  #undef _POSIX_C_SOURCE
  #endif
  #define _POSIX_C_SOURCE 200112L
  #endif // __linux__
  #include <fcntl.h>

  int main()
  {
    return posix_fallocate(0, 0, 0);
  }
  " HAVE_POSIX_FALLOCATE
)

# Check for different ways of gathering OS randomness:
# - Linux getrandom()
check_cxx_source_compiles("
  #include <unistd.h>
  #include <sys/syscall.h>
  #include <linux/random.h>

  int main()
  {
    syscall(SYS_getrandom, nullptr, 32, 0);
  }
  " HAVE_SYS_GETRANDOM
)

# - BSD getentropy()
check_cxx_symbol_exists(getentropy "unistd.h;sys/random.h" HAVE_GETENTROPY_RAND)

# - BSD sysctl()
check_cxx_source_compiles("
  #include <sys/types.h>
  #include <sys/sysctl.h>

  #ifdef __linux__
  #error Don't use sysctl on Linux, it's deprecated even when it works
  #endif

  int main()
  {
    sysctl(nullptr, 2, nullptr, nullptr, nullptr, 0);
  }
  " HAVE_SYSCTL
)

# - BSD sysctl(KERN_ARND)
check_cxx_source_compiles("
  #include <sys/types.h>
  #include <sys/sysctl.h>

  #ifdef __linux__
  #error Don't use sysctl on Linux, it's deprecated even when it works
  #endif

  int main()
  {
    static int name[2] = {CTL_KERN, KERN_ARND};
    sysctl(name, 2, nullptr, nullptr, nullptr, 0);
  }
  " HAVE_SYSCTL_ARND
)

add_library(bitcoin_util STATIC EXCLUDE_FROM_ALL
  asmap.cpp
  bip32.cpp
  bytevectorhash.cpp
  check.cpp
  error.cpp
  fees.cpp
  getuniquepath.cpp
  hasher.cpp
  message.cpp
  moneystr.cpp
  rbf.cpp
  readwritefile.cpp
  settings.cpp
  serfloat.cpp
  sock.cpp
  spanparsing.cpp
  strencodings.cpp
  string.cpp
  syscall_sandbox.cpp
  syserror.cpp
  system.cpp
  thread.cpp
  threadinterrupt.cpp
  threadnames.cpp
  time.cpp
  tokenpipe.cpp
  ../chainparamsbase.cpp
  ../clientversion.cpp
  ../fs.cpp
  ../logging.cpp
  ../random.cpp
  ../randomenv.cpp
  ../support/cleanse.cpp
  ../support/lockedpool.cpp
  ../sync.cpp
)

target_compile_definitions(bitcoin_util
  PRIVATE
    $<$<BOOL:${HAVE_POSIX_FALLOCATE}>:HAVE_POSIX_FALLOCATE=1>
    $<$<BOOL:${HAVE_SYS_GETRANDOM}>:HAVE_SYS_GETRANDOM=1>
    $<$<BOOL:${HAVE_SYSCTL}>:HAVE_SYSCTL=1>
    $<$<BOOL:${HAVE_SYSCTL_ARND}>:HAVE_SYSCTL_ARND=1>
    $<$<CXX_COMPILER_ID:MSVC>:_SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING>
)

target_link_libraries(bitcoin_util
  PRIVATE
    bitcoin_crypto
    univalue
    Threads::Threads
    $<TARGET_NAME_IF_EXISTS:std_filesystem>
)
