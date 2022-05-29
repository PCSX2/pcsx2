#!/bin/bash

set -e

if [ "${COMPILER}" = "gcc" ]; then
  export CC=gcc-10
  export CXX=g++-10
else
  export CC=clang
  export CXX=clang++
fi

if [ "${PLATFORM}" = x86 ]; then
  ADDITIONAL_CMAKE_ARGS="$ADDITIONAL_CMAKE_ARGS -DCMAKE_TOOLCHAIN_FILE=cmake/linux-compiler-i386-multilib.cmake"
fi
echo "Additional CMake Args - ${ADDITIONAL_CMAKE_ARGS}"

# Generate CMake into ./build
cmake                                       \
-DCMAKE_CXX_COMPILER_LAUNCHER=ccache        \
-DCMAKE_BUILD_TYPE=Release                  \
-DPACKAGE_MODE=TRUE                         \
-DWAYLAND_API=TRUE                          \
-DDISABLE_ADVANCE_SIMD=TRUE                 \
-DDISABLE_PCSX2_WRAPPER=TRUE                \
-DCMAKE_INSTALL_PREFIX="squashfs-root/usr/" \
-DOpenGL_GL_PREFERENCE="LEGACY"             \
-DOPENGL_opengl_LIBRARY=""                  \
-DXDG_STD=TRUE                              \
-DUSE_SYSTEM_ZSTD=FALSE                     \
$ADDITIONAL_CMAKE_ARGS                      \
-GNinja                                     \
-B build
