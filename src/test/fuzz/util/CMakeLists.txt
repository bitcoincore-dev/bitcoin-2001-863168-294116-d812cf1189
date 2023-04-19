# Copyright (c) 2023-present The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/license/mit/.

add_library(test_fuzz STATIC EXCLUDE_FROM_ALL
  mempool.cpp
  net.cpp
  ../fuzz.cpp
  ../util.cpp
)

target_link_libraries(test_fuzz
  PRIVATE
    core
    test_util
    bitcoin_node
    Boost::headers
)

if(NOT FUZZ_OSS)
  include(CheckSourceCompilesAndLinks)
  check_cxx_source_links_with_flags("-fsanitize=${SANITIZERS}" "
      #include <cstdint>
      #include <cstddef>
      extern \"C\" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) { return 0; }
      // No main() function.
    " BINARY_LINKS_WITHOUT_MAIN_FUNCTION
  )
  if(NOT BINARY_LINKS_WITHOUT_MAIN_FUNCTION)
    target_compile_definitions(test_fuzz PRIVATE PROVIDE_FUZZ_MAIN_FUNCTION)
  endif()
endif()
