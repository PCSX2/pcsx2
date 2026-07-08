# Copyright (c) 2019-2025 Cosmin Truta.
#
# Use, modification and distribution are subject to the MIT License.
# Please see the accompanying file LICENSE_MIT.txt
#
# SPDX-License-Identifier: MIT

test -f "$BASH_SOURCE" || {
    echo >&2 "warning: this module requires Bash version 3 or newer"
}
test "${#BASH_SOURCE[@]}" -gt 1 || {
    echo >&2 "warning: this module should be sourced from a Bash script"
}

# Reset the locale to avoid surprises from locale-dependent commands.
export LC_ALL=C
export LANG=C
export LANGUAGE=C

# Reset CDPATH to avoid surprises from the "cd" command.
export CDPATH=""

# Initialize the global constants CI_SCRIPT_{NAME,DIR} and CI_TOPLEVEL_DIR.
CI_SCRIPT_NAME="$(basename -- "$0")"
CI_SCRIPT_DIR="$(cd "$(dirname -- "$0")" && pwd)"
CI_TOPLEVEL_DIR="$(cd "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"

# Initialize the global constants CI_BUILD_{...} for the host build platform.
CI_BUILD_ARCH="${CI_BUILD_ARCH:-"$(uname -m | tr 'A-Z/\.-' 'a-z____')"}"
CI_BUILD_SYSTEM="${CI_BUILD_SYSTEM:-"$(uname -s | tr 'A-Z/\.-' 'a-z____')"}"

# Initialize the global constants CI_TARGET_{...} for the target platform.
CI_TARGET_ARCH="${CI_TARGET_ARCH:-"$CI_BUILD_ARCH"}"
CI_TARGET_SYSTEM="${CI_TARGET_SYSTEM:-"$CI_BUILD_SYSTEM"}"

function ci_info {
    printf >&2 "%s: %s\\n" "$CI_SCRIPT_NAME" "$*"
}

function ci_warn {
    printf >&2 "%s: warning: %s\\n" "$CI_SCRIPT_NAME" "$*"
}

function ci_err {
    printf >&2 "%s: error: %s\\n" "$CI_SCRIPT_NAME" "$*"
    exit 2
}

function ci_err_internal {
    printf >&2 "%s: internal error: %s\\n" "$CI_SCRIPT_NAME" "$*"
    printf >&2 "ABORTED\\n"
    # Exit with the conventional SIGABRT code.
    exit 134
}

function ci_expr {
    if [[ ${*:-0} == [0-9] ]]
    then
        # This is the same as in the else-branch below, albeit much faster
        # for our intended use cases.
        return $((!$1))
    else
        # The funny-looking compound command "... && return $? || return $?"
        # allows the execution to continue uninterrupted under "set -e".
        expr >/dev/null "$@" && return $? || return $?
    fi
}

function ci_spawn {
    printf >&2 "%s: executing:" "$CI_SCRIPT_NAME"
    printf >&2 " %q" "$@"
    printf >&2 "\\n"
    "$@"
}

# Ensure that the internal initialization is correct.
[[ $CI_TOPLEVEL_DIR/ci/lib/ci.lib.sh -ef ${BASH_SOURCE[0]} ]] || {
    ci_err_internal "bad or missing \$CI_TOPLEVEL_DIR"
}
[[ $CI_SCRIPT_DIR/$CI_SCRIPT_NAME -ef $0 ]] || {
    ci_err_internal "bad or missing \$CI_SCRIPT_DIR/\$CI_SCRIPT_NAME"
}
[[ $CI_BUILD_ARCH && $CI_BUILD_SYSTEM ]] || {
    ci_err_internal "missing \$CI_BUILD_ARCH or \$CI_BUILD_SYSTEM"
}
[[ $CI_TARGET_ARCH && $CI_TARGET_SYSTEM ]] || {
    ci_err_internal "missing \$CI_TARGET_ARCH or \$CI_TARGET_SYSTEM"
}

# Ensure that the user initialization is correct.
[[ ${CI_FORCE:-0} == [01] ]] || {
    ci_err "bad boolean option: \$CI_FORCE: '$CI_FORCE'"
}
[[ ${CI_NO_BUILD:-0} == [01] ]] || {
    ci_err "bad boolean option: \$CI_NO_BUILD: '$CI_NO_BUILD'"
}
[[ ${CI_NO_TEST:-0} == [01] ]] || {
    ci_err "bad boolean option: \$CI_NO_TEST: '$CI_NO_TEST'"
}
[[ ${CI_NO_INSTALL:-0} == [01] ]] || {
    ci_err "bad boolean option: \$CI_NO_INSTALL: '$CI_NO_INSTALL'"
}
[[ ${CI_NO_CLEAN:-0} == [01] ]] || {
    ci_err "bad boolean option: \$CI_NO_CLEAN: '$CI_NO_CLEAN'"
}
if ci_expr $((CI_NO_BUILD))
then
    ci_expr $((CI_NO_TEST && CI_NO_INSTALL)) || {
        ci_err "\$CI_NO_BUILD requires \$CI_NO_TEST and \$CI_NO_INSTALL"
    }
fi
