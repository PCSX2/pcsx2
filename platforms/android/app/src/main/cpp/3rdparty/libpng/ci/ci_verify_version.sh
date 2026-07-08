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

# Declare the global environments collected from various sources.
declare CI_ENV_LIBPNG_VER        # collected from png.h
declare CI_ENV_AUTOCONF_VER      # collected from configure.ac
declare CI_ENV_CMAKE_VER         # collected from CMakeLists.txt
declare CI_ENV_LIBPNGCONFIG_VER  # collected from scripts/libpng-config-head.in

function ci_run_shellify {
    local my_script my_result
    my_script="$CI_SCRIPT_DIR/libexec/ci_shellify_${1#--}.sh"
    shift 1
    [[ -f $my_script ]] || {
        ci_err_internal "missing script: '$my_script'"
    }
    ci_info "shellifying:" "$@"
    "$BASH" "$my_script" "$@"
    echo "$my_result" | "$BASH" --posix || ci_err "bad shellify output"
    echo "$my_result"
}

function ci_init_version_verification {
    ci_info "## START OF VERIFICATION ##"
    CI_ENV_LIBPNG_VER="$(ci_run_shellify --c png.h)"
    echo "$CI_ENV_LIBPNG_VER"
    CI_ENV_AUTOCONF_VER="$(ci_run_shellify --autoconf configure.ac)"
    echo "$CI_ENV_AUTOCONF_VER"
    CI_ENV_CMAKE_VER="$(ci_run_shellify --cmake CMakeLists.txt)"
    echo "$CI_ENV_CMAKE_VER"
    CI_ENV_LIBPNGCONFIG_VER="$(ci_run_shellify --shell scripts/libpng-config-head.in)"
    echo "$CI_ENV_LIBPNGCONFIG_VER"
}

