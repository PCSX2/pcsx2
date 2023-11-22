# sanitizers-cmake

 [![](https://img.shields.io/github/issues-raw/arsenm/sanitizers-cmake.svg?style=flat-square)](https://github.com/arsenm/sanitizers-cmake/issues)
[![MIT](http://img.shields.io/badge/license-MIT-blue.svg?style=flat-square)](LICENSE)

CMake module to enable sanitizers for binary targets.


## Include into your project

To use [FindSanitizers.cmake](cmake/FindSanitizers.cmake), simply add this repository as git submodule into your own repository
```Shell
mkdir externals
git submodule add git://github.com/arsenm/sanitizers-cmake.git externals/sanitizers-cmake
```
and adding ```externals/sanitizers-cmake/cmake``` to your ```CMAKE_MODULE_PATH```
```CMake
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/externals/sanitizers-cmake/cmake" ${CMAKE_MODULE_PATH})
```

If you don't use git or dislike submodules you can copy the files in [cmake directory](cmake) into your repository. *Be careful and keep updates in mind!*

Now you can simply run ```find_package``` in your CMake files:
```CMake
find_package(Sanitizers)
```


## Usage

You can enable the sanitizers with ``SANITIZE_ADDRESS``, ``SANITIZE_MEMORY``, ``SANITIZE_THREAD`` or ``SANITIZE_UNDEFINED`` options in your CMake configuration. You can do this by passing e.g. ``-DSANITIZE_ADDRESS=On`` on your command line or with your graphical interface.

If sanitizers are supported by your compiler, the specified targets will be build with sanitizer support. If your compiler has no sanitizing capabilities (I asume intel compiler doesn't) you'll get a warning but CMake will continue processing and sanitizing will simply just be ignored.

#### Compiler issues

Different compilers may be using different implementations for sanitizers. If you'll try to sanitize targets with C and Fortran code but don't use gcc & gfortran but clang & gfortran, this will cause linking problems. To avoid this, such problems will be detected and sanitizing will be disabled for these targets.

Even C only targets may cause problems in certain situations. Some problems have been seen with AddressSanitizer for preloading or dynamic linking. In such cases you may try the ``SANITIZE_LINK_STATIC`` to link sanitizers for gcc static.



## Build targets with sanitizer support

To enable sanitizer support you simply have to add ``add_sanitizers(<TARGET>)`` after defining your target. To provide a sanitizer blacklist file you can use the ``sanitizer_add_blacklist_file(<FILE>)`` function:
```CMake
find_package(Sanitizers)

sanitizer_add_blacklist_file("blacklist.txt")

add_executable(some_exe foo.c bar.c)
add_sanitizers(some_exe)

add_library(some_lib foo.c bar.c)
add_sanitizers(some_lib)
```

## Run your application

The sanitizers check your program, while it's running. In some situations (e.g. LD_PRELOAD your target) it might be required to preload the used AddressSanitizer library first. In this case you may use the ``asan-wrapper`` script defined in ``ASan_WRAPPER`` variable to execute your application with ``${ASan_WRAPPER} myexe arg1 ...``.


## Contribute

Anyone is welcome to contribute. Simply fork this repository, make your changes **in an own branch** and create a pull-request for your change. Please do only one change per pull-request.

You found a bug? Please fill out an [issue](https://github.com/arsenm/sanitizers-cmake/issues) and include any data to reproduce the bug.


#### Contributors

* [Matt Arsenault](https://github.com/arsenm)
* [Alexander Haase](https://github.com/alehaa)
