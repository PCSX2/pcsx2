# PNGGenConfig.cmake
# Utility functions for configuring and building libpng

# Copyright (c) 2018-2025 Cosmin Truta
# Copyright (c) 2016-2018 Glenn Randers-Pehrson
# Written by Roger Leigh, 2016
#
# Use, modification and distribution are subject to
# the same licensing terms and conditions as libpng.
# Please see the copyright notice in png.h or visit
# http://libpng.org/pub/png/src/libpng-LICENSE.txt
#
# SPDX-License-Identifier: libpng-2.0

# Generate .chk from .out with awk, based upon the automake logic:
# generate_chk(INPUT <file> OUTPUT <file> [DEPENDS <deps>...])
function(generate_chk)
  set(options)
  set(oneValueArgs INPUT OUTPUT)
  set(multiValueArgs DEPENDS)
  cmake_parse_arguments(_GENCHK "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
  if(NOT _GENCHK_INPUT)
    message(FATAL_ERROR "generate_chk: Missing INPUT argument")
  endif()
  if(NOT _GENCHK_OUTPUT)
    message(FATAL_ERROR "generate_chk: Missing OUTPUT argument")
  endif()

  # Run genchk.cmake to generate the .chk file.
  add_custom_command(OUTPUT "${_GENCHK_OUTPUT}"
                     COMMAND "${CMAKE_COMMAND}"
                             "-DINPUT=${_GENCHK_INPUT}"
                             "-DOUTPUT=${_GENCHK_OUTPUT}"
                             -P "${CMAKE_CURRENT_BINARY_DIR}/scripts/cmake/genchk.cmake"
                     DEPENDS "${_GENCHK_INPUT}" ${_GENCHK_DEPENDS}
                     WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
endfunction()

# Generate .out from C source file with awk:
# generate_out(INPUT <file> OUTPUT <file> [DEPENDS <deps>...])
function(generate_out)
  set(options)
  set(oneValueArgs INPUT OUTPUT)
  set(multiValueArgs DEPENDS)
  cmake_parse_arguments(_GENOUT "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
  if(NOT _GENOUT_INPUT)
    message(FATAL_ERROR "generate_out: Missing INPUT argument")
  endif()
  if(NOT _GENOUT_OUTPUT)
    message(FATAL_ERROR "generate_out: Missing OUTPUT argument")
  endif()

  # Run genout.cmake to generate the .out file.
  add_custom_command(OUTPUT "${_GENOUT_OUTPUT}"
                     COMMAND "${CMAKE_COMMAND}"
                             "-DINPUT=${_GENOUT_INPUT}"
                             "-DOUTPUT=${_GENOUT_OUTPUT}"
                             -P "${CMAKE_CURRENT_BINARY_DIR}/scripts/cmake/genout.cmake"
                     DEPENDS "${_GENOUT_INPUT}" ${_GENOUT_DEPENDS}
                     WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
endfunction()

# Generate a source file with awk:
# generate_source(OUTPUT <file> [DEPENDS <deps>...])
function(generate_source)
  set(options)
  set(oneValueArgs OUTPUT)
  set(multiValueArgs DEPENDS)
  cmake_parse_arguments(_GENSRC "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
  if(NOT _GENSRC_OUTPUT)
    message(FATAL_ERROR "generate_source: Missing OUTPUT argument")
  endif()

  # Run gensrc.cmake to generate the source file.
  add_custom_command(OUTPUT "${_GENSRC_OUTPUT}"
                     COMMAND "${CMAKE_COMMAND}"
                             "-DOUTPUT=${_GENSRC_OUTPUT}"
                             -P "${CMAKE_CURRENT_BINARY_DIR}/scripts/cmake/gensrc.cmake"
                     DEPENDS ${_GENSRC_DEPENDS}
                     WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
endfunction()

# Generate an identical file copy:
# generate_copy(INPUT <file> OUTPUT <file> [DEPENDS <deps>...])
function(generate_copy)
  set(options)
  set(oneValueArgs INPUT OUTPUT)
  set(multiValueArgs DEPENDS)
  cmake_parse_arguments(_GENCPY "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
  if(NOT _GENCPY_INPUT)
    message(FATAL_ERROR "generate_copy: Missing INPUT argument")
  endif()
  if(NOT _GENCPY_OUTPUT)
    message(FATAL_ERROR "generate_copy: Missing OUTPUT argument")
  endif()

  # Make a forced file copy, overwriting any pre-existing output file.
  add_custom_command(OUTPUT "${_GENCPY_OUTPUT}"
                     COMMAND "${CMAKE_COMMAND}"
                             -E remove "${_GENCPY_OUTPUT}"
                     COMMAND "${CMAKE_COMMAND}"
                             -E copy "${_GENCPY_INPUT}" "${_GENCPY_OUTPUT}"
                     DEPENDS "${_GENCPY_INPUT}" ${_GENCPY_DEPENDS})
endfunction()
