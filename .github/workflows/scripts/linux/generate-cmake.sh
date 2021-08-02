#!/bin/bash

set -e

if [ "${COMPILER}" = "gcc" ]; then
  export CC=gcc
  export CXX=g++
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
-DDISABLE_ADVANCE_SIMD=TRUE                 \
-DCMAKE_INSTALL_LIBDIR="/tmp/"              \
-DCMAKE_INSTALL_DATADIR="/tmp/"             \
-DCMAKE_INSTALL_DOCDIR="/tmp/PCSX2"         \
-DOpenGL_GL_PREFERENCE="LEGACY"             \
-DOPENGL_opengl_LIBRARY=""                  \
-DXDG_STD=TRUE                              \
$ADDITIONAL_CMAKE_ARGS                      \
-GNinja                                     \
-B build
