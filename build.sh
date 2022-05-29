#!/bin/sh

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
set -u

# Function declarations
find_freetype()
{
    if [ "$(uname -m)" = "x86_64" ] && [ -e "/usr/include/x86_64-linux-gnu/freetype2/ft2build.h" ]; then
        export GTKMM_BASEPATH=/usr/include/x86_64-linux-gnu/freetype2
    fi
}

set_make()
{
    ncpu=$(getconf NPROCESSORS_ONLN 2>/dev/null || getconf _NPROCESSORS_ONLN)
    if command -v ninja >/dev/null ; then
        flags="$flags -GNinja"
        make=ninja
    else
        make="make -j$ncpu"
    fi
}

set_compiler()
{
    if [ "$useClang" -eq 1 ]; then
        CC=clang CXX=clang++ cmake $flags "$root" 2>&1 | tee -a "$log"
    else
        if [ "$useIcc" -eq 1 ]; then
            CC="icc" CXX="icpc" cmake $flags "$root" 2>&1 | tee -a "$log"
        else
        # Default compiler AKA GCC
        cmake $flags "$root" 2>&1 | tee -a "$log"
        fi
    fi
}

run_cppcheck()
{
    summary=cpp_check_summary.log
    rm -f $summary
    touch $summary

    define=""
    for undef in _WINDOWS _M_AMD64 _MSC_VER WIN32 __INTEL_COMPILER __x86_64__ \
    __SSE4_1__ __SSSE3__ __SSE__ __AVX2__ __USE_ISOC11 ASAN_WORKAROUND ENABLE_OGL_DEBUG \
    XBYAK_USE_MMAP_ALLOCATOR MAP_ANONYMOUS MAP_ANON XBYAK_DISABLE_AVX512
    do
        define="$define -U$undef"
    done

    check="--enable=warning,style,missingInclude"

    for d in pcsx2 common
    do
        flat_d=$(echo $d | sed -e 's@/@_@')
        log=cpp_check__${flat_d}.log
        rm -f "$log"

        cppcheck $check -j "$ncpu" --platform=unix32 "$define" "$root/$d" 2>&1 | tee "$log"
        # Create a small summary (warning it might miss some issues)
        fgrep -e "(warning)" -e "(error)" -e "(style)" -e "(performance)" -e "(portability)" "$log" >> $summary
    done

    exit 0
}

run_clangtidy()
{
    compile_json=compile_commands.json
    cpp_list=cpp_file.txt
    summary=clang_tidy_summary.txt
    grep '"file"' $compile_json | sed -e 's/"//g' -e 's/^\s*file\s*:\s*//' | sort -u  > $cpp_list

    # EXAMPLE
    #
    #   Modernize loop syntax, fix if old style found.
    #     $ clang-tidy -p build_dev/compile_commands.json pcsx2/GS/Renderers/HW/GSTextureCache.cpp -checks='modernize-loop-convert' -fix
    #   Check all, tons of output:
    #     $ clang-tidy -p $compile_json $cpp -checks='*' -header-filter='.*'
    #   List of modernize checks:
    #     modernize-loop-convert
    #     modernize-make-unique
    #     modernize-pass-by-value
    #     modernize-redundant-void-arg
    #     modernize-replace-auto-ptr
    #     modernize-shrink-to-fit
    #     modernize-use-auto
    #     modernize-use-default
    #     modernize-use-nullptr
    #     modernize-use-override

    # Don't check headers, don't check google/llvm coding conventions
    if command -v parallel >/dev/null ; then
        # Run clang-tidy in parallel with as many jobs as there are CPUs.
        parallel -v --keep-order "clang-tidy -p $compile_json -checks='*,-llvm-*,-google-*' {}"
    else
        # xargs(1) can also run jobs in parallel with -P, but will mix the
        # output from the distinct processes together willy-nilly.
        xargs clang-tidy -p $compile_json -checks='*,-llvm-*,-google-*'
    fi < $cpp_list > $summary

    exit 0
}

run_coverity()
{
    cov-build --dir "$coverity_dir" $make 2>&1 | tee -a "$log"
    # Warning: $coverity_dir must be the root directory
    (cd "$build"; tar caf $coverity_result "$coverity_dir")
    exit 0
}

# Main script
flags="-DCMAKE_BUILD_PO=FALSE"

cleanBuild=0
useClang=0
useIcc=0

CoverityBuild=0
cppcheck=0
clangTidy=0

root=$PWD/$(dirname "$0")
log="$root/install_log.txt"
build="$root/build"
coverity_dir="cov-int"
coverity_result=pcsx2-coverity.xz

set_make

