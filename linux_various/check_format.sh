#!/bin/sh

ret=0

branch=`git rev-parse --abbrev-ref HEAD`
if [ x$branch = "xmaster" ]
then
    # check the last 20 commits. It ought to be enough even for big push
    diff_range=HEAD~20
else
    # check filed updated in the branch
    diff_range="master...HEAD"
fi

# get updates and blacklist directories that don't use yet the clang-format syntax
files=`git diff $diff_range --name-only --diff-filter=ACMRT | \
    grep "\.\(c\|h\|inl\|cpp\|hpp\)$" | \
    grep -v "${1}common/" | \
    grep -v "${1}pcsx2/" | \
    grep -v "${1}plugins/cdvdGigaherz/" | \
    grep -v "${1}plugins/CDVDiso/" | \
    grep -v "${1}plugins/CDVDisoEFP/" | \
    grep -v "${1}plugins/CDVDlinuz/" | \
    grep -v "${1}plugins/CDVDnull/" | \
    grep -v "${1}plugins/CDVDolio/" | \
    grep -v "${1}plugins/CDVDpeops/" | \
    grep -v "${1}plugins/dev9ghzdrk/" | \
    grep -v "${1}plugins/dev9null/" | \
    grep -v "${1}plugins/FWnull/" | \
    grep -v "${1}plugins/GSdx/" | \
    grep -v "${1}plugins/GSdx_legacy/" | \
    grep -v "${1}plugins/GSnull/" | \
    grep -v "${1}plugins/LilyPad/" | \
    grep -v "${1}plugins/onepad/" | \
    grep -v "${1}plugins/PadNull/" | \
    grep -v "${1}plugins/PeopsSPU2/" | \
    grep -v "${1}plugins/SPU2null/" | \
    grep -v "${1}plugins/spu2-x/" | \
    grep -v "${1}plugins/SSSPSXPAD/" | \
    grep -v "${1}plugins/USBnull/" | \
    grep -v "${1}plugins/USBqemu/" | \
    grep -v "${1}plugins/xpad/" | \
    grep -v "${1}plugins/zerogs/" | \
    grep -v "${1}plugins/zerospu2/" | \
    grep -v "${1}plugins/zzogl-pg/" | \
    \
    grep -v "/resource.h" | \
    grep -v "3rdparty/" | \
    grep -v "bin/" | \
    grep -v "cmake/" | \
    grep -v "tools/" | \
    grep -v "tests/" | \
    grep -v "unfree/"
`

# Check remaining files are clang-format compliant
for f in $files
do
    clang-format -style=file -output-replacements-xml $f | grep "<replacement " >/dev/null
    if [ $? -ne 1 ]
    then
        echo "file $f did not match clang-format"
        ret=1;
    fi
done

exit $ret;
