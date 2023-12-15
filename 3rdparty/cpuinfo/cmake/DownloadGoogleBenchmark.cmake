CMAKE_MINIMUM_REQUIRED(VERSION 2.8.12 FATAL_ERROR)

PROJECT(googlebenchmark-download NONE)

INCLUDE(ExternalProject)
ExternalProject_Add(googlebenchmark
	URL https://github.com/google/benchmark/archive/v1.6.1.zip
	URL_HASH SHA256=367e963b8620080aff8c831e24751852cffd1f74ea40f25d9cc1b667a9dd5e45
	SOURCE_DIR "${CONFU_DEPENDENCIES_SOURCE_DIR}/googlebenchmark"
	BINARY_DIR "${CONFU_DEPENDENCIES_BINARY_DIR}/googlebenchmark"
	CONFIGURE_COMMAND ""
	BUILD_COMMAND ""
	INSTALL_COMMAND ""
	TEST_COMMAND ""
)
