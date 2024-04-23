# Copyright (c) 2024-present The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/license/mit/.

find_package(Doxygen COMPONENTS dot)

if(DOXYGEN_FOUND)
  set(doxyfile ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
  configure_file(Doxyfile.in ${doxyfile})

  # In CMake 3.27, The FindDoxygen module's doxygen_add_docs()
  # command gained a CONFIG_FILE option to specify a custom doxygen
  # configuration file.
  # TODO: Consider using it.
  set(stampfile ${CMAKE_CURRENT_BINARY_DIR}/.stamp)
  add_custom_command(
    OUTPUT ${stampfile}
    COMMAND Doxygen::doxygen ${doxyfile}
    COMMAND ${CMAKE_COMMAND} -E touch ${stampfile}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    DEPENDS ${doxyfile}
    COMMENT "Generating developer documentation"
    VERBATIM
  )
  add_custom_target(docs
    DEPENDS ${stampfile}
  )
else()
  add_custom_target(docs
    COMMAND ${CMAKE_COMMAND} -E echo "error: doxygen not found"
  )
endif()
