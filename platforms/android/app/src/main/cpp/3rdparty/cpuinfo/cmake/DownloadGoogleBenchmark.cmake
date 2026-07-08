CMAKE_MINIMUM_REQUIRED(VERSION 3.18 FATAL_ERROR)

PROJECT(googlebenchmark-download NONE)

INCLUDE(ExternalProject)
ExternalProject_Add(googlebenchmark
	URL https://github.com/google/benchmark/archive/refs/tags/v1.9.4.tar.gz
	URL_HASH SHA256=b334658edd35efcf06a99d9be21e4e93e092bd5f95074c1673d5c8705d95c104
	SOURCE_DIR "${CONFU_DEPENDENCIES_SOURCE_DIR}/googlebenchmark"
	BINARY_DIR "${CONFU_DEPENDENCIES_BINARY_DIR}/googlebenchmark"
	CONFIGURE_COMMAND ""
	BUILD_COMMAND ""
	INSTALL_COMMAND ""
	TEST_COMMAND ""
)
