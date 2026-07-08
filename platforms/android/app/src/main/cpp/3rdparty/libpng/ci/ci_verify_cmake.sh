#!/usr/bin/env bash
set -o errexit -o pipefail -o posix

# Copyright (c) 2019-2025 Cosmin Truta.
#
# Use, modification and distribution are subject to the MIT License.
# Please see the accompanying file LICENSE_MIT.txt
#
# SPDX-License-Identifier: MIT

# shellcheck source=ci/lib/ci.lib.sh
source "$(dirname "$0")/lib/ci.lib.sh"
cd "$CI_TOPLEVEL_DIR"

CI_SRC_DIR="$CI_TOPLEVEL_DIR"
CI_OUT_DIR="$CI_TOPLEVEL_DIR/out"
CI_BUILD_DIR="$CI_OUT_DIR/ci_verify_cmake.$CI_TARGET_SYSTEM.$CI_TARGET_ARCH.build"
CI_INSTALL_DIR="$CI_OUT_DIR/ci_verify_cmake.$CI_TARGET_SYSTEM.$CI_TARGET_ARCH.install"

function ci_init_build {
    # Ensure that the mandatory variables are initialized.
    CI_CMAKE="${CI_CMAKE:-cmake}"
    CI_CTEST="${CI_CTEST:-ctest}"
    CI_CMAKE_BUILD_TYPE="${CI_CMAKE_BUILD_TYPE:-Release}"
    if [[ $CI_CMAKE_GENERATOR == "Visual Studio"* ]]
    then
        # Clean up incidental mixtures of Windows and Bash-on-Windows
        # environment variables, to avoid confusing MSBuild.
        [[ $TEMP && ( $Temp || $temp ) ]] && unset TEMP
        [[ $TMP && ( $Tmp || $tmp ) ]] && unset TMP
        # Ensure that CI_CMAKE_GENERATOR_PLATFORM is initialized for this generator.
        [[ $CI_CMAKE_GENERATOR_PLATFORM ]] || {
            ci_err_internal "missing \$CI_CMAKE_GENERATOR_PLATFORM"
        }
    elif [[ -x $(command -v ninja) ]]
    then
        CI_CMAKE_GENERATOR="${CI_CMAKE_GENERATOR:-Ninja}"
    fi
}

function ci_trace_build {
    ci_info "## START OF CONFIGURATION ##"
    ci_info "build arch: $CI_BUILD_ARCH"
    ci_info "build system: $CI_BUILD_SYSTEM"
    [[ "$CI_TARGET_SYSTEM.$CI_TARGET_ARCH" != "$CI_BUILD_SYSTEM.$CI_BUILD_ARCH" ]] && {
        ci_info "target arch: $CI_TARGET_ARCH"
        ci_info "target system: $CI_TARGET_SYSTEM"
    }
    ci_info "source directory: $CI_SRC_DIR"
    ci_info "build directory: $CI_BUILD_DIR"
    ci_info "install directory: $CI_INSTALL_DIR"
    ci_info "environment option: \$CI_CMAKE: '$CI_CMAKE'"
    ci_info "environment option: \$CI_CMAKE_GENERATOR: '$CI_CMAKE_GENERATOR'"
    ci_info "environment option: \$CI_CMAKE_GENERATOR_PLATFORM: '$CI_CMAKE_GENERATOR_PLATFORM'"
    ci_info "environment option: \$CI_CMAKE_BUILD_TYPE: '$CI_CMAKE_BUILD_TYPE'"
    ci_info "environment option: \$CI_CMAKE_BUILD_FLAGS: '$CI_CMAKE_BUILD_FLAGS'"
    ci_info "environment option: \$CI_CMAKE_TOOLCHAIN_FILE: '$CI_CMAKE_TOOLCHAIN_FILE'"
    ci_info "environment option: \$CI_CMAKE_VARS: '$CI_CMAKE_VARS'"
    ci_info "environment option: \$CI_CTEST: '$CI_CTEST'"
    ci_info "environment option: \$CI_CTEST_FLAGS: '$CI_CTEST_FLAGS'"
    ci_info "environment option: \$CI_CC: '$CI_CC'"
    ci_info "environment option: \$CI_CC_FLAGS: '$CI_CC_FLAGS'"
    ci_info "environment option: \$CI_AR: '$CI_AR'"
    ci_info "environment option: \$CI_RANLIB: '$CI_RANLIB'"
    ci_info "environment option: \$CI_SANITIZERS: '$CI_SANITIZERS'"
    ci_info "environment option: \$CI_FORCE: '$CI_FORCE'"
    ci_info "environment option: \$CI_NO_BUILD: '$CI_NO_BUILD'"
    ci_info "environment option: \$CI_NO_TEST: '$CI_NO_TEST'"
    ci_info "environment option: \$CI_NO_INSTALL: '$CI_NO_INSTALL'"
    ci_info "environment option: \$CI_NO_CLEAN: '$CI_NO_CLEAN'"
    ci_info "executable: \$CI_CMAKE: $(command -V "$CI_CMAKE")"
    ci_info "executable: \$CI_CTEST: $(command -V "$CI_CTEST")"
    [[ $CI_CC ]] && {
        ci_info "executable: \$CI_CC: $(command -V "$CI_CC")"
    }
    [[ $CI_AR ]] && {
        ci_info "executable: \$CI_AR: $(command -V "$CI_AR")"
    }
    [[ $CI_RANLIB ]] && {
        ci_info "executable: \$CI_RANLIB: $(command -V "$CI_RANLIB")"
    }
    ci_info "## END OF CONFIGURATION ##"
}

