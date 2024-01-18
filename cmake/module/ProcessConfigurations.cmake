# Copyright (c) 2023-present The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/license/mit/.

function(get_all_configs output)
  get_property(is_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
  if(is_multi_config)
    set(all_configs ${CMAKE_CONFIGURATION_TYPES})
  else()
    get_property(all_configs CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS)
    if(NOT all_configs)
      # See https://cmake.org/cmake/help/latest/manual/cmake-buildsystem.7.html#default-and-custom-configurations
      set(all_configs Debug Release RelWithDebInfo MinSizeRel)
    endif()
  endif()
  set(${output} "${all_configs}" PARENT_SCOPE)
endfunction()


#[=[
Set the default build configuration.

See: https://cmake.org/cmake/help/latest/manual/cmake-buildsystem.7.html#build-configurations.

On single-configuration generators, this function sets the CMAKE_BUILD_TYPE variable to
the default build configuration, which can be overridden by the user at configure time if needed.

On multi-configuration generators, this function rearranges the CMAKE_CONFIGURATION_TYPES list,
ensuring that the default build configuration appears first while maintaining the order of the
remaining configurations. The user can choose a build configuration at build time.
]=]
function(set_default_config config)
  get_all_configs(all_configs)
  if(NOT ${config} IN_LIST all_configs)
    message(FATAL_ERROR "The default config is \"${config}\", but must be one of ${all_configs}.")
  endif()

  list(REMOVE_ITEM all_configs ${config})
  list(PREPEND all_configs ${config})

  get_property(is_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
  if(is_multi_config)
    get_property(help_string CACHE CMAKE_CONFIGURATION_TYPES PROPERTY HELPSTRING)
    set(CMAKE_CONFIGURATION_TYPES "${all_configs}" CACHE STRING "${help_string}" FORCE)
  else()
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY
      STRINGS "${all_configs}"
    )
    if(NOT CMAKE_BUILD_TYPE)
      message(STATUS "Setting build type to \"${config}\" as none was specified")
      get_property(help_string CACHE CMAKE_BUILD_TYPE PROPERTY HELPSTRING)
      set(CMAKE_BUILD_TYPE "${config}" CACHE STRING "${help_string}" FORCE)
    endif()
  endif()
endfunction()

function(remove_c_flag_from_all_configs flag)
  get_all_configs(all_configs)
  foreach(config IN LISTS all_configs)
    string(TOUPPER "${config}" config_uppercase)
    set(flags "${CMAKE_C_FLAGS_${config_uppercase}}")
    separate_arguments(flags)
    list(FILTER flags EXCLUDE REGEX "${flag}")
    list(JOIN flags " " new_flags)
    set(CMAKE_C_FLAGS_${config_uppercase} "${new_flags}" PARENT_SCOPE)
  endforeach()
endfunction()

function(remove_cxx_flag_from_all_configs flag)
  get_all_configs(all_configs)
  foreach(config IN LISTS all_configs)
    string(TOUPPER "${config}" config_uppercase)
    set(flags "${CMAKE_CXX_FLAGS_${config_uppercase}}")
    separate_arguments(flags)
    list(FILTER flags EXCLUDE REGEX "${flag}")
    list(JOIN flags " " new_flags)
    set(CMAKE_CXX_FLAGS_${config_uppercase} "${new_flags}" PARENT_SCOPE)
  endforeach()
endfunction()

function(replace_c_flag_in_config config old_flag new_flag)
  string(TOUPPER "${config}" config_uppercase)
  string(REGEX REPLACE "(^| )${old_flag}( |$)" "\\1${new_flag}\\2" new_flags "${CMAKE_C_FLAGS_${config_uppercase}}")
  set(CMAKE_C_FLAGS_${config_uppercase} "${new_flags}" PARENT_SCOPE)
endfunction()

function(replace_cxx_flag_in_config config old_flag new_flag)
  string(TOUPPER "${config}" config_uppercase)
  string(REGEX REPLACE "(^| )${old_flag}( |$)" "\\1${new_flag}\\2" new_flags "${CMAKE_CXX_FLAGS_${config_uppercase}}")
  set(CMAKE_CXX_FLAGS_${config_uppercase} "${new_flags}" PARENT_SCOPE)
endfunction()

function(separate_by_configs options)
  list(JOIN ${options} " " ${options}_ALL)
  string(GENEX_STRIP "${${options}_ALL}" ${options}_ALL)
  string(STRIP "${${options}_ALL}" ${options}_ALL)
  set(${options}_ALL "${${options}_ALL}" PARENT_SCOPE)

  get_all_configs(all_configs)
  foreach(config IN LISTS all_configs)
    string(REGEX MATCHALL "\\$<\\$<CONFIG:${config}>:[^<>\n]*>" match "${${options}}")
    list(JOIN match " " match)
    string(REPLACE "\$<\$<CONFIG:${config}>:" "" match "${match}")
    string(REPLACE ">" "" match "${match}")
    string(TOUPPER "${config}" conf_upper)
    set(${options}_${conf_upper} "${match}" PARENT_SCOPE)
  endforeach()
endfunction()

function(print_config_flags)
  macro(print_flags config)
    string(TOUPPER "${config}" config_uppercase)
    message(" - Preprocessor defined macros ........ ${definitions_${config_uppercase}}")
    message(" - CFLAGS ............................. ${CMAKE_C_FLAGS_${config_uppercase}}")
    message(" - CXXFLAGS ........................... ${CMAKE_CXX_FLAGS_${config_uppercase}}")
    message(" - LDFLAGS for executables ............ ${CMAKE_EXE_LINKER_FLAGS_${config_uppercase}}")
    message(" - LDFLAGS for shared libraries ....... ${CMAKE_SHARED_LINKER_FLAGS_${config_uppercase}}")
  endmacro()

  get_property(is_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
  if(is_multi_config)
    list(JOIN CMAKE_CONFIGURATION_TYPES " " configs)
    message("Available build types (configurations)  ${configs}")
    foreach(config IN LISTS CMAKE_CONFIGURATION_TYPES)
      message("'${config}' build type (configuration):")
      print_flags(${config})
    endforeach()
  else()
    message("Build type (configuration):")
    message(" - CMAKE_BUILD_TYPE ................... ${CMAKE_BUILD_TYPE}")
    print_flags(${CMAKE_BUILD_TYPE})
  endif()
endfunction()