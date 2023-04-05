# Copyright (c) 2023 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

# On macOS host, sets the following variables:
#  - ${package}_ROOT
#  - ${package}_path_include
#  - ${package}_path_lib
function(get_brew_package_details package)
  if(CMAKE_HOST_APPLE AND BREW_COMMAND)
    execute_process(
      COMMAND ${BREW_COMMAND} --prefix ${package}
      OUTPUT_VARIABLE prefix
      ERROR_QUIET
      OUTPUT_STRIP_TRAILING_WHITESPACE
    )
  endif()

  if(prefix)
    set(${package}_ROOT ${prefix} PARENT_SCOPE)
    set(${package}_path_include ${prefix}/include PARENT_SCOPE)
    set(${package}_path_lib ${prefix}/lib PARENT_SCOPE)
  endif()
endfunction()