function ci_cleanup_old_build {
    ci_info "## START OF PRE-BUILD CLEANUP ##"
    [[ ! -e $CI_BUILD_DIR && ! -e $CI_INSTALL_DIR ]] || {
        ci_spawn rm -fr "$CI_BUILD_DIR"
        ci_spawn rm -fr "$CI_INSTALL_DIR"
    }
    ci_info "## END OF PRE-BUILD CLEANUP ##"
}

function ci_build {
    ci_info "## START OF BUILD ##"
    # Adjust the CI environment variables, as needed.
    CI_CMAKE="$(command -v "$CI_CMAKE")" || ci_err "bad or missing \$CI_CMAKE"
    ci_spawn "$CI_CMAKE" --version
    CI_CTEST="$(command -v "$CI_CTEST")" || ci_err "bad or missing \$CI_CTEST"
    ci_spawn "$CI_CTEST" --version
    [[ $CI_CMAKE_GENERATOR == *"Ninja"* ]] && {
        CI_NINJA="$(command -v ninja)" || ci_err "bad or missing ninja, no pun intended"
        ci_spawn "$CI_NINJA" --version
    }
    [[ $CI_AR ]] && {
        # Use the full path of CI_AR to work around a mysterious CMake error.
        CI_AR="$(command -v "$CI_AR")" || ci_err "bad or missing \$CI_AR"
    }
    [[ $CI_RANLIB ]] && {
        # Use the full path of CI_RANLIB to work around a mysterious CMake error.
        CI_RANLIB="$(command -v "$CI_RANLIB")" || ci_err "bad or missing \$CI_RANLIB"
    }
    # Export the CMake environment variables.
    [[ $CI_CMAKE_GENERATOR ]] && {
        ci_spawn export CMAKE_GENERATOR="$CI_CMAKE_GENERATOR"
    }
    [[ $CI_CMAKE_GENERATOR_PLATFORM ]] && {
        ci_spawn export CMAKE_GENERATOR_PLATFORM="$CI_CMAKE_GENERATOR_PLATFORM"
    }
    # Initialize and populate the local arrays.
    local all_cmake_vars=()
    local all_cmake_build_flags=()
    local all_ctest_flags=()
    [[ $CI_CMAKE_TOOLCHAIN_FILE ]] && {
        all_cmake_vars+=("-DCMAKE_TOOLCHAIN_FILE=$CI_CMAKE_TOOLCHAIN_FILE")
    }
    [[ $CI_CC ]] && {
        all_cmake_vars+=("-DCMAKE_C_COMPILER=$CI_CC")
    }
    [[ $CI_CC_FLAGS || $CI_SANITIZERS ]] && {
        [[ $CI_SANITIZERS ]] && CI_CC_FLAGS+="${CI_CC_FLAGS:+" "}-fsanitize=$CI_SANITIZERS"
        all_cmake_vars+=("-DCMAKE_C_FLAGS=$CI_CC_FLAGS")
    }
    [[ $CI_AR ]] && {
        all_cmake_vars+=("-DCMAKE_AR=$CI_AR")
    }
    [[ $CI_RANLIB ]] && {
        all_cmake_vars+=("-DCMAKE_RANLIB=$CI_RANLIB")
    }
    all_cmake_vars+=("-DCMAKE_BUILD_TYPE=$CI_CMAKE_BUILD_TYPE")
    all_cmake_vars+=("-DCMAKE_VERBOSE_MAKEFILE=ON")
    all_cmake_vars+=($CI_CMAKE_VARS)
    all_cmake_build_flags+=($CI_CMAKE_BUILD_FLAGS)
    all_ctest_flags+=($CI_CTEST_FLAGS)
    # And... build!
    ci_spawn mkdir -p "$CI_BUILD_DIR"
    # Spawn "cmake ...".
    ci_spawn "$CI_CMAKE" -B "$CI_BUILD_DIR" \
                         -S . \
                         -DCMAKE_INSTALL_PREFIX="$CI_INSTALL_DIR" \
                         "${all_cmake_vars[@]}"
    ci_expr $((CI_NO_BUILD)) || {
        # Spawn "cmake --build ...".
        ci_spawn "$CI_CMAKE" --build "$CI_BUILD_DIR" \
                             --config "$CI_CMAKE_BUILD_TYPE" \
                             "${all_cmake_build_flags[@]}"
    }
    ci_expr $((CI_NO_TEST)) || {
        # Spawn "ctest" if testing is not disabled.
        ci_spawn pushd "$CI_BUILD_DIR"
        ci_spawn "$CI_CTEST" --build-config "$CI_CMAKE_BUILD_TYPE" \
                             "${all_ctest_flags[@]}"
        ci_spawn popd
    }
    ci_expr $((CI_NO_INSTALL)) || {
        # Spawn "cmake --build ... --target install" if installation is not disabled.
        ci_spawn "$CI_CMAKE" --build "$CI_BUILD_DIR" \
                             --config "$CI_CMAKE_BUILD_TYPE" \
                             --target install \
                             "${all_cmake_build_flags[@]}"
    }
    ci_expr $((CI_NO_CLEAN)) || {
        # Spawn "make --build ... --target clean" if cleaning is not disabled.
        ci_spawn "$CI_CMAKE" --build "$CI_BUILD_DIR" \
                             --config "$CI_CMAKE_BUILD_TYPE" \
                             --target clean \
                             "${all_cmake_build_flags[@]}"
    }
    ci_info "## END OF BUILD ##"
}

function usage {
    echo "usage: $CI_SCRIPT_NAME [<options>]"
    echo "options: -?|-h|--help"
    exit "${@:-0}"
}

function main {
    local opt
    while getopts ":" opt
    do
        # This ain't a while-loop. It only pretends to be.
        [[ $1 == -[?h]* || $1 == --help || $1 == --help=* ]] && usage 0
        ci_err "unknown option: '$1'"
    done
    shift $((OPTIND - 1))
    # And... go!
    ci_init_build
    ci_trace_build
    [[ $# -eq 0 ]] || {
        echo >&2 "error: unexpected argument: '$1'"
        echo >&2 "note: this program accepts environment options only"
        usage 2
    }
    ci_cleanup_old_build
    ci_build
}

main "$@"
