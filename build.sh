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
# 0 => no, 1 => yes, 2 => force yes
useCross=2
CoverityBuild=0
cppcheck=0
clangTidy=0

root=$PWD/$(dirname "$0")
log="$root/install_log.txt"
build="$root/build"
coverity_dir="cov-int"
coverity_result=pcsx2-coverity.xz

if [[ $(uname -s) == 'Darwin' ]]; then
    ncpu=$(sysctl -n hw.ncpu)
    release=$(uname -r)
    if [[ ${release:0:2} -lt 13 ]]; then
        echo "This old OSX version is not supported! Build will fail."
        toolfile=cmake/darwin-compiler-i386-clang.cmake
    else
        echo "Using Mavericks build with C++11 support."
        toolfile=cmake/darwin13-compiler-i386-clang.cmake
    fi
else
    ncpu=$(grep -w -c processor /proc/cpuinfo)
    toolfile=cmake/linux-compiler-i386-multilib.cmake
fi

for ARG in "$@"; do
    case "$ARG" in
        --clean             ) cleanBuild=1 ;;
        --clang-tidy        ) flags+=(-DCMAKE_EXPORT_COMPILE_COMMANDS=ON); clangTidy=1 ;;
        --clang             ) useClang=1 ;;
        --cppcheck          ) cppcheck=1 ;;
        --dev|--devel       ) flags+=(-DCMAKE_BUILD_TYPE=Devel)   build="$root/build_dev";;
        --dbg|--debug       ) flags+=(-DCMAKE_BUILD_TYPE=Debug)   build="$root/build_dbg";;
        --release           ) flags+=(-DCMAKE_BUILD_TYPE=Release) build="$root/build_rel";;
        --strip             ) flags+=(-DCMAKE_BUILD_STRIP=TRUE) ;;
        --glsl              ) flags+=(-DGLSL_API=TRUE) ;;
        --egl               ) flags+=(-DEGL_API=TRUE) ;;
        --sdl12             ) flags+=(-DSDL2_API=FALSE) ;;
        --extra             ) flags+=(-DEXTRA_PLUGINS=TRUE) ;;
        --asan              ) flags+=(-DUSE_ASAN=TRUE) ;;
        --gtk3              ) flags+=(-DGTK3_API=TRUE) ;;
        --no-simd           ) flags+=(-DDISABLE_ADVANCE_SIMD=TRUE) ;;
        --cross-multilib    ) flags+=(-DCMAKE_TOOLCHAIN_FILE=$toolfile); useCross=1; ;;
        --no-cross-multilib ) useCross=0; ;;
        --coverity          ) CoverityBuild=1; cleanBuild=1; ;;
        -D*                 ) flags+=($ARG) ;;

        *)
            # Unknown option
            echo "** User options **"
            echo "--dev / --devel : Build PCSX2 as a Development build."
            echo "--debug         : Build PCSX2 as a Debug build."
            echo "--release       : Build PCSX2 as a Release build."
            echo
            echo "--clean         : Do a clean build."
            echo "--extra         : Build all plugins"
            echo "--no-simd       : Only allow sse2"
            echo
            echo "** Developer option **"
            echo "--glsl          : Replace CG backend of ZZogl by GLSL"
            echo "--egl           : Replace GLX by EGL (ZZogl/GSdx plugins)"
            echo "--cross-multilib: Build a 32bit PCSX2 on a 64bit machine using multilib."
            echo
            echo "** Distribution Compatibilities **"
            echo "--sdl12         : Build with SDL1.2 (requires if wx is linked against SDL1.2)"
            echo
            echo "** Expert Developer option **"
            echo "--gtk3          : replace GTK2 by GTK3"
            echo "--no-cross-multilib: Build a native PCSX2"
            echo "--clang         : Build with Clang/llvm"
            echo
            echo "** Quality & Assurance (Please install the external tool) **"
            echo "--asan          : Enable Address sanitizer"
            echo "--clang-tidy    : Do a clang-tidy analysis. Results can be found in build directory"
            echo "--cppcheck      : Do a cppcheck analysis. Results can be found in build directory"
            echo "--coverity      : Do a build for coverity"

            exit 1
    esac
done

