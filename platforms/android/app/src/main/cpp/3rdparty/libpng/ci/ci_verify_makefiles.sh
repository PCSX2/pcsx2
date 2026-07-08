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

function ci_init_build {
    # Ensure that the mandatory variables are initialized.
    CI_MAKE="${CI_MAKE:-make}"
    case "$CI_CC" in
    ( *clang* )
        CI_MAKEFILES="${CI_MAKEFILES:-"scripts/makefile.clang"}" ;;
    ( *gcc* )
        CI_MAKEFILES="${CI_MAKEFILES:-"scripts/makefile.gcc"}" ;;
    ( * )
        CI_MAKEFILES="${CI_MAKEFILES:-"scripts/makefile.std"}" ;;
    esac
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
    ci_info "environment option: \$CI_MAKEFILES: '$CI_MAKEFILES'"
    ci_info "environment option: \$CI_MAKE: '$CI_MAKE'"
    ci_info "environment option: \$CI_MAKE_FLAGS: '$CI_MAKE_FLAGS'"
    ci_info "environment option: \$CI_MAKE_VARS: '$CI_MAKE_VARS'"
    ci_info "environment option: \$CI_CC: '$CI_CC'"
    ci_info "environment option: \$CI_CC_FLAGS: '$CI_CC_FLAGS'"
    ci_info "environment option: \$CI_CPP: '$CI_CPP'"
    ci_info "environment option: \$CI_CPP_FLAGS: '$CI_CPP_FLAGS'"
    ci_info "environment option: \$CI_AR: '$CI_AR'"
    ci_info "environment option: \$CI_RANLIB: '$CI_RANLIB'"
    ci_info "environment option: \$CI_LD: '$CI_LD'"
    ci_info "environment option: \$CI_LD_FLAGS: '$CI_LD_FLAGS'"
    ci_info "environment option: \$CI_LIBS: '$CI_LIBS'"
    ci_info "environment option: \$CI_SANITIZERS: '$CI_SANITIZERS'"
    ci_info "environment option: \$CI_FORCE: '$CI_FORCE'"
    ci_info "environment option: \$CI_NO_BUILD: '$CI_NO_BUILD'"
    ci_info "environment option: \$CI_NO_TEST: '$CI_NO_TEST'"
    ci_info "environment option: \$CI_NO_CLEAN: '$CI_NO_CLEAN'"
    ci_info "executable: \$CI_MAKE: $(command -V "$CI_MAKE")"
    [[ $CI_CC ]] && {
        ci_info "executable: \$CI_CC: $(command -V "$CI_CC")"
    }
    [[ $CI_CPP ]] && {
        ci_info "executable: \$CI_CPP: $(command -V "$CI_CPP")"
    }
    [[ $CI_AR ]] && {
        ci_info "executable: \$CI_AR: $(command -V "$CI_AR")"
    }
    [[ $CI_RANLIB ]] && {
        ci_info "executable: \$CI_RANLIB: $(command -V "$CI_RANLIB")"
    }
    [[ $CI_LD ]] && {
        ci_info "executable: \$CI_LD: $(command -V "$CI_LD")"
    }
    ci_info "## END OF CONFIGURATION ##"
}

function ci_cleanup_old_build {
    # Any old makefile-based build will most likely leave a mess
    # of object files behind if interrupted, e.g., via Ctrl+C.
    # There may be other files behind, depending on what makefile
    # had been used. We cannot easily enumerate all of those.
    # Fortunately, for a clean makefiles-based build, it should be
    # sufficient to remove the old object files only.
    ci_info "## START OF PRE-BUILD CLEANUP ##"
    local find_args=(-maxdepth 1 \( -iname "*.o" -o -iname "*.obj" \))
    [[ ! $(find "$CI_SRC_DIR" "${find_args[@]}" | head -n1) ]] || {
        ci_warn "unexpected build found in '$CI_SRC_DIR'"
        if ci_expr $((CI_FORCE))
        then
            # Delete the old build.
            local my_file
            find "$CI_SRC_DIR" "${find_args[@]}" |
                while IFS="" read -r my_file
                do
                    ci_spawn rm -fr "$my_file"
                done
            ci_info "## END OF PRE-BUILD CLEANUP ##"
        else
            # Alert the user, but do not delete their existing files,
            # and do not mess up their existing build.
            ci_info "hint: consider using the option \$CI_FORCE=1"
            ci_err "unable to continue"
        fi
    }
}

function ci_build {
    ci_info "## START OF BUILD ##"
    # Initialize and populate the local arrays.
    local all_make_flags=()
    local all_make_vars=()
    [[ $CI_MAKE_FLAGS ]] && {
        all_make_flags+=($CI_MAKE_FLAGS)
    }
    [[ $CI_CC ]] && {
        all_make_vars+=("CC=$CI_CC")
    }
    [[ $CI_CC_FLAGS || $CI_SANITIZERS ]] && {
        [[ $CI_SANITIZERS ]] && CI_CC_FLAGS="${CI_CC_FLAGS:-"-O2"} -fsanitize=$CI_SANITIZERS"
        all_make_vars+=("CFLAGS=$CI_CC_FLAGS")
    }
    [[ $CI_CPP ]] && {
        all_make_vars+=("CPP=$CI_CPP")
    }
    [[ $CI_CPP_FLAGS ]] && {
        all_make_vars+=("CPPFLAGS=$CI_CPP_FLAGS")
    }
    [[ $CI_AR ]] && {
        all_make_vars+=("AR=$CI_AR")
    }
    [[ $CI_RANLIB ]] && {
        all_make_vars+=("RANLIB=$CI_RANLIB")
    }
    [[ $CI_LD ]] && {
        all_make_vars+=("LD=$CI_LD")
    }
    [[ $CI_LD_FLAGS || $CI_SANITIZERS ]] && {
        [[ $CI_SANITIZERS ]] && CI_LD_FLAGS+="${CI_LD_FLAGS:+" "}-fsanitize=$CI_SANITIZERS"
        all_make_vars+=("LDFLAGS=$CI_LD_FLAGS")
    }
    [[ $CI_LIBS ]] && {
        all_make_vars+=("LIBS=$CI_LIBS")
    }
    all_make_vars+=($CI_MAKE_VARS)
    # And... build!
    local my_makefile
    for my_makefile in $CI_MAKEFILES
    do
        ci_info "using makefile: $my_makefile"
        ci_expr $((CI_NO_BUILD)) || {
            # Spawn "make".
            ci_spawn "$CI_MAKE" -f "$my_makefile" \
                                "${all_make_flags[@]}" \
                                "${all_make_vars[@]}"
        }
        ci_expr $((CI_NO_TEST)) || {
            # Spawn "make test" if testing is not disabled.
            ci_spawn "$CI_MAKE" -f "$my_makefile" \
                                "${all_make_flags[@]}" \
                                "${all_make_vars[@]}" \
                                test
        }
        ci_expr $((CI_NO_CLEAN)) || {
            # Spawn "make clean" if cleaning is not disabled.
            ci_spawn "$CI_MAKE" -f "$my_makefile" \
                                "${all_make_flags[@]}" \
                                "${all_make_vars[@]}" \
                                clean
        }
    done
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
