# Copyright (c) 2024-present The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/license/mit/.

include(${CMAKE_CURRENT_LIST_DIR}/CoverageInclude.cmake)

if(DEFINED JOBS)
  list(APPEND CMAKE_CTEST_COMMAND -j ${JOBS})
endif()

execute_process(
  COMMAND ${CMAKE_CTEST_COMMAND} --build-config Coverage
  WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
)
execute_process(
  COMMAND ${LCOV_COMMAND} --capture --directory src --test-name test_bitcoin --output-file test_bitcoin.info
  WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
)
execute_process(
  COMMAND ${LCOV_COMMAND} --zerocounters --directory src
  WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
)
execute_process(
  COMMAND ${LCOV_FILTER_COMMAND} test_bitcoin.info test_bitcoin_filtered.info
  WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
)
execute_process(
  COMMAND ${LCOV_COMMAND} --add-tracefile test_bitcoin_filtered.info --output-file test_bitcoin_filtered.info
  WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
)
execute_process(
  COMMAND ${LCOV_COMMAND} --add-tracefile baseline_filtered.info --add-tracefile test_bitcoin_filtered.info --output-file test_bitcoin_coverage.info
  WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
)
execute_process(
  COMMAND ${GENHTML_COMMAND} test_bitcoin_coverage.info --output-directory test_bitcoin.coverage
  WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
)
