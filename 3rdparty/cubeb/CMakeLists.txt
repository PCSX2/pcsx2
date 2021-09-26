# Disable building the stuff we don't need.
set(BUILD_SHARED_LIBS OFF)
set(BUILD_TESTS OFF)
set(BUILD_RUST_LIBS OFF)
set(BUILD_TOOLS OFF)
set(BUNDLE_SPEEX ON)
set(USE_SANITIZERS OFF)
set(LAZY_LOAD_LIBS ON)

add_subdirectory(cubeb)
