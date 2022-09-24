# Copyright (c) 2022 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

find_program(PROG_HEXDUMP hexdump REQUIRED)
find_program(PROG_SED sed REQUIRED)
mark_as_advanced(PROG_HEXDUMP PROG_SED)

set(json2header_path ${CMAKE_BINARY_DIR}/json2header.sh)
if(NOT EXISTS ${json2header_path})
  configure_file(${CMAKE_SOURCE_DIR}/cmake/command/json2header.sh.in ${json2header_path} @ONLY)
endif()
function(generate_header_from_json json_source)
  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${json_source}.h
    COMMAND ${json2header_path} ${CMAKE_CURRENT_SOURCE_DIR}/${json_source} ${CMAKE_CURRENT_BINARY_DIR}/${json_source}.h
    DEPENDS ${json_source}
  )
endfunction()

set(raw2header_path ${CMAKE_BINARY_DIR}/raw2header.sh)
if(NOT EXISTS ${raw2header_path})
  configure_file(${CMAKE_SOURCE_DIR}/cmake/command/raw2header.sh.in ${raw2header_path} @ONLY)
endif()
function(generate_header_from_raw raw_source)
  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${raw_source}.h
    COMMAND ${raw2header_path} ${CMAKE_CURRENT_SOURCE_DIR}/${raw_source} ${CMAKE_CURRENT_BINARY_DIR}/${raw_source}.h
    DEPENDS ${raw_source}
  )
endfunction()
