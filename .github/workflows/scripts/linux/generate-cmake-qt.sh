#!/usr/bin/env bash

set -e

if [[ -z "${DEPS_PREFIX}" ]]; then
  echo "DEPS_PREFIX is not set."
  exit 1
fi

echo "Using build dependencies from: ${DEPS_PREFIX}"

if [ "${COMPILER}" = "clang" ]; then
  if [[ -z "${CLANG_PATH}" ]] || [[ -z "${CLANGXX_PATH}" ]]; then
    echo "CLANG_PATH or CLANGXX_PATH is not set."
    exit 1
  fi

	echo "Using clang toolchain"
	cat > "clang-toolchain.cmake" << EOF
set(CMAKE_C_COMPILER "${CLANG_PATH}")
set(CMAKE_CXX_COMPILER "${CLANGXX_PATH}")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-fuse-ld=lld")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "-fuse-ld=lld")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-fuse-ld=lld")
EOF
	ADDITIONAL_CMAKE_ARGS="$ADDITIONAL_CMAKE_ARGS -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON -DCMAKE_TOOLCHAIN_FILE=clang-toolchain.cmake"
fi

echo "Additional CMake Args - ${ADDITIONAL_CMAKE_ARGS}"

# Generate CMake into ./build
# DISABLE_ADVANCE_SIMD is needed otherwise we end up doing -march=native.

# shellcheck disable=SC2086
cmake \
	-B build \
	-G Ninja \
	$ADDITIONAL_CMAKE_ARGS \
	-DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
	-DCMAKE_BUILD_TYPE=Release \
	-DX11_API=ON \
	-DWAYLAND_API=ON \
	-DENABLE_SETCAP=OFF \
	-DCMAKE_PREFIX_PATH="${DEPS_PREFIX}" \
	-DDISABLE_ADVANCE_SIMD=TRUE

