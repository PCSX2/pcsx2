#!/bin/bash

set -x
# -e => need to handle empty $files variable

ret=0

if command -v clang-format-3.8 > /dev/null ; then
    clang_format=clang-format-3.8
else
    if command -v clang-format > /dev/null ; then
        clang_format=clang-format
    else
        return 2;
    fi
fi

$clang_format -version

# Doesn't work as travis only populate a single branch history

#branch=`git rev-parse --abbrev-ref HEAD`
#if [ x$branch = "xmaster" ]
#then
#    # check the last 20 commits. It ought to be enough even for big push
#    diff_range=HEAD~20
#else
#    # check filed updated in the branch
#    diff_range=master...HEAD
#fi

# Get the number of commits that share a linear history with the HEAD. Limit the value to 20
# Solution isn't perfect but it ough to be close enough of the current branch size
#
# Picking more commits might hurt during the conversion. When everything will be ready, we
# could get back to 20
br_commit=`git log --oneline --decorate --graph -n 20 | grep "^\* [[:alnum:]]" -c`
if [ $br_commit -lt 1 ]; then
    # Something got wrong
    diff_range=HEAD~20
else
    diff_range=HEAD~$br_commit
fi

# get updates and blacklist directories that don't use yet the clang-format syntax
files=`git diff --name-only --diff-filter=ACMRT $diff_range  -- $PWD | \
    grep "\.\(c\|h\|inl\|cpp\|hpp\)$" | \
    grep -v "${1}pcsx2/" | \
    \
    grep -v "/resource.h" | \
    grep -v "3rdparty/" | \
    grep -v "bin/" | \
    grep -v "cmake/" | \
    grep -v "tools/" | \
    grep -v "tests/"
`

# Check remaining files are clang-format compliant
for f in $files
do
    $clang_format -style=file -output-replacements-xml $f | grep "<replacement " >/dev/null
    if [ $? -ne 1 ]
    then
        echo "file $f did not match clang-format"
        ret=1;
    fi
done

exit $ret;
