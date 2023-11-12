# Copyright (c) 2023-present The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/license/mit/.

include_guard(GLOBAL)

# Illumos/SmartOS requires linking with -lsocket if
# using getifaddrs & freeifaddrs.
# See: https://github.com/bitcoin/bitcoin/pull/21486
function(test_append_socket_library target)
  set(check_socket_source "
    #include <sys/types.h>
    #include <ifaddrs.h>

    int main() {
      struct ifaddrs* ifaddr;
      getifaddrs(&ifaddr);
      freeifaddrs(ifaddr);
    }
  ")

  include(CheckSourceCompilesAndLinks)
  check_cxx_source_links("${check_socket_source}" IFADDR_LINKS_WITHOUT_LIBSOCKET)
  if(IFADDR_LINKS_WITHOUT_LIBSOCKET)
    return()
  endif()

  check_cxx_source_links_with_libs(socket "${check_socket_source}" IFADDR_NEEDS_LINK_TO_LIBSOCKET)
  if(IFADDR_NEEDS_LINK_TO_LIBSOCKET)
    target_link_libraries(${target} INTERFACE socket)
  else()
    message(FATAL_ERROR "Cannot figure out how to use getifaddrs/freeifaddrs.")
  endif()
endfunction()

# Clang prior to version 15, when building for 32-bit,
# and linking against libstdc++, requires linking with
# -latomic if using the C++ atomic library.
# Can be tested with: clang++ test.cpp -m32
#
# Sourced from http://bugs.debian.org/797228
function(test_append_atomic_library target)
  set(check_atomic_source "
    #include <atomic>
    #include <cstdint>
    #include <chrono>

    using namespace std::chrono_literals;

    int main() {
      std::atomic<bool> lock{true};
      lock.exchange(false);

      std::atomic<std::chrono::seconds> t{0s};
      t.store(2s);

      std::atomic<int64_t> a{};

      int64_t v = 5;
      int64_t r = a.fetch_add(v);
      return static_cast<int>(r);
    }
  ")

  include(CheckSourceCompilesAndLinks)
  check_cxx_source_links("${check_atomic_source}" STD_ATOMIC_LINKS_WITHOUT_LIBATOMIC)
  if(STD_ATOMIC_LINKS_WITHOUT_LIBATOMIC)
    return()
  endif()

  check_cxx_source_links_with_libs(atomic "${check_atomic_source}" STD_ATOMIC_NEEDS_LINK_TO_LIBATOMIC)
  if(STD_ATOMIC_NEEDS_LINK_TO_LIBATOMIC)
    target_link_libraries(${target} INTERFACE atomic)
  else()
    message(FATAL_ERROR "Cannot figure out how to use std::atomic.")
  endif()
endfunction()
