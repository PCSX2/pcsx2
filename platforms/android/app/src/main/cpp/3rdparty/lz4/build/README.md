Projects for various integrated development environments (IDE)
==============================================================

#### Included projects

The following projects are included with the lz4 distribution:
- `cmake` - CMake project
- `meson` - Meson project
- `visual` - scripts to generate Visual Studio solutions from `cmake` script
- `VS2022` - Visual Studio 2022 solution - will soon be deprecated, prefer `visual` generators


#### Projects available within VS2022\lz4.sln

The Visual Studio solution file `lz4.sln` contains many projects that will be compiled to the
`build\VS2010\bin\$(Platform)_$(Configuration)` directory. For example `lz4` set to `x64` and
`Release` will be compiled to `build\VS2010\bin\x64_Release\lz4.exe`. The solution file contains the
following projects:

- `lz4` : Command Line Utility, supporting gzip-like arguments
- `datagen` : Synthetic and parametrable data generator, for tests
- `frametest` : Test tool that checks lz4frame integrity on target platform
- `fullbench`  : Precisely measure speed for each lz4 inner functions
- `fuzzer` : Test tool, to check lz4 integrity on target platform
- `liblz4` : A static LZ4 library compiled to `liblz4_static.lib`
- `liblz4-dll` : A dynamic LZ4 library (DLL) compiled to `liblz4.dll` with the import library `liblz4.lib`
- `fullbench-dll` : The fullbench program compiled with the import library; the executable requires LZ4 DLL


#### Using LZ4 DLL with Microsoft Visual C++ project

The header files `lib\lz4.h`, `lib\lz4hc.h`, `lib\lz4frame.h` and the import library
`build\VS2010\bin\$(Platform)_$(Configuration)\liblz4.lib` are required to
compile a project using Visual C++.

1. The path to header files should be added to `Additional Include Directories` that can
   be found in Project Properties of Visual Studio IDE in the `C/C++` Property Pages on the `General` page.
2. The import library has to be added to `Additional Dependencies` that can
   be found in Project Properties in the `Linker` Property Pages on the `Input` page.
   If one will provide only the name `liblz4.lib` without a full path to the library
   then the directory has to be added to `Linker\General\Additional Library Directories`.

The compiled executable will require LZ4 DLL which is available at
`build\VS2010\bin\$(Platform)_$(Configuration)\liblz4.dll`.
