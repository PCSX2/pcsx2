cmake_minimum_required(VERSION 3.5...4.0.2)

project(cmake_install_test LANGUAGES C)

# Find LZ4 installation in Config mode
find_package(lz4 CONFIG REQUIRED)

# Verify that the universal target always exists
if (NOT TARGET LZ4::lz4)
  message(FATAL_ERROR "LZ4::lz4 target does not exist!")
endif()

# Verify that the location property is set
get_property(LZ4_LOCATION TARGET LZ4::lz4_shared PROPERTY LOCATION)
if (NOT LZ4_LOCATION)
  message(FATAL_ERROR "Missing LOCATION property for LZ4::lz4_shared!")
endif()

# Verify that the include directory property is set
get_property(LZ4_SHARED_INCLUDE_DIRECTORIES TARGET LZ4::lz4_shared PROPERTY INTERFACE_INCLUDE_DIRECTORIES)
if (NOT LZ4_SHARED_INCLUDE_DIRECTORIES)
  message(FATAL_ERROR "Missing INTERFACE_INCLUDE_DIRECTORIES property for LZ4::lz4_shared!")
endif()

# Add a test that builds against the installation
set(LZ4_TESTS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")
add_executable(decompress-partial "${LZ4_TESTS_SOURCE_DIR}/decompress-partial.c")
target_link_libraries(decompress-partial LZ4::lz4_shared)

# Add a test that builds against the lz4::lz4 target
set(LZ4_TESTS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")
add_executable(decompress-partial-lz4 "${LZ4_TESTS_SOURCE_DIR}/decompress-partial.c")
target_link_libraries(decompress-partial-lz4 lz4::lz4)
