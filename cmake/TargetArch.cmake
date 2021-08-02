# Copyright (c) 2014 PCSX2 Dev Team
# Copyright (c) 2012 Petroules Corporation. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this
#    list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# https://github.com/petroules/solar-cmake/blob/master/TargetArch.cmake
#
# Based on the Qt 5 processor detection code, so should be very accurate
# https://qt.gitorious.org/qt/qtbase/blobs/master/src/corelib/global/qprocessordetection.h
# Currently handles arm (v5, v6, v7), x86 (32/64), ia64, and ppc (32/64)

# Regarding POWER/PowerPC, just as is noted in the Qt source,
# "There are many more known variants/revisions that we do not handle/detect."

set(archdetect_c_code "
#if defined(__arm__) || defined(__TARGET_ARCH_ARM)
	#if defined(__ARM_ARCH_7__) \\
		|| defined(__ARM_ARCH_7A__) \\
		|| defined(__ARM_ARCH_7R__) \\
		|| defined(__ARM_ARCH_7M__) \\
		|| (defined(__TARGET_ARCH_ARM) && __TARGET_ARCH_ARM-0 >= 7)
		#error cmake_ARCH armv7
	#elif defined(__ARM_ARCH_6__) \\
		|| defined(__ARM_ARCH_6J__) \\
		|| defined(__ARM_ARCH_6T2__) \\
		|| defined(__ARM_ARCH_6Z__) \\
		|| defined(__ARM_ARCH_6K__) \\
		|| defined(__ARM_ARCH_6ZK__) \\
		|| defined(__ARM_ARCH_6M__) \\
		|| (defined(__TARGET_ARCH_ARM) && __TARGET_ARCH_ARM-0 >= 6)
		#error cmake_ARCH armv6
	#elif defined(__ARM_ARCH_5TEJ__) \\
		|| (defined(__TARGET_ARCH_ARM) && __TARGET_ARCH_ARM-0 >= 5)
		#error cmake_ARCH armv5
	#else
		#error cmake_ARCH arm
	#endif
#elif defined(__i386) || defined(__i386__) || defined(_M_IX86)
	#error cmake_ARCH i386
#elif defined(__x86_64) || defined(__x86_64__) || defined(__amd64) || defined(_M_X64)
	#error cmake_ARCH x86_64
#elif defined(__ia64) || defined(__ia64__) || defined(_M_IA64)
	#error cmake_ARCH ia64
#elif defined(__ppc__) || defined(__ppc) || defined(__powerpc__) \\
	|| defined(_ARCH_COM) || defined(_ARCH_PWR) || defined(_ARCH_PPC)  \\
	|| defined(_M_MPPC) || defined(_M_PPC)
	#if defined(__ppc64__) || defined(__powerpc64__) || defined(__64BIT__)
		#error cmake_ARCH ppc64
	#else
		#error cmake_ARCH ppc
	#endif
#endif

#error cmake_ARCH unknown
")

# Set ppc_support to TRUE before including this file or ppc and ppc64
# will be treated as invalid architectures since they are no longer supported by Apple

function(target_architecture output_var)
	if(APPLE AND CMAKE_OSX_ARCHITECTURES)
		# On OS X we use CMAKE_OSX_ARCHITECTURES *if* it was set
		# First let's normalize the order of the values

		# Note that it's not possible to compile PowerPC applications if you are using
		# the OS X SDK version 10.6 or later - you'll need 10.4/10.5 for that, so we
		# disable it by default
		# See this page for more information:
		# http://stackoverflow.com/questions/5333490/how-can-we-restore-ppc-ppc64-as-well-as-full-10-4-10-5-sdk-support-to-xcode-4

		# Architecture defaults to i386 or ppc on OS X 10.5 and earlier, depending on the CPU type detected at runtime.
		# On OS X 10.6+ the default is x86_64 if the CPU supports it, i386 otherwise.

		LIST(LENGTH CMAKE_OSX_ARCHITECTURES osx_arch_num)
		if(NOT (osx_arch_num EQUAL 1))
			message(FATAL_ERROR "Currently ${CMAKE_PROJECT_NAME} does not support multiple architectures in CMAKE_OSX_ARCHITECTURES")
		endif()

		foreach(osx_arch ${CMAKE_OSX_ARCHITECTURES})
			if("${osx_arch}" STREQUAL "ppc" AND ppc_support)
				set(osx_arch_ppc TRUE)
			elseif("${osx_arch}" STREQUAL "i386")
				set(osx_arch_i386 TRUE)
			elseif("${osx_arch}" STREQUAL "x86_64")
				set(osx_arch_x86_64 TRUE)
			elseif("${osx_arch}" STREQUAL "ppc64" AND ppc_support)
				set(osx_arch_ppc64 TRUE)
			else()
				message(FATAL_ERROR "Invalid OS X arch name: ${osx_arch}")
			endif()
		endforeach()

		# Now add all the architectures in our normalized order
		if(osx_arch_ppc)
			list(APPEND ARCH ppc)
		endif()

		if(osx_arch_i386)
			list(APPEND ARCH i386)
		endif()

		if(osx_arch_x86_64)
			list(APPEND ARCH x86_64)
		endif()

		if(osx_arch_ppc64)
			list(APPEND ARCH ppc64)
		endif()

		LIST(LENGTH ARCH osx_arch_num)
		if(osx_arch_num LESS 1)
			message(FATAL_ERROR "Invalid CMAKE_OSX_ARCHITECTURES: ${CMAKE_OSX_ARCHITECTURES}")
		endif()
	else()
		file(WRITE "${CMAKE_BINARY_DIR}/arch.c" "${archdetect_c_code}")

		enable_language(C)

		# Detect the architecture in a rather creative way...
		# This compiles a small C program which is a series of ifdefs that selects a
		# particular #error preprocessor directive whose message string contains the
		# target architecture. The program will always fail to compile (both because
		# file is not a valid C program, and obviously because of the presence of the
		# #error preprocessor directives... but by exploiting the preprocessor in this
		# way, we can detect the correct target architecture even when cross-compiling,
		# since the program itself never needs to be run (only the compiler/preprocessor)
		try_run(
			run_result_unused
			compile_result_unused
			"${CMAKE_BINARY_DIR}"
			"${CMAKE_BINARY_DIR}/arch.c"
			COMPILE_OUTPUT_VARIABLE ARCH
			CMAKE_FLAGS CMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES}
		)

		# Parse the architecture name from the compiler output
		string(REGEX MATCH "cmake_ARCH ([a-zA-Z0-9_]+)" ARCH "${ARCH}")

		# Get rid of the value marker leaving just the architecture name
		string(REPLACE "cmake_ARCH " "" ARCH "${ARCH}")

		# If we are compiling with an unknown architecture this variable should
		# already be set to "unknown" but in the case that it's empty (i.e. due
		# to a typo in the code), then set it to unknown
		if (NOT ARCH)
			set(ARCH unknown)
		endif()
	endif()

	set(${output_var} "${ARCH}" PARENT_SCOPE)
endfunction()
