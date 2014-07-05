# Contribution requirement
* be patient
    Your contribution will gladly be reviewed but 
* discuss with us your future contribution before coding it
    Let's avoid duplicate work! Besides specification could be clarified this way.


# Window contribution possibility
* check linux chapter ;) Various improvements are actually cross platform!

# Linux contribution possibility

You're welcome to the [linux contribution thread](http://forums.pcsx2.net/Thread-Areas-of-interest-for-new-linux-developers) to have full details. Here an handy list of feature that you could implement. Feel free to propse new enhancement.

## House keeping and general compilation
* clean gcc flags
* clean gcc warning
* support clang (template mess)
* speed comparison clang/gcc
* support avx (gsdx)
* add missing/update copyright header 
* LTO support
* PGO support

## Core
* support XZ compressed iso

## GSdx
* Fix OpenGL
* implement DX features on OGL (Amsodean's fxaa/video recording ....)
* Fix GLES3 
* add tooltip on gsdx gui
* finish shader subroutine usage (+find a way to clean shader and avoid duplication)
* finish buffer storage
* OSD

## CDVD
* port CDVDgiga to linux ?

## zzogl
* reduce gl requirement to 3.3 + gl4 extension
* use multibind
* fix EGL
* port GLSL to window
* Drop old GLSL backend (and much later Nivida CG)
* support wx3.0

## Portability
* port GSThread to std::thread
* port core thread to std::thread
* C11 aligned_alloc
* C++11 alignof/alignas syntax
* replace volatile/lock-free queue with real C++ atomic

## Debian package
* need a refresh to the latest standard
* Clean debian/copyright => debmake -k


## QA
* [C++11 auto port](http://clang.llvm.org/extra/clang-modernize.html). Initial requirement: drop XP and support clang/llvm
* [Clean header include](https://code.google.com/p/include-what-you-use/) 
* address sanitizer (gcc or clang)
* valgrind (not sure it can run PCSX2, maybe limit the scope to plugin)
* reformat the core/plugin to a constant style with tool like astyle

# Very very long term feature
Those features will require a lots of work! It would require months if not years. There are listed here for completeness ;)
* PS2 ROM reimplementation (wrongly named HLE bios)
* Android X86 port
* Win/Linux ARM port
