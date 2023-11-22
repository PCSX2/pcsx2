# This file is part of CMake-sanitizers.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#
#
# Copyright (c)
#   2013-2015 Matt Arsenault
#   2015      RWTH Aachen University, Federal Republic of Germany
#

# Function to add testcases.
function(add_testcase TESTNAME SOURCEFILES)
	# remove ${TESTNAME} from ${ARGV} to use ${ARGV} as ${SOURCEFILES}
	list(REMOVE_AT ARGV 0)

	# add a new executable
	add_executable(${TESTNAME} ${ARGV})

	# add a testcase for executable
	add_test(${TESTNAME} ${TESTNAME})
endfunction(add_testcase)

# Function to add testcases with asan enabled.
function(add_sanitized_testcase TESTNAME SOURCEFILES)
	add_testcase(${TESTNAME} ${SOURCEFILES})
	add_sanitizers(${TESTNAME})
endfunction(add_sanitized_testcase)



set(SANITIZE_ADDRESS TRUE)

#
# search for sanitizers
#
find_package(Sanitizers)


#
# add testcases
#
add_sanitized_testcase("asan_test_cpp" asan_test.cpp)
add_sanitized_testcase("shortest_ext_test_cpp" shortest.ext.test.cpp)

set_tests_properties(
	"asan_test_cpp"
	"shortest_ext_test_cpp"
PROPERTIES
	WILL_FAIL TRUE
)