for ARG in "$@"; do
    case "$ARG" in
        --clean             ) cleanBuild=1 ;;
        --clang-tidy        ) flags="$flags -DCMAKE_EXPORT_COMPILE_COMMANDS=ON"; clangTidy=1 ; useClang=1;;
        --ftime-trace       ) flags="$flags -DTIMETRACE=TRUE"; useClang=1;;
        --clang             ) useClang=1 ;;
        --intel             ) useIcc=1 ;;
        --cppcheck          ) cppcheck=1 ;;
        --dev|--devel       ) flags="$flags -DCMAKE_BUILD_TYPE=Devel"   ; build="$root/build_dev";;
        --dbg|--debug       ) flags="$flags -DCMAKE_BUILD_TYPE=Debug"   ; build="$root/build_dbg";;
        --rel|--release     ) flags="$flags -DCMAKE_BUILD_TYPE=Release" ; build="$root/build_rel";;
        --prof              ) flags="$flags -DCMAKE_BUILD_TYPE=RelWithDebInfo"; build="$root/build_prof";;
        --strip             ) flags="$flags -DCMAKE_BUILD_STRIP=TRUE" ;;
        --use-system-yaml   ) flags="$flags -DUSE_SYSTEM_YAML=TRUE" ;;
        --asan              ) flags="$flags -DUSE_ASAN=TRUE" ;;
        --gtk2              ) flags="$flags -DGTK2_API=TRUE" ;;
        --lto               ) flags="$flags -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=TRUE" ;;
        --pgo-optimize      ) flags="$flags -DUSE_PGO_OPTIMIZE=TRUE" ;;
        --pgo-generate      ) flags="$flags -DUSE_PGO_GENERATE=TRUE" ;;
        --no-portaudio      ) flags="$flags -DPORTAUDIO_API=FALSE" ;;
        --no-simd           ) flags="$flags -DDISABLE_ADVANCE_SIMD=TRUE" ;;
        --no-trans          ) flags="$flags -DNO_TRANSLATION=TRUE" ;;
        --coverity          ) CoverityBuild=1; cleanBuild=1; ;;
        --vtune             ) flags="$flags -DUSE_VTUNE=TRUE" ;;
        -D*                 ) flags="$flags $ARG" ;;

        *)
            echo $ARG
            # Unknown option
            echo "** User options **"
            echo "--dev / --devel   : Build PCSX2 as a Development build."
            echo "--dbg / --debug   : Build PCSX2 as a Debug build."
            echo "--prof            : Build PCSX2 as a Profiler build (release + debug symbol)."
            echo "--rel / --release : Build PCSX2 as a Release build."
            echo
            echo "--clean           : Do a clean build. (Remove anything in the build directory)"
            echo "--no-simd         : Only allow sse2."
            echo
            echo "** Distribution Compatibilities **"
            echo "--no-portaudio    : Skip portaudio for SPU2."
            echo "--use-system-yaml : Use system rapidyaml library."
            echo
            echo "** Expert Developer option **"
            echo "--gtk2            : use GTK 2 instead of GTK 3."
            echo "--no-trans        : Don't regenerate mo files when building."
            echo "--strip           : Strip binaries to save a small amount of space."
            echo "--clang           : Build with Clang/llvm."
            echo "--intel           : Build with ICC (Intel compiler)."
            echo "--lto             : Use Link Time Optimization."
            echo "--pgo-generate    : Executable will generate profiling information when run."
            echo "--pgo-optimize    : Use previously generated profiling information."
            echo "-D<argument>      : Add <argument> to the flags passed."
            echo
            echo "** Quality & Assurance (Please install the external tool) **"
            echo "--asan            : Enable Address sanitizer."
            echo "--clang-tidy      : Do a clang-tidy analysis. Results can be found in build directory."
            echo "--cppcheck        : Do a cppcheck analysis. Results can be found in build directory."
            echo "--coverity        : Do a build for coverity."
            echo "--vtune           : Plug GS with VTUNE."
            echo "--ftime-trace     : Analyse build time. Clang only."

            exit 1
    esac
done

if [ "$cleanBuild" -eq 1 ]; then
    echo "Doing a clean build."
    # allow to keep build as a symlink (for example to a ramdisk)
    rm -fr "$build"/*
fi

# Workaround for Debian. Cmake failed to find freetype include path
find_freetype

echo "Building pcsx2 with $flags" | tee "$log"

# Resolve the symlink otherwise cmake is lost
# Besides, it allows 'mkdir' to create the real destination directory
if [ -L "$build"  ]; then
    build=$(readlink "$build")
fi

mkdir -p "$build"
# Cmake will generate file inside $CWD. It would be nicer if an option to cmake can be provided.
cd "$build"

set_compiler

if [ "$cppcheck" -eq 1 ] && command -v cppcheck >/dev/null ; then
    run_cppcheck
fi

if [ "$clangTidy" -eq 1 ] && command -v clang-tidy >/dev/null ; then
    run_clangtidy
fi

if [ "$CoverityBuild" -eq 1 ] && command -v cov-build >/dev/null ; then
    run_coverity
fi

$make 2>&1 | tee -a "$log"
$make install 2>&1 | tee -a "$log"

exit 0