if [[ "$cleanBuild" -eq 1 ]]; then
    echo "Doing a clean build."
    # allow to keep build as a symlink (for example to a ramdisk)
    rm -fr "$build"/*
fi

if [[ "$useCross" -eq 2 ]] && [[ "$(getconf LONG_BIT 2> /dev/null)" != 32 ]]; then
    echo "Forcing cross compilation."
    flags+=(-DCMAKE_TOOLCHAIN_FILE=$toolfile)
elif [[ "$useCross" -ne 1 ]]; then
    useCross=0
fi

# Helper to easily switch wx-config on my system
if [[ "$useCross" -eq 0 ]] && [[ "$(uname -m)" == "x86_64" ]] && [[ -e "/usr/lib/i386-linux-gnu/wx/config/gtk2-unicode-3.0" ]]; then
    sudo update-alternatives --set wx-config /usr/lib/x86_64-linux-gnu/wx/config/gtk2-unicode-3.0
fi
if [[ "$useCross" -eq 2 ]] && [[ "$(uname -m)" == "x86_64" ]] && [[ -e "/usr/lib/x86_64-linux-gnu/wx/config/gtk2-unicode-3.0" ]]; then
    sudo update-alternatives --set wx-config /usr/lib/i386-linux-gnu/wx/config/gtk2-unicode-3.0
fi

echo "Building pcsx2 with ${flags[*]}" | tee "$log"

# Resolve the symlink otherwise cmake is lost
# Besides, it allows 'mkdir' to create the real destination directory
if [[ -L "$build"  ]]; then
    build=`readlink "$build"`
fi

mkdir -p "$build"
# Cmake will generate file inside $CWD. It would be nicer if an option to cmake can be provided.
cd "$build"

if [[ "$useClang" -eq 1 ]]; then
    if [[ "$useCross" -eq 0 ]]; then
        CC=clang CXX=clang++ cmake "${flags[@]}" "$root" 2>&1 | tee -a "$log"
    else
        CC="clang -m32" CXX="clang++ -m32" cmake "${flags[@]}" "$root" 2>&1 | tee -a "$log"
    fi
else
    cmake "${flags[@]}" "$root" 2>&1 | tee -a "$log"
fi



############################################################
# CPP check build
############################################################
if [[ "$cppcheck" -eq 1 ]] && [[ -x `which cppcheck` ]]; then
    summary=cpp_check_summary.log
    rm -f $summary
    touch $summary

    for undef in _WINDOWS _M_AMD64 _MSC_VER WIN32 __INTEL_COMPILER __x86_64__ \
        __SSE4_1__ __SSSE3__ __SSE__ __AVX2__ __USE_ISOC11 ASAN_WORKAROUND ENABLE_OPENCL ENABLE_OGL_DEBUG
    do
        define="$define -U$undef"
    done
    check="--enable=warning,style,missingInclude"
    for d in pcsx2 common plugins/GSdx plugins/spu2\-x plugins/onepad
    do
        flat_d=`echo $d | sed -e 's@/@_@'`
        log=cpp_check__${flat_d}.log
        rm -f "$log"

        cppcheck $check -j $ncpu --platform=unix32 $define "$root/$d" 2>&1 | tee "$log"
        # Create a small summary (warning it might miss some issues)
        fgrep -e "(warning)" -e "(error)" -e "(style)" -e "(performance)" -e "(portability)" "$log" >> $summary
    done
    exit 0
fi

############################################################
# Clang tidy build
############################################################
if [[ "$clangTidy" -eq 1 ]] && [[ -x `which clang-tidy` ]]; then
    compile_json=compile_commands.json
    cpp_list=cpp_file.txt
    summary=clang_tidy_summary.txt
    rm -f $summary
    touch $summary

    grep '"file"' $compile_json | sed -e 's/"//g' | sed -e 's/^\s*file\s*:\s*//' > $cpp_list

    for cpp in `cat $cpp_list`
    do
        # Check all, likely severals millions of log...
        #clang-tidy -p $compile_json $cpp -checks='*' -header-filter='.*'

        # Don't check header, don't check google/llvm coding conventions
        echo "$count/$total"
        clang-tidy -p $compile_json $cpp -checks='*,-llvm-*,-google-*'  >> $summary
    done

    exit 0
fi

############################################################
# Coverity build
############################################################
if [[ "$CoverityBuild" -eq 1 ]] && [[ -x `which cov-build` ]]; then
    cov-build --dir "$coverity_dir" make -j"$ncpu" 2>&1 | tee -a "$log"
    # Warning: $coverity_dir must be the root directory
    (cd "$build"; tar caf $coverity_result "$coverity_dir")
    exit 0
fi

############################################################
# Real build
############################################################
make -j"$ncpu" 2>&1 | tee -a "$log"
make install 2>&1 | tee -a "$log"

exit 0
