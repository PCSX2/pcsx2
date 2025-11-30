CMAKE_MINIMUM_REQUIRED(VERSION 3.18 FATAL_ERROR)

PROJECT(googletest-download NONE)

INCLUDE(ExternalProject)
ExternalProject_Add(googletest
	URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
	URL_HASH SHA256=40d4ec942217dcc84a9ebe2a68584ada7d4a33a8ee958755763278ea1c5e18ff
	SOURCE_DIR "${CONFU_DEPENDENCIES_SOURCE_DIR}/googletest"
	BINARY_DIR "${CONFU_DEPENDENCIES_BINARY_DIR}/googletest"
	CONFIGURE_COMMAND ""
	BUILD_COMMAND ""
	INSTALL_COMMAND ""
	TEST_COMMAND ""
)
