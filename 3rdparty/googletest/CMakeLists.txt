# We need to use a custom CMakeLists for GoogleTest, because we can't compile with exceptions on.

add_library(gtest
	googletest/src/gtest-all.cc
	googlemock/src/gmock-all.cc
	googlemock/src/gmock_main.cc
)
target_include_directories(gtest PUBLIC
	googletest/include
	googlemock/include
)
target_include_directories(gtest PRIVATE
	googletest
	googlemock
)
target_link_libraries(gtest Threads::Threads)
set_property(TARGET gtest PROPERTY CXX_STANDARD 17)
set_property(TARGET gtest PROPERTY CXX_STANDARD_REQUIRED ON)

