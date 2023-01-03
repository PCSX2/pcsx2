#!/usr/bin/env bash

set -e

if [ "${COMPILER}" = "clang" ]; then
	echo "Using clang toolchain"
	cat > "$HOME/clang-toolchain.cmake" << EOF
set(CMAKE_C_COMPILER /usr/bin/clang-12)
set(CMAKE_CXX_COMPILER /usr/bin/clang++-12)
set(CMAKE_EXE_LINKER_FLAGS_INIT "-fuse-ld=lld")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "-fuse-ld=lld")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-fuse-ld=lld")
EOF
	ADDITIONAL_CMAKE_ARGS="$ADDITIONAL_CMAKE_ARGS -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON -DCMAKE_TOOLCHAIN_FILE=$HOME/clang-toolchain.cmake"
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
	-DQT_BUILD=ON \
	-DCUBEB_API=ON \
	-DX11_API=ON \
	-DWAYLAND_API=ON \
	-DENABLE_SETCAP=OFF \
	-DCMAKE_PREFIX_PATH="$HOME/deps" \
	-DUSE_SYSTEM_SDL2=ON \
	-DUSE_SYSTEM_ZSTD=OFF \
	-DDISABLE_ADVANCE_SIMD=TRUE

