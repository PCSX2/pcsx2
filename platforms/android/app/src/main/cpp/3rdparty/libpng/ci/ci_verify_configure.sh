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
CI_BUILD_DIR="$CI_OUT_DIR/ci_verify_configure.$CI_TARGET_SYSTEM.$CI_TARGET_ARCH.build"
CI_INSTALL_DIR="$CI_OUT_DIR/ci_verify_configure.$CI_TARGET_SYSTEM.$CI_TARGET_ARCH.install"

function ci_init_build {
    # Ensure that the mandatory variables are initialized.
    CI_MAKE="${CI_MAKE:-make}"
    [[ "$CI_TARGET_SYSTEM.$CI_TARGET_ARCH" != "$CI_BUILD_SYSTEM.$CI_BUILD_ARCH" ]] || {
        # For native builds, set CI_CC to "cc" by default if the cc command is available.
        # The configure script defaults CC to "gcc", which is not always a good idea.
        [[ -x $(command -v cc) ]] && CI_CC="${CI_CC:-cc}"
    }
    # Ensure that the CI_ variables that cannot be customized reliably are not initialized.
    [[ ! $CI_CONFIGURE_VARS ]] || {
        ci_err "unsupported: \$CI_CONFIGURE_VARS='$CI_CONFIGURE_VARS'"
    }
    [[ ! $CI_MAKE_VARS ]] || {
        ci_err "unsupported: \$CI_MAKE_VARS='$CI_MAKE_VARS'"
    }
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
    ci_info "environment option: \$CI_CONFIGURE_FLAGS: '$CI_CONFIGURE_FLAGS'"
    ci_info "environment option: \$CI_MAKE: '$CI_MAKE'"
    ci_info "environment option: \$CI_MAKE_FLAGS: '$CI_MAKE_FLAGS'"
    ci_info "environment option: \$CI_CC: '$CI_CC'"
    ci_info "environment option: \$CI_CC_FLAGS: '$CI_CC_FLAGS'"
    ci_info "environment option: \$CI_CPP: '$CI_CPP'"
    ci_info "environment option: \$CI_CPP_FLAGS: '$CI_CPP_FLAGS'"
    ci_info "environment option: \$CI_AR: '$CI_AR'"
    ci_info "environment option: \$CI_RANLIB: '$CI_RANLIB'"
    ci_info "environment option: \$CI_LD: '$CI_LD'"
    ci_info "environment option: \$CI_LD_FLAGS: '$CI_LD_FLAGS'"
    ci_info "environment option: \$CI_SANITIZERS: '$CI_SANITIZERS'"
    ci_info "environment option: \$CI_FORCE: '$CI_FORCE'"
    ci_info "environment option: \$CI_NO_BUILD: '$CI_NO_BUILD'"
    ci_info "environment option: \$CI_NO_TEST: '$CI_NO_TEST'"
    ci_info "environment option: \$CI_NO_INSTALL: '$CI_NO_INSTALL'"
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
    ci_info "## START OF PRE-BUILD CLEANUP ##"
    [[ ! -e $CI_BUILD_DIR && ! -e $CI_INSTALL_DIR ]] || {
        ci_spawn rm -fr "$CI_BUILD_DIR"
        ci_spawn rm -fr "$CI_INSTALL_DIR"
    }
    [[ ! -e "$CI_SRC_DIR/config.status" ]] || {
        ci_warn "unexpected build configuration file: '$CI_SRC_DIR/config.status'"
        if ci_expr $((CI_FORCE))
        then
            # Delete the old config and (possibly) the old build.
            ci_info "note: forcing an in-tree build cleanup"
            if [[ -f $CI_SRC_DIR/Makefile ]]
            then
                ci_spawn make -C "$CI_SRC_DIR" distclean
            else
                ci_spawn rm -fr "$CI_SRC_DIR"/config.{log,status}
            fi
        else
            # Alert the user, but do not delete their files.
            ci_warn "the configure script might fail"
            ci_info "hint: consider using the option \$CI_FORCE=1"
        fi
    }
    ci_info "## END OF PRE-BUILD CLEANUP ##"
}

function ci_build {
    ci_info "## START OF BUILD ##"
    # Export the configure build environment.
    [[ $CI_CC ]] && ci_spawn export CC="$CI_CC"
    [[ $CI_CC_FLAGS ]] && ci_spawn export CFLAGS="$CI_CC_FLAGS"
    [[ $CI_CPP ]] && ci_spawn export CPP="$CI_CPP"
    [[ $CI_CPP_FLAGS ]] && ci_spawn export CPPFLAGS="$CI_CPP_FLAGS"
    [[ $CI_AR ]] && ci_spawn export AR="$CI_AR"
    [[ $CI_RANLIB ]] && ci_spawn export RANLIB="$CI_RANLIB"
    [[ $CI_LD ]] && ci_spawn export LD="$CI_LD"
    [[ $CI_LD_FLAGS ]] && ci_spawn export LDFLAGS="$CI_LD_FLAGS"
    [[ $CI_SANITIZERS ]] && {
        ci_spawn export CFLAGS="${CFLAGS:-"-O2"} -fsanitize=$CI_SANITIZERS"
        ci_spawn export LDFLAGS="${LDFLAGS}${LDFLAGS:+" "}-fsanitize=$CI_SANITIZERS"
    }
    # Spawn "autogen.sh" if the configure script is not available.
    [[ -x "$CI_SRC_DIR/configure" ]] || {
        ci_spawn "$CI_SRC_DIR/autogen.sh"
    }
    # And... build!
    ci_spawn mkdir -p "$CI_BUILD_DIR"
    ci_spawn cd "$CI_BUILD_DIR"
    # Spawn "configure".
    ci_spawn "$CI_SRC_DIR/configure" --prefix="$CI_INSTALL_DIR" $CI_CONFIGURE_FLAGS
    ci_expr $((CI_NO_BUILD)) || {
        # Spawn "make".
        ci_spawn "$CI_MAKE" $CI_MAKE_FLAGS
    }
    ci_expr $((CI_NO_TEST)) || {
        # Spawn "make test" if testing is not disabled.
        ci_spawn "$CI_MAKE" $CI_MAKE_FLAGS test
    }
    ci_expr $((CI_NO_INSTALL)) || {
        # Spawn "make install" if installation is not disabled.
        ci_spawn "$CI_MAKE" $CI_MAKE_FLAGS install
    }
    ci_expr $((CI_NO_CLEAN)) || {
        # Spawn "make clean" and "make distclean" if cleaning is not disabled.
        ci_spawn "$CI_MAKE" $CI_MAKE_FLAGS clean
        ci_spawn "$CI_MAKE" $CI_MAKE_FLAGS distclean
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
