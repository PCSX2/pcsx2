# Tell cmake we are cross compiling and targeting darwin
set(CMAKE_SYSTEM_NAME Darwin)
set(CMAKE_SYSTEM_PROCESSOR i686)

# Leave it generic since it could be clang, gnu, etc.
if("$ENV{CC}" STREQUAL "" OR "$ENV{CXX}" STREQUAL "")
    set(CMAKE_C_COMPILER cc -m32)
    set(CMAKE_CXX_COMPILER c++ -m32)
endif()

# If given a CMAKE_FIND_ROOT_PATH then
# FIND_PROGRAM ignores CMAKE_FIND_ROOT_PATH (probably can't run)
# FIND_{LIBRARY,INCLUDE,PACKAGE} only uses the files in CMAKE_FIND_ROOT_PATH.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
