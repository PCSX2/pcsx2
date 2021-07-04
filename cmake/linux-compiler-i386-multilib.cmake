# Tell cmake we are cross compiling and targeting linux
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR i686)

# It could be i?86-*linux-gnu, x86_64-*linux-gnu, x86_64-*linux-gnux32, etc.
# Leave it generic to only support amd64 or x32 to i386 with any compiler.
if ("$ENV{CC}" STREQUAL "")
	set(CMAKE_C_COMPILER cc -m32)
elseif(NOT "$ENV{CC}" MATCHES "-m32")
	set(CMAKE_C_COMPILER $ENV{CC} -m32)
endif()
if ("$ENV{CXX}" STREQUAL "")
	set(CMAKE_CXX_COMPILER c++ -m32)
elseif(NOT "$ENV{CXX}" MATCHES "-m32")
	set(CMAKE_CXX_COMPILER $ENV{CXX} -m32)
endif()

# cmake 2.8.5 correctly sets CMAKE_LIBRARY_ARCHITECTURE for Debian multiarch.
# Be really strict about what gets used.
if(EXISTS /usr/lib/i386-linux-gnu)
	set(CMAKE_SYSTEM_IGNORE_PATH
		/lib             /lib64             /lib32
		/usr/lib         /usr/lib64         /usr/lib32
		/usr/local/lib   /usr/local/lib64   /usr/local/lib32)
	list(APPEND CMAKE_LIBRARY_PATH /usr/local/lib/i386-linux-gnu)
	list(APPEND CMAKE_LIBRARY_PATH /usr/lib/i386-linux-gnu)
	list(APPEND CMAKE_LIBRARY_PATH /lib/i386-linux-gnu)
elseif(EXISTS /usr/lib32)
	set(CMAKE_SYSTEM_IGNORE_PATH
		/lib             /lib64
		/usr/lib         /usr/lib64
		/usr/local/lib   /usr/local/lib64)
	set(CMAKE_LIBRARY_ARCHITECTURE "../lib32")
	list(APPEND CMAKE_LIBRARY_PATH /usr/local/lib32)
	list(APPEND CMAKE_LIBRARY_PATH /usr/lib32)
	list(APPEND CMAKE_LIBRARY_PATH /lib32)
else()
	set(CMAKE_SYSTEM_IGNORE_PATH
		/lib64
		/usr/lib64
		/usr/local/lib64)
	set(CMAKE_LIBRARY_ARCHITECTURE ".")
	list(APPEND CMAKE_LIBRARY_PATH /usr/local/lib)
	list(APPEND CMAKE_LIBRARY_PATH /usr/lib)
	list(APPEND CMAKE_LIBRARY_PATH /lib)
endif()
list(REMOVE_DUPLICATES CMAKE_LIBRARY_PATH)

# If given a CMAKE_FIND_ROOT_PATH then
# FIND_PROGRAM ignores CMAKE_FIND_ROOT_PATH (probably can't run)
# FIND_{LIBRARY,INCLUDE,PACKAGE} only uses the files in CMAKE_FIND_ROOT_PATH.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
