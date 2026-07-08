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

# Initialize the global constants CI_{...}{CHECK,CHECKER,LINT}.
CI_SHELLCHECK="${CI_SHELLCHECK:-shellcheck}"
CI_EDITORCONFIG_CHECKER="${CI_EDITORCONFIG_CHECKER:-editorconfig-checker}"
CI_YAMLLINT="${CI_YAMLLINT:-yamllint}"

# Initialize the global lint status.
CI_LINT_STATUS=0

function ci_init_lint {
    ci_info "## START OF LINTING ##"
    local my_program
    # Complete the initialization of CI_SHELLCHECK.
    # Set it to the empty string if the shellcheck program is unavailable.
    my_program="$(command -v "$CI_SHELLCHECK")" || {
        ci_warn "program not found: '$CI_SHELLCHECK'"
    }
    CI_SHELLCHECK="$my_program"
    # Complete the initialization of CI_EDITORCONFIG_CHECKER.
    # Set it to the empty string if the editorconfig-checker program is unavailable.
    my_program="$(command -v "$CI_EDITORCONFIG_CHECKER")" || {
        ci_warn "program not found: '$CI_EDITORCONFIG_CHECKER'"
    }
    CI_EDITORCONFIG_CHECKER="$my_program"
    # Complete the initialization of CI_YAMLLINT.
    # Set it to the empty string if the yamllint program is unavailable.
    my_program="$(command -v "$CI_YAMLLINT")" || {
        ci_warn "program not found: '$CI_YAMLLINT'"
    }
    CI_YAMLLINT="$my_program"
}

function ci_finish_lint {
    ci_info "## END OF LINTING ##"
    if [[ $CI_LINT_STATUS -eq 0 ]]
    then
        ci_info "## SUCCESS ##"
    else
        ci_info "linting failed"
    fi
    return "$CI_LINT_STATUS"
}

function ci_lint_ci_scripts {
    [[ -x $CI_SHELLCHECK ]] || {
        ci_warn "## NOT LINTING: CI scripts ##"
        return 0
    }
    ci_info "## LINTING: CI scripts ##"
    ci_spawn "$CI_SHELLCHECK" --version
    find ./ci -name "ci_*.sh" -not -name "ci_env.*.sh" | {
        local my_file
        while IFS="" read -r my_file
        do
            ci_spawn "$CI_SHELLCHECK" -x "$my_file" || {
                # Linting failed.
                return 1
            }
        done
    }
}

function ci_lint_text_files {
    [[ -x $CI_EDITORCONFIG_CHECKER ]] || {
        ci_warn "## NOT LINTING: text files ##"
        return 0
    }
    ci_info "## LINTING: text files ##"
    ci_spawn "$CI_EDITORCONFIG_CHECKER" --version
    ci_spawn "$CI_EDITORCONFIG_CHECKER" --config .editorconfig-checker.json || {
        # Linting failed.
        return 1
    }
}

function ci_lint_yaml_files {
    [[ -x $CI_YAMLLINT ]] || {
        ci_warn "## NOT LINTING: YAML files ##"
        return 0
    }
    ci_info "## LINTING: YAML files ##"
    ci_spawn "$CI_YAMLLINT" --version
    # Considering that the YAML format is an extension of the JSON format,
    # we can lint both the YAML files and the plain JSON files here.
    find . \( -iname "*.yml" -o -iname "*.yaml" -o -iname "*.json" \) -not -path "./out/*" | {
        local my_file
        while IFS="" read -r my_file
        do
            ci_spawn "$CI_YAMLLINT" --strict "$my_file" || {
                # Linting failed.
                return 1
            }
        done
    }
}

function ci_lint {
    ci_init_lint
    ci_lint_ci_scripts || CI_LINT_STATUS=1
    ci_lint_text_files || CI_LINT_STATUS=1
    ci_lint_yaml_files || CI_LINT_STATUS=1
    # TODO: ci_lint_png_files, etc.
    ci_finish_lint
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
    ci_lint
}

main "$@"
