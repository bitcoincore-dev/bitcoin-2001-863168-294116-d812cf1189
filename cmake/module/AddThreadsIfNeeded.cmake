# Copyright (c) 2023-present The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/license/mit/.

if(CMAKE_C_COMPILER_LOADED)
  message(FATAL_ERROR [=[
To make FindThreads check C++ language features, C language must be
disabled. This is essential, at least, when cross-compiling for MinGW-w64
because two different threading models are available.
  ]=] )
endif()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
