# Build instructions for libcubeb

You must have CMake v3.14 or later installed.

1. `git clone --recursive https://github.com/mozilla/cubeb.git`
2. `cd cubeb`
3. `cmake -B ./build .`
4. `cmake --build ./build`
5. `cd build && ctest`

# Windows build notes

Windows builds can use Microsoft Visual Studio 2015, Microsoft Visual Studio
2017, or MinGW-w64 with Win32 threads (by passing `cmake -G` to generate the
appropriate build configuration).

## Microsoft Visual Studio 2015 or 2017 Command Line

CMake can be used from the command line by following the build steps at the top
of this file. CMake will select a default generator based on the environment,
or one can be specified with the `-G` argument.

## Microsoft Visual Studio 2017 IDE

Visual Studio 2017 adds in built support for CMake. CMake can be used from
within the IDE via the following steps:

- Navigate to `File -> Open -> Cmake...`
- Open `CMakeLists.txt` file in the root of the project.

Note, to generate the build in the cubeb dir CMake settings need to be updated
via: `CMake -> Change CMake Settings -> CMakeLists.txt`. The default
configuration used by Visual Studio will place the build in a different location
than the steps detailed at the top of this file.

## MinGW-w64

To build with MinGW-w64, install the following items:

- Download and install MinGW-w64 with Win32 threads.
- Download and install CMake.
- Run MinGW-w64 Terminal from the Start Menu.
- Follow the build steps at the top of this file, but at step 4 run:
  `cmake -G "MinGW Makefiles" ../cubeb`
- Continue the build steps at the top of this file.
