# Shaderc

A collection of tools, libraries and tests for shader compilation.
At the moment it includes:

- [`glslc`](glslc), a command line compiler for GLSL/HLSL to SPIR-V, and
- [`libshaderc`](libshaderc), a library API for accessing `glslc` functionality.

**Note:** The fact that that `libshaderc` is not named `libshaderc_glslc` is a
quirk of history, and a known inconsistency. Changing it would require a
significant amount of renaming and breaking of downstream projects, so it is
being left as is.

`glslc` wraps around core functionality in [glslang][khr-glslang]
and [SPIRV-Tools][spirv-tools]. `glslc` and its library aims to
to provide:
* a command line compiler with GCC- and Clang-like usage, for better
  integration with build systems
* an API where functionality can be added without breaking existing clients
* an API supporting standard concurrency patterns across multiple
  operating systems
* increased functionality such as file `#include` support

## Downloads

**Note: These binaries are just the artifacts of the builders and have not
  undergone any QA, thus they should be considered unsupported.**

<img alt="Linux" src="kokoro/img/linux.png" width="20px" height="20px" hspace="2px"/>[![Linux Build Status](https://storage.googleapis.com/shaderc/badges/build_status_linux_clang_release.svg)](https://storage.googleapis.com/shaderc/badges/build_link_linux_clang_release.html)
<img alt="MacOS" src="kokoro/img/macos.png" width="20px" height="20px" hspace="2px"/>[![MacOS Build Status](https://storage.googleapis.com/shaderc/badges/build_status_macos_clang_release.svg)](https://storage.googleapis.com/shaderc/badges/build_link_macos_clang_release.html)
<img alt="Windows" src="kokoro/img/windows.png" width="20px" height="20px" hspace="2px"/>[![Windows Build Status](https://storage.googleapis.com/shaderc/badges/build_status_windows_vs2022_amd64_release.svg)](https://storage.googleapis.com/shaderc/badges/build_link_windows_vs2022_release.html)

[More downloads](downloads.md)

## Status

Shaderc has maintained backward compatibility for quite some time, and we
don't anticipate any breaking changes.
Ongoing enhancements are described in the [CHANGES](CHANGES) file.

Shaderc has been shipping in the
[Android NDK](https://developer.android.com/ndk/index.html) since version r12b.
(The NDK build uses sources from https://android.googlesource.com/platform/external/shaderc/.
Those repos are downstream from GitHub.)
We currently require r25c.

For licensing terms, please see the [`LICENSE`](LICENSE) file.  If interested in
contributing to this project, please see [`CONTRIBUTING.md`](CONTRIBUTING.md).

This is not an official Google product (experimental or otherwise), it is just
code that happens to be owned by Google.  That may change if Shaderc gains
contributions from others.  See the [`CONTRIBUTING.md`](CONTRIBUTING.md) file
for more information. See also the [`AUTHORS`](AUTHORS) and
[`CONTRIBUTORS`](CONTRIBUTORS) files.

## File organization

- `android_test/` : a small Android application to verify compilation
- `cmake/`: CMake utility functions and configuration for Shaderc
- `examples/`: Example programs
- `glslc/`: an executable to compile GLSL to SPIR-V
- `libshaderc/`: a library for compiling shader strings into SPIR-V
- `libshaderc_util/`: a utility library used by multiple shaderc components
- `third_party/`: third party open source packages; see below
- `utils/`: utility scripts for Shaderc

Shaderc depends on glslang, the Khronos reference compiler for GLSL.

Shaderc depends on [SPIRV-Tools][spirv-tools] for assembling, disassembling,
and transforming SPIR-V binaries.

For testing, Shaderc depends on:
| Library | URL | Description |
| -- | -- | -- |
| Googletest | https://github.com/google/googletest | Testing framework |
| Effcee | https://github.com/google/effcee | Stateful pattern matcher inspired by LLVM's FileCheck |
| RE2 | https://github.com/google/re2 | Regular expression matcher |
| Abseil | https://github.com/abseil/abseil-cpp | Common basic utilities in C++ |

In the following sections, `$SOURCE_DIR` is the directory you intend to clone
Shaderc into.

## Getting and building Shaderc

**If you only want prebuilt executables or libraries, see the
[Downloads](#downloads) section.**

The rest of this section describes how to build Shaderc from sources.

Note: Shaderc assumes Glslang supports HLSL compilation.  The instructions
below assume you're building Glslang from sources, and in a subtree
of `shaderc/third_party`.  In that scenario, Glslang's HLSL support
is automatically enabled.  Shaderc also can be built using a Glslang
from outside the `shaderc/third_party` tree.  In that case you must
ensure that that external Glslang is built with HLSL functionality.
See Glslang's `ENABLE_HLSL` CMake setting.)

1) Check out the source code:

```sh
git clone https://github.com/google/shaderc $SOURCE_DIR
cd $SOURCE_DIR
./utils/git-sync-deps
```

**Note:** The [known-good](https://github.com/google/shaderc/tree/known-good)
branch of the repository contains a
[known_good.json](https://github.com/google/shaderc/blob/known-good/known_good.json)
file describing a set of repo URLs and specific commits that have been
tested together.  This information is updated periodically, and typically
matches the latest update of these sources in the development branch
of the Android NDK.
The `known-good` branch also contains a
[update_shaderc.py](https://github.com/google/shaderc/blob/known-good/update_shaderc_sources.py)
script that will read the JSON file and checkout those specific commits for you.

2) Ensure you have the requisite tools -- see the tools subsection below.

3) Decide where to place the build output. In the following steps, we'll call it
   `$BUILD_DIR`. Any new directory should work. We recommend building outside
   the source tree, but it is also common to build in a (new) subdirectory of
   `$SOURCE_DIR`, such as `$SOURCE_DIR/build`.

4a) Build (and test) with Ninja on Linux or Windows:

```sh
cd $BUILD_DIR
cmake -GNinja -DCMAKE_BUILD_TYPE={Debug|Release|RelWithDebInfo} $SOURCE_DIR
ninja
ctest # optional
```

4b) Or build (and test) with MSVC on Windows:

```sh
cd $BUILD_DIR
cmake $SOURCE_DIR
cmake --build . --config {Release|Debug|MinSizeRel|RelWithDebInfo}
ctest -C {Release|Debug|MinSizeRel|RelWithDebInfo}
```

4c) Or build with MinGW on Linux for Windows:

```sh
cd $BUILD_DIR
cmake -GNinja -DCMAKE_BUILD_TYPE={Debug|Release|RelWithDebInfo} $SOURCE_DIR \
   -DCMAKE_TOOLCHAIN_FILE=$SOURCE_DIR/cmake/linux-mingw-toolchain.cmake
ninja
```

After a successful build, you should have a `glslc` executable somewhere under
the `$BUILD_DIR/glslc/` directory, as well as a `libshaderc` library somewhere
under the `$BUILD_DIR/libshaderc/` directory.

The default behavior on MSVC is to link with the static CRT. If you would like
to change this behavior `-DSHADERC_ENABLE_SHARED_CRT` may be passed on the
cmake configure line.

See [the libshaderc README](libshaderc/README.md) for more on using the library
API in your project.

### Tools you'll need

For building, testing, and profiling Shaderc, the following tools should be
installed regardless of your OS:

- A C++17 compiler. Recent versions of Clang, GCC, and MSVC work.
- [CMake](http://www.cmake.org/) 3.14 or later: for generating compilation targets.
    - Shaderc is tested with cmake 3.17.2
- [Python 3](http://www.python.org/): for utility scripts and running the test suite.

On Linux, if cross compiling to Windows:
- [`mingw`](http://www.mingw.org): A GCC-based cross compiler targeting Windows
    so that generated executables use the Microsoft C runtime libraries.
    The MinGW compiler must support C++17.

On Windows, the following tools should be installed and available on your path:

- Visual Studio 2022 or later. Previous versions of Visual Studio may work but
  are untested and unsupported.
- Git - including the associated tools, Bash, `diff`.

Optionally, the following tools may be installed on any OS:

 - [`asciidoctor`](http://asciidoctor.org/): for generating documentation.
   - [`pygments.rb`](https://rubygems.org/gems/pygments.rb) required by
     `asciidoctor` for syntax highlighting.

### Building and running Shaderc using Docker

Please make sure you have the Docker engine
[installed](https://docs.docker.com/engine/installation/) on your machine.

To create a Docker image containing Shaderc command line tools, issue the
following command in `${SOURCE_DIR}`: `docker build -t <IMAGE-NAME> .`.
The created image will have all the command line tools installed at
`/usr/local` internally, and a data volume mounted at `/code`.

Assume `<IMAGE-NAME>` is `shaderc/shaderc` from now on.

To invoke a tool from the above created image in a Docker container:

```bash
docker run shaderc/shaderc glslc --version
```

Alternatively, you can mount a host directory (e.g., `example`) containing
the shaders you want to manipulate and run different kinds of tools via
an interactive shell in the container:

```bash
$ docker run -i -t -v `pwd`/example:/code shaderc/shaderc
/code $ ls
test.vert
/code $ glslc -c -o - test.vert | spirv-dis
```

## Bug tracking

We track bugs using GitHub -- click on the "Issues" button on
[the project's GitHub page](https://github.com/google/shaderc).

## Bindings

Bindings are maintained by third parties, may contain content
offered under a different license, and may reference or contain
older versions of Shaderc and its dependencies.

* **Python:** [pyshaderc][pyshaderc]
* **Rust:** [shaderc-rs][shaderc-rs]
* **Go:** [gshaderc][gshaderc]
* **.NET:** [shaderc.net][shadercdotnet]
* **Common Lisp:** [shadercl][shaderccommonlisp]

[khr-glslang]: https://github.com/KhronosGroup/glslang
[spirv-tools]: https://github.com/KhronosGroup/SPIRV-Tools
[spirv-cross]: https://github.com/KhronosGroup/SPIRV-Cross
[pyshaderc]: https://github.com/realitix/pyshaderc
[shaderc-rs]: https://github.com/google/shaderc-rs
[appveyor]: https://ci.appveyor.com/project/dneto0/shaderc
[dawn]: https://dawn.googlesource.com/dawn
[gshaderc]: https://github.com/celer/gshaderc
[shadercdotnet]: https://github.com/jpbruyere/shaderc.net
[shaderccommonlisp]: https://github.com/JolifantoBambla/shadercl
