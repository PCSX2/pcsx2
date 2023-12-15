# We need to use a custom CMakeLists for GoogleTest, because we can't compile with exceptions on.
set(GTEST_DIR "${CMAKE_SOURCE_DIR}/3rdparty/gtest/googletest")

set(SRCS
    "${GTEST_DIR}/src/gtest.cc"
    "${GTEST_DIR}/src/gtest-assertion-result.cc"
    "${GTEST_DIR}/src/gtest-death-test.cc"
    "${GTEST_DIR}/src/gtest-filepath.cc"
    "${GTEST_DIR}/src/gtest-matchers.cc"
    "${GTEST_DIR}/src/gtest-port.cc"
    "${GTEST_DIR}/src/gtest-printers.cc"
    "${GTEST_DIR}/src/gtest-test-part.cc"
    "${GTEST_DIR}/src/gtest-typed-test.cc"
)

set(MAIN_SRCS
    "${GTEST_DIR}/src/gtest_main.cc"
)

add_library(gtest ${SRCS})
target_include_directories(gtest PRIVATE "${GTEST_DIR}" "${GTEST_DIR}/include")
target_include_directories(gtest INTERFACE "${GTEST_DIR}/include")
target_link_libraries(gtest Threads::Threads)
set_property(TARGET gtest PROPERTY CXX_STANDARD 17)
set_property(TARGET gtest PROPERTY CXX_STANDARD_REQUIRED ON)

add_library(gtest_main "${MAIN_SRCS}")
target_include_directories(gtest_main PRIVATE "${GTEST_DIR}" "${GTEST_DIR}/include")
target_include_directories(gtest_main INTERFACE "${GTEST_DIR}/include")
target_link_libraries(gtest_main Threads::Threads)
set_property(TARGET gtest_main PROPERTY CXX_STANDARD 17)
set_property(TARGET gtest_main PROPERTY CXX_STANDARD_REQUIRED ON)

