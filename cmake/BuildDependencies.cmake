# Build PCSX2 dependencies during the configure step.
#
# This is intentionally executed during CMake configure because
# SearchForStuff.cmake uses find_package() during configure.

set(PCSX2_DEPS_PREFIX "${CMAKE_BINARY_DIR}/deps" CACHE PATH
	"Where PCSX2 dependencies are installed")

set(PCSX2_DEPS_JOBS "" CACHE STRING
	"Parallel jobs for dependency builds")

set(_deps_build "${CMAKE_BINARY_DIR}/deps-build")

set(_deps_args
	-DCMAKE_INSTALL_PREFIX=${PCSX2_DEPS_PREFIX}
)

if(PCSX2_DEPS_JOBS)
	list(APPEND _deps_args -DNPROCS=${PCSX2_DEPS_JOBS})
endif()

# Pass the parent toolchain/compiler configuration to the dependency build.
foreach(_var
	CMAKE_TOOLCHAIN_FILE
	CMAKE_C_COMPILER
	CMAKE_CXX_COMPILER
	CMAKE_C_COMPILER_LAUNCHER
	CMAKE_CXX_COMPILER_LAUNCHER
	CMAKE_SYSROOT
	CMAKE_OSX_DEPLOYMENT_TARGET
	CMAKE_OSX_ARCHITECTURES
	ANDROID_ABI
	ANDROID_PLATFORM
)
	if(DEFINED ${_var})
		list(APPEND _deps_args -D${_var}=${${_var}})
	endif()
endforeach()

message(STATUS "")
message(STATUS "Building PCSX2 dependencies into:")
message(STATUS "  ${PCSX2_DEPS_PREFIX}")
message(STATUS "")

execute_process(
	COMMAND ${CMAKE_COMMAND}
		-S "${CMAKE_SOURCE_DIR}/pcsx2-deps"
		-B "${_deps_build}"
		-G "${CMAKE_GENERATOR}"
		${_deps_args}
	RESULT_VARIABLE _deps_result
)

if(NOT _deps_result EQUAL 0)
	message(FATAL_ERROR
		"Configuring the PCSX2 dependency build failed (${_deps_result})")
endif()

execute_process(
	COMMAND ${CMAKE_COMMAND}
		--build "${_deps_build}"
		--parallel
	RESULT_VARIABLE _deps_result
)

if(NOT _deps_result EQUAL 0)
	message(FATAL_ERROR
		"Building the PCSX2 dependencies failed (${_deps_result})")
endif()

# Make the freshly installed packages visible to SearchForStuff.cmake.
list(PREPEND CMAKE_PREFIX_PATH "${PCSX2_DEPS_PREFIX}")

if(CMAKE_FIND_ROOT_PATH)
	list(PREPEND CMAKE_FIND_ROOT_PATH "${PCSX2_DEPS_PREFIX}")
endif()
