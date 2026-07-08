# PNGConfig.cmake
# Utility functions for configuring and building libpng

# Copyright (c) 2025 Cosmin Truta
#
# Use, modification and distribution are subject to
# the same licensing terms and conditions as libpng.
# Please see the copyright notice in png.h or visit
# http://libpng.org/pub/png/src/libpng-LICENSE.txt
#
# SPDX-License-Identifier: libpng-2.0

# Check libconf file (pnglibconf.h.* or *.dfa):
# png_check_libconf([HEADER <file>] [DFA_XTRA <file>])
function(png_check_libconf)
  set(options)
  set(oneValueArgs HEADER DFA_XTRA)
  set(multiValueArgs)

  cmake_parse_arguments(_CHK "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  if(_CHK_HEADER AND _CHK_DFA_XTRA)
    message(FATAL_ERROR "png_check_libconf: Mutually-exclusive arguments: HEADER and DFA_XTRA")
  endif()

  if(_CHK_HEADER)
    if(EXISTS "${_CHK_HEADER}")
      if("x${_CHK_HEADER}" STREQUAL "x${PNG_LIBCONF_HEADER_PREBUILT}")
        message(STATUS "Using standard libconf header: ${_CHK_HEADER}")
      else()
        message(STATUS "Using custom libconf header: ${_CHK_HEADER}")
      endif()
    else()
      message(SEND_ERROR "Could not find libconf header: ${_CHK_HEADER}")
    endif()
  else()
    if("x${_CHK_DFA_XTRA}" STREQUAL "x")
      message(STATUS "Creating standard configuration")
    elseif(EXISTS "${_CHK_DFA_XTRA}")
      message(STATUS "Creating custom configuration with DFA_XTRA file: ${_CHK_DFA_XTRA}")
    else()
      message(SEND_ERROR "Could not find DFA_XTRA file: ${_CHK_DFA_XTRA}")
    endif()
  endif()
endfunction()
