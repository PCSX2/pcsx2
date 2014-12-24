# Tell cmake we are cross compiling and targeting darwin
set(CMAKE_SYSTEM_NAME Darwin)
set(CMAKE_SYSTEM_PROCESSOR i686)

# Use clang and target i686-apple-darwin.
set(CMAKE_C_COMPILER clang -m32)
set(CMAKE_C_COMPILER_TARGET i686-apple-darwin)
set(CMAKE_CXX_COMPILER clang++ -m32)
set(CMAKE_CXX_COMPILER_TARGET i686-apple-darwin)

# Enable clang
set(USE_CLANG TRUE)

# If given a CMAKE_FIND_ROOT_PATH then
# FIND_PROGRAM ignores CMAKE_FIND_ROOT_PATH (probably can't run)
# FIND_{LIBRARY,INCLUDE,PACKAGE} only uses the files in CMAKE_FIND_ROOT_PATH.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
