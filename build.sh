#!/bin/bash

# PCSX2 - PS2 Emulator for PCs
# Copyright (C) 2002-2014  PCSX2 Dev Team
#
# PCSX2 is free software: you can redistribute it and/or modify it under the terms
# of the GNU Lesser General Public License as published by the Free Software Found-
# ation, either version 3 of the License, or (at your option) any later version.
#
# PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
# without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
# PURPOSE.  See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along with PCSX2.
# If not, see <http://www.gnu.org/licenses/>.

#set -e # This terminates the script in case of any error

flags=(-DCMAKE_BUILD_PO=FALSE)
cleanBuild=0
useClang=0

for ARG in "$@"; do
    case "$ARG" in
        --clean       ) cleanBuild=1 ;;
        --clang       ) flags+=(-DUSE_CLANG=TRUE); useClang=1; ;;
        --dev|--devel ) flags+=(-DCMAKE_BUILD_TYPE=Devel) ;;
        --dbg|--debug ) flags+=(-DCMAKE_BUILD_TYPE=Debug) ;;
        --strip       ) flags+=(-DCMAKE_BUILD_STRIP=TRUE) ;;
        --release     ) flags+=(-DCMAKE_BUILD_TYPE=Release) ;;
        --glsl        ) flags+=(-DGLSL_API=TRUE) ;;
        --egl         ) flags+=(-DEGL_API=TRUE) ;;
        --gles        ) flags+=(-DGLES_API=TRUE) ;;
        --sdl2        ) flags+=(-DSDL2_API=TRUE) ;;
        --extra       ) flags+=(-DEXTRA_PLUGINS=TRUE) ;;
        --asan        ) flags+=(-DUSE_ASAN=TRUE) ;;
        --wx28        ) flags+=(-DWX28_API=TRUE) ;;
        --wx30        ) flags+=(-DWX28_API=FALSE) ;;

        *)
            # Unknown option
            echo "** User options **"
            echo "--dev / --devel : Build PCSX2 as a Development build."
            echo "--debug         : Build PCSX2 as a Debug build."
            echo "--release       : Build PCSX2 as a Release build."
            echo "--clean         : Do a clean build."
            echo "** Developper option **"
            echo "--clang         : Build with Clang/llvm"
            echo "--extra         : Build all plugins"
            echo "--asan          : Enable with Address sanitizer"
            echo
            echo "--wx28          : Force wxWidget 2.8"
            echo "--wx30          : Allow to use wxWidget 3.0"
            echo "--glsl          : Replace CG backend of ZZogl by GLSL"
            echo "--egl           : Replace GLX by EGL (ZZogl plugins only)"
            echo "--sdl2          : Build with SDL2 (crash if wx is linked to SDL1)"
            echo "--gles          : Replace openGL backend of GSdx by openGLES3"
            exit 1
    esac
done

root=$PWD/$(dirname "$0")
log=$root/install_log.txt
build=$root/build

if [[ "$cleanBuild" -eq 1 ]]; then
    echo "Doing a clean build."
    # allow to keep build as a symlink
    rm -fr $build/*
fi

# Resolve the symlink otherwise cmake is lost

echo "Building pcsx2 with ${flags[*]}" | tee $log
if [[ -L $build  ]]; then
    build=`readlink $build`
fi

mkdir -p $build
# Cmake will generate file inside $CWD
cd $build

if [[ "$useClang" -eq 1 ]]; then
    CC=clang CXX=clang++ cmake "${flags[@]}" $root 2>&1 | tee -a $log
else
    cmake "${flags[@]}" $root 2>&1 | tee -a $log
fi

make -j "$(grep -w -c processor /proc/cpuinfo)" 2>&1 | tee -a $log
make install 2>&1 | tee -a $log

exit 0