# shellcheck disable=SC2154
function ci_do_version_verification {
    local my_expect
    ci_info "## VERIFYING: version definitions in 'png.h' ##"
    eval "$CI_ENV_LIBPNG_VER"
    my_expect="${PNG_LIBPNG_VER_MAJOR}.${PNG_LIBPNG_VER_MINOR}.${PNG_LIBPNG_VER_RELEASE}"
    if [[ "$PNG_LIBPNG_VER_STRING" == "$my_expect"* ]]
    then
        ci_info "matched: \$PNG_LIBPNG_VER_STRING == $my_expect*"
    else
        ci_err "mismatched: \$PNG_LIBPNG_VER_STRING != $my_expect*"
    fi
    my_expect=$((PNG_LIBPNG_VER_MAJOR*10000 + PNG_LIBPNG_VER_MINOR*100 + PNG_LIBPNG_VER_RELEASE))
    if [[ "$PNG_LIBPNG_VER" == "$my_expect" ]]
    then
        ci_info "matched: \$PNG_LIBPNG_VER == $my_expect"
    else
        ci_err "mismatched: \$PNG_LIBPNG_VER != $my_expect"
    fi
    my_expect=$((PNG_LIBPNG_VER_MAJOR*10 + PNG_LIBPNG_VER_MINOR))
    if [[ "$PNG_LIBPNG_VER_SHAREDLIB" == "$my_expect" ]]
    then
        ci_info "matched: \$PNG_LIBPNG_VER_SHAREDLIB == $my_expect"
    else
        ci_err "mismatched: \$PNG_LIBPNG_VER_SHAREDLIB != $my_expect"
    fi
    if [[ "$PNG_LIBPNG_VER_SONUM" == "$my_expect" ]]
    then
        ci_info "matched: \$PNG_LIBPNG_VER_SONUM == $my_expect"
    else
        ci_err "mismatched: \$PNG_LIBPNG_VER_SONUM != $my_expect"
    fi
    if [[ "$PNG_LIBPNG_VER_DLLNUM" == "$my_expect" ]]
    then
        ci_info "matched: \$PNG_LIBPNG_VER_DLLNUM == $my_expect"
    else
        ci_err "mismatched: \$PNG_LIBPNG_VER_DLLNUM != $my_expect"
    fi
    if [[ "$PNG_LIBPNG_VER_BUILD" == [01] ]]
    then
        ci_info "matched: \$PNG_LIBPNG_VER_BUILD == [01]"
    else
        ci_err "mismatched: \$PNG_LIBPNG_VER_BUILD != [01]"
    fi
    ci_info "## VERIFYING: build definitions in 'png.h' ##"
    my_expect="${PNG_LIBPNG_VER_MAJOR}.${PNG_LIBPNG_VER_MINOR}.${PNG_LIBPNG_VER_RELEASE}"
    if [[ "$PNG_LIBPNG_VER_STRING" == "$my_expect" ]]
    then
        if [[ $PNG_LIBPNG_VER_BUILD -eq 0 ]]
        then
            ci_info "matched: \$PNG_LIBPNG_VER_BUILD -eq 0"
        else
            ci_err "mismatched: \$PNG_LIBPNG_VER_BUILD -ne 0"
        fi
        if [[ $PNG_LIBPNG_BUILD_BASE_TYPE -eq $PNG_LIBPNG_BUILD_STABLE ]]
        then
            ci_info "matched: \$PNG_LIBPNG_BUILD_BASE_TYPE -eq \$PNG_LIBPNG_BUILD_STABLE"
        else
            ci_err "mismatched: \$PNG_LIBPNG_BUILD_BASE_TYPE -ne \$PNG_LIBPNG_BUILD_STABLE"
        fi
    elif [[ "$PNG_LIBPNG_VER_STRING" == "$my_expect".git ]]
    then
        if [[ $PNG_LIBPNG_VER_BUILD -ne 0 ]]
        then
            ci_info "matched: \$PNG_LIBPNG_VER_BUILD -ne 0"
        else
            ci_err "mismatched: \$PNG_LIBPNG_VER_BUILD -eq 0"
        fi
        if [[ $PNG_LIBPNG_BUILD_BASE_TYPE -ne $PNG_LIBPNG_BUILD_STABLE ]]
        then
            ci_info "matched: \$PNG_LIBPNG_BUILD_BASE_TYPE -ne \$PNG_LIBPNG_BUILD_STABLE"
        else
            ci_err "mismatched: \$PNG_LIBPNG_BUILD_BASE_TYPE -eq \$PNG_LIBPNG_BUILD_STABLE"
        fi
    else
        ci_err "unexpected: \$PNG_LIBPNG_VER_STRING == '$PNG_LIBPNG_VER_STRING'"
    fi
    ci_info "## VERIFYING: type definitions in 'png.h' ##"
    my_expect="$(echo "png_libpng_version_${PNG_LIBPNG_VER_STRING}" | tr . _)"
    ci_spawn grep -w -e "$my_expect" png.h
    ci_info "## VERIFYING: version definitions in 'configure.ac' ##"
    eval "$CI_ENV_AUTOCONF_VER"
    if [[ "$PNGLIB_VERSION" == "$PNG_LIBPNG_VER_STRING" ]]
    then
        ci_info "matched: \$PNGLIB_VERSION == \$PNG_LIBPNG_VER_STRING"
    else
        ci_err "mismatched: \$PNGLIB_VERSION != \$PNG_LIBPNG_VER_STRING"
    fi
    ci_info "## VERIFYING: version definitions in 'CMakeLists.txt' ##"
    eval "$CI_ENV_CMAKE_VER"
    if [[ "$PNGLIB_VERSION" == "$PNG_LIBPNG_VER_STRING" && "$PNGLIB_SUBREVISION" == 0 ]]
    then
        ci_info "matched: \$PNGLIB_VERSION == \$PNG_LIBPNG_VER_STRING"
        ci_info "matched: \$PNGLIB_SUBREVISION == 0"
    elif [[ "$PNGLIB_VERSION.$PNGLIB_SUBREVISION" == "$PNG_LIBPNG_VER_STRING" ]]
    then
        ci_info "matched: \$PNGLIB_VERSION.\$PNGLIB_SUBREVISION == \$PNG_LIBPNG_VER_STRING"
    else
        ci_err "mismatched: \$PNGLIB_VERSION != \$PNG_LIBPNG_VER_STRING"
    fi
    ci_info "## VERIFYING: version definitions in 'scripts/libpng-config-head.in' ##"
    eval "$CI_ENV_LIBPNGCONFIG_VER"
    if [[ "$version" == "$PNG_LIBPNG_VER_STRING" ]]
    then
        ci_info "matched: \$version == \$PNG_LIBPNG_VER_STRING"
    else
        ci_err "mismatched: \$version != \$PNG_LIBPNG_VER_STRING"
    fi
}

function ci_finish_version_verification {
    ci_info "## END OF VERIFICATION ##"
    # Relying on "set -o errexit" to not reach here in case of error.
    ci_info "## SUCCESS ##"
}

function ci_verify_version {
    ci_init_version_verification
    ci_do_version_verification
    ci_finish_version_verification
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
    [[ $# -eq 0 ]] || {
        echo >&2 "error: unexpected argument: '$1'"
        usage 2
    }
    # And... go!
    ci_verify_version
}

main "$@"
