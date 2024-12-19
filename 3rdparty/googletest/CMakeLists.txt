# We need to use a custom CMakeLists for GoogleTest, because we can't compile with exceptions on.

add_library(gtest
	googletest/src/gtest.cc
	googletest/src/gtest-assertion-result.cc
	googletest/src/gtest-death-test.cc
	googletest/src/gtest-filepath.cc
	googletest/src/gtest-matchers.cc
	googletest/src/gtest-port.cc
	googletest/src/gtest-printers.cc
	googletest/src/gtest-test-part.cc
	googletest/src/gtest-typed-test.cc
)
target_include_directories(gtest PRIVATE
	googletest
	googletest/include
)
target_include_directories(gtest INTERFACE
	googletest/include
)
target_link_libraries(gtest Threads::Threads)
set_property(TARGET gtest PROPERTY CXX_STANDARD 17)
set_property(TARGET gtest PROPERTY CXX_STANDARD_REQUIRED ON)

add_library(gtest_main
	googletest/src/gtest_main.cc
)
target_include_directories(gtest_main PRIVATE
	googletest
	googletest/include
)
target_include_directories(gtest_main INTERFACE
	googletest/include
)
target_link_libraries(gtest_main Threads::Threads)
set_property(TARGET gtest_main PROPERTY CXX_STANDARD 17)
set_property(TARGET gtest_main PROPERTY CXX_STANDARD_REQUIRED ON)

