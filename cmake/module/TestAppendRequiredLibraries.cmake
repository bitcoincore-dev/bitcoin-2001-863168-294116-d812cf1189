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
