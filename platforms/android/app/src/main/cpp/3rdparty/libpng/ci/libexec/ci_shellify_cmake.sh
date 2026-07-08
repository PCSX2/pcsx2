#!/usr/bin/env bash
set -o errexit -o pipefail -o posix

# Copyright (c) 2019-2025 Cosmin Truta.
#
# Use, modification and distribution are subject to the MIT License.
# Please see the accompanying file LICENSE_MIT.txt
#
# SPDX-License-Identifier: MIT

# shellcheck source=ci/lib/ci.lib.sh
source "$(dirname "$0")/../lib/ci.lib.sh"

function ci_shellify_cmake {
    # Convert CMake lists text, specifically originating
    # from CMakeLists.txt, to shell scripting text.
    # Select only the easy-to-parse definitions of PNGLIB_*.
    sed -n -e '/^ *set *(PNGLIB_[^ ]* * [$"0-9A-Za-z_].*)/ p' |
        sed -e 's/^ *set *(PNG\([^ ]*\) * \([^() ]*\)).*$/PNG\1=\2/' \
            -e 's/^\([^ ]*=[^ ]*\).*$/export \1;/'
}

function usage {
    echo "usage: $CI_SCRIPT_NAME [<options>] CMakeLists.txt"
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
    [[ $# -eq 0 ]] && usage 2
    [[ $# -eq 1 ]] || ci_err "too many operands"
    # And... go!
    test -e "$1" || ci_err "no such file: '$1'"
    filename="$(basename -- "$1")"
    [[ $filename == [Cc][Mm]ake[Ll]ists.txt ]] || {
        ci_err "incorrect operand: '$1' (expecting: 'CMakeLists.txt')"
    }
    ci_shellify_cmake <"$1"
}

main "$@"
