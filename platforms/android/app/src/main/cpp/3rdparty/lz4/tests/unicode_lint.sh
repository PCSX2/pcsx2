#!/bin/bash

# `unicode_lint.sh' determines whether source files under the ./lib/, ./tests/ and ./programs/ directories
# contain Unicode characters, and fails if any do.
#
# See https://github.com/lz4/lz4/issues/1018

echo "Ensure no unicode character is present in source files *.{c,h}"
pass=true

# Scan ./lib/ for Unicode in source (*.c, *.h) files
echo "Scanning lib/"
result=$(
	find ./lib/ -regex '.*\.\(c\|h\)$' -exec grep -P -n "[^\x00-\x7F]" {} \; -exec echo "{}: FAIL" \;
)
if [[ $result ]]; then
	echo "$result"
	pass=false
fi

# Scan ./programs/ for Unicode in source (*.c, *.h) files
echo "Scanning programs/"
result=$(
	find ./programs/ -regex '.*\.\(c\|h\)$' -exec grep -P -n "[^\x00-\x7F]" {} \; -exec echo "{}: FAIL" \;
)
if [[ $result ]]; then
	echo "$result"
	pass=false
fi

# Scan ./tests/ for Unicode in source (*.c, *.h) files
echo "Scanning tests/"
result=$(
	find ./tests/ -regex '.*\.\(c\|h\)$' -exec grep -P -n "[^\x00-\x7F]" {} \; -exec echo "{}: FAIL" \;
)
if [[ $result ]]; then
	echo "$result"
	pass=false
fi

if [ "$pass" = true ]; then
	echo "All tests successful: no unicode character detected"
	echo "Result: PASS"
	exit 0
else
	echo "Result: FAIL"
	exit 1
fi
