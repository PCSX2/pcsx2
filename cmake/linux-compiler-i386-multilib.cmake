# Tell cmake we are cross compiling and targeting linux
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR i686)

# It could be i?86-*linux-gnu, x86_64-*linux-gnu, x86_64-*linux-gnux32, etc.
# Leave it generic to only support amd64 or x32 to i386 with any compiler.
set(CMAKE_C_COMPILER cc -m32)
set(CMAKE_CXX_COMPILER c++ -m32)

# If given a CMAKE_FIND_ROOT_PATH then
# FIND_PROGRAM ignores CMAKE_FIND_ROOT_PATH (probably can't run)
# FIND_{LIBRARY,INCLUDE,PACKAGE} only uses the files in CMAKE_FIND_ROOT_PATH.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
