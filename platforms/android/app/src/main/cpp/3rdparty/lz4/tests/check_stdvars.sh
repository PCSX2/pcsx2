#!/usr/bin/env sh
set -e   # exit on first command error

###############################################################################
#  check_stdvars.sh  –  verify that user-supplied CC/CFLAGS/CPPFLAGS/LDFLAGS/
#  LDLIBS reach every compile- and link-command emitted by a recursive Make.
#
#  Usage:  tests/check_stdvars.sh build.log
#   where build.log is produced with:
#     make -nr V=1 \
#          CC='cc -DCC_TEST' \
#          CFLAGS='-DCFLAGS_TEST' \
#          CPPFLAGS='-DCPPFLAGS_TEST' \
#          LDFLAGS='-DLDFLAGS_TEST' \
#          LDLIBS='-DLDLIBS_TEST' \
#          > build.log
###############################################################################

log_file=${1:?need build-log file}

# marker flags injected by the Makefile test target
CC_TAG='-DCC_TEST'
CF_TAG='-DCFLAGS_TEST'
CP_TAG='-DCPPFLAGS_TEST'
LD_TAG='-DLDFLAGS_TEST'
LL_TAG='-DLDLIBS_TEST'

compile_seen=0 ; compile_ok=0
link_seen=0    ; link_ok=0
fail=0

report() {
    echo "✖ $1:" >&2
    echo "  $2"   >&2
    fail=1
}

# read log line-by-line
while IFS= read -r line
do
    # skip blank lines, comments, or progress "echo …" lines
    [ -z "$line" ] && continue
    case "$line" in
        \#*)             continue ;;
        ' echo '*)       continue ;; # leading whitespace + echo
        'echo '*)        continue ;; # echo with no leading whitespace
    esac

    # classify
    step=other
    case "$line" in
        *' -c '*) step=compile ;;               # compile if -c appears
        *' cc '*|cc\ *|*' gcc '*|gcc\ *|*' clang '*|clang\ *|*' g++ '*|g++\ *|*' clang++ '*|clang++\ *)
            step=link ;;                        # compiler without -c ⇒ link
    esac
    [ "$step" = other ] && continue

    if [ "$step" = compile ]; then
        compile_seen=$((compile_seen + 1))
        ok=1
        case "$line" in *"$CC_TAG"*) : ;; *) ok=0 ;; esac
        case "$line" in *"$CF_TAG"*) : ;; *) ok=0 ;; esac
        case "$line" in *"$CP_TAG"*) : ;; *) ok=0 ;; esac
        if [ "$ok" -eq 1 ]; then
            compile_ok=$((compile_ok + 1))
        else
            report "compile cmd missing marker(s)" "$line"
        fi
    else    # link
        link_seen=$((link_seen + 1))
        ok=1
        case "$line" in *"$LD_TAG"*) : ;; *) ok=0 ;; esac
        case "$line" in *"$LL_TAG"*) : ;; *) ok=0 ;; esac
        if [ "$ok" -eq 1 ]; then
            link_ok=$((link_ok + 1))
        else
            report "link cmd missing marker(s)" "$line"
        fi
    fi
done < "$log_file"

if [ "$fail" -ne 0 ]; then
    echo "standard-variable propagation test FAILED" >&2
    echo "  compile: $compile_ok / $compile_seen OK" >&2
    echo "  link   : $link_ok / $link_seen OK"       >&2
    exit 1
fi

echo "✓ $compile_seen compile + $link_seen link commands: all markers present"
exit 0