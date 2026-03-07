# Windows Implementation Libraries (WIL)

[![Build Status](https://dev.azure.com/msft-wil/Windows%20Implementation%20Library/_apis/build/status/Microsoft.wil?branchName=master)](https://dev.azure.com/msft-wil/Windows%20Implementation%20Library/_build/latest?definitionId=1&branchName=master)

The Windows Implementation Libraries (WIL) is a header-only C++ library created to make life easier
for developers on Windows through readable type-safe C++ interfaces for common Windows coding patterns.

Some things that WIL includes to whet your appetite:

- [`include/wil/resource.h`](include/wil/resource.h)
  ([documentation](https://github.com/Microsoft/wil/wiki/RAII-resource-wrappers)):
  Smart pointers and auto-releasing resource wrappers to let you manage Windows
  API HANDLEs, HWNDs, and other resources and resource handles with
  [RAII](https://en.cppreference.com/w/cpp/language/raii) semantics.
- [`include/wil/win32_helpers.h`](include/wil/win32_helpers.h)
  ([documentation](https://github.com/microsoft/wil/wiki/Win32-helpers)): Wrappers for API functions
  that save you the work of manually specifying buffer sizes, calling a function twice
  to get the needed buffer size and then allocate and pass the right-size buffer,
  casting or converting between types, and so on.
- [`include/wil/registry.h`](include/wil/registry.h) ([documentation](https://github.com/microsoft/wil/wiki/Registry-Helpers)): Type-safe functions to read from, write to,
  and watch the registry. Also, registry watchers that can call a lambda function or a callback function
  you provide whenever a certain tree within the Windows registry changes.
- [`include/wil/network.h`](include/wil/network.h): ([documentation](https://github.com/microsoft/wil/wiki/Network-Helpers)) Supports Winsock and network APIs
  by providing a header-include list which addresses the inter-header include dependendies;
  provides RAII objects for WSAStartup refcounts as well as the various addrinfo* types
  returned from the family of getaddrinfo* functions; provides a type-safe class for managing
  the entire family of sockaddr-related structures.
- [`include/wil/result.h`](include/wil/result.h)
  ([documentation](https://github.com/Microsoft/wil/wiki/Error-handling-helpers)):
  Preprocessor macros to help you check for errors from Windows API functions,
  in many of the myriad ways those errors are reported, and surface them as
  error codes or C++ exceptions in your code.
- [`include/wil/Tracelogging.h`](include/wil/Tracelogging.h): This file contains the convenience macros
  that enable developers define and log telemetry. These macros use
  [`TraceLogging API`](https://docs.microsoft.com/en-us/windows/win32/tracelogging/trace-logging-portal)
  to log data. This data can be viewed in tools such as
  [`Windows Performance Analyzer`](https://docs.microsoft.com/en-us/windows-hardware/test/wpt/windows-performance-analyzer).

WIL can be used by C++ code that uses C++ exceptions as well as code that uses returned
error codes to report errors. All of WIL can be used from user-space Windows code,
and some (such as the RAII resource wrappers) can even be used in kernel mode.

# Documentation

This project is documented in [its GitHub wiki](https://github.com/Microsoft/wil/wiki). Feel free to contribute to it!

# Consuming WIL
WIL follows the "live at head" philosophy, so you should feel free to consume WIL directly from the GitHub repo however you please: as a GIT submodule, symbolic link, download and copy files, etc. and update to the latest version at your own cadence. Alternatively, WIL is available using a few package managers, mentioned below. These packages will be updated periodically, likely to average around once or twice per month.

## Consuming WIL via NuGet
WIL is available on nuget.org under the name [Microsoft.Windows.ImplementationLibrary](https://www.nuget.org/packages/Microsoft.Windows.ImplementationLibrary/). This package includes the header files under the [include](include) directory as well as a [.targets](packaging/nuget/Microsoft.Windows.ImplementationLibrary.targets) file.

## Consuming WIL via vcpkg
WIL is also available using [vcpkg](https://github.com/microsoft/vcpkg) under the name [wil](https://github.com/microsoft/vcpkg/blob/master/ports/wil/portfile.cmake). Instructions for installing packages can be found in the [vcpkg GitHub docs](https://github.com/microsoft/vcpkg/blob/master/docs/examples/installing-and-using-packages.md). In general, once vcpkg is set up on the system, you can run:
```cmd
C:\vcpkg> vcpkg install wil:x86-windows
C:\vcpkg> vcpkg install wil:x64-windows
```
Note that even though WIL is a header-only library, you still need to install the package for all architectures/platforms you wish to use it with. Otherwise, WIL won't be added to the include path for the missing architectures/platforms. Execute `vcpkg help triplet` for a list of available options.

# Building/Testing

## Prerequisites

To get started contributing to WIL, first make sure that you have:

* The latest version of [Visual Studio](https://visualstudio.microsoft.com/downloads/) or Build Tools for Visual Studio with the latest MSVC C++
  build tools and Address Sanitizer components included. In Visual Studio Installer's Import Configuration tool, pick [.vsconfig](.vsconfig)
  to the necessary workloads installed.
* The most recent [Windows SDK](https://developer.microsoft.com/windows/downloads/windows-sdk)
* [Nuget](https://www.nuget.org/downloads) downloaded and added to `PATH` - see [Install NuGet client tools](https://learn.microsoft.com/nuget/install-nuget-client-tools)
* [vcpkg](https://vcpkg.io) available on your system. Follow their [getting started](https://vcpkg.io/en/getting-started) guide to get set up. Make sure the `VCKPKG_ROOT` environment variable is set, and the path to `vcpkg.exe` is in your `%PATH%`.
* A recent version of [Clang](http://releases.llvm.org/download.html)

You can install these with WinGet in a console command line:

```powershell
winget install Microsoft.VisualStudio.2022.Community
winget install Microsoft.WindowsSDK.10.0.22621
winget install Microsoft.NuGet -e
winget install Kitware.CMake -e
winget install Ninja-build.Ninja -e

# Select "Add LLVM to the system path for all users"
winget install -i llvm.llvm
```

By default, `init.cmd` and `cmake` attempt to find vcpkg's cmake integration toolchain file on their own, by
looking in the `VCPKG_ROOT` environment variable.  After bootstrapping, you can use `setx` to have this variable
across shell sessions.  Make sure the directory containing `vcpkg.exe` is in your `PATH`.

If `VCPKG_ROOT` is not set you can pass its location to CMake or the `init.cmd` script with one of these:

* `cmake [...] --toolchain [path to vcpkg]/scripts/buildsystems/vcpkg.cmake`
* `scripts\init.cmd [...] --vcpkg <path to vcpkg>`

Note that if you use the `init.cmd` script (mentioned below), this path can be specified or auto-detected if you:
  1. Have the `VCPKG_ROOT` environment variable set to the root of your vcpkg clone.
  You can use the `setx` command to have this variable persist across shell sessions,
  1. Have the path to the root of your vcpkg clone added to your `PATH` (i.e. the path to `vcpkg.exe`), or
  1. If your vcpkg clone is located at the root of the same drive as your WIL clone (e.g. `C:\vcpkg` if your WIL clone is on the `C:` drive)

## Visual Studio 2022

Visual Studio 2022 has [fully integrated CMake support](https://learn.microsoft.com/cpp/build/cmake-projects-in-visual-studio). Opening
this directory in Visual Studio 2022 uses the default "clang Debug" configurations. Other configurations for "clang Release," "msvc Debug,"
and "msvc Release" are available in the Configuration drop-down.

Use "Build > Build All" to compile all targets.

## Visual Studio Code

Microsoft's [CMake Tools for Visual Studio Code](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools) make working
in VSCode easy.

1. Start a "Native Tools Command Prompt" (see below)
1. Launch VS Code from the prompt, giving it the path to this repo (like `code c:\wil`)
3. Use "CMake: Select Configure Preset" to pick `clang`, then "CMake: Select Build Preset" to pick `clang Debug`, then "CMake: Select Build Target" and pick `all`
4. Use "CMake: Build" to compile all targets

## Command Line

Once everything is installed (you'll need to restart Terminal if you updated `PATH` and don't have [this 2023 fix](https://github.com/microsoft/terminal/pull/14999)),
open a VS native command window (e.g. `x64 Native Tools Command Prompt for VS 2022` \[_not_ `Developer Command Prompt for VS2022`]).

You can use a [CMake preset](./CMakePresets.json) to configure the environment:

```powershell
# Configure for clang compiler, then build clang-debug
C:\wil> cmake --preset "clang"
C:\wil> cmake --build --preset "clang-debug"
```

You can also use [`scripts/init.cmd`](./scripts/init.cmd) to pick the configuration:

```cmd
C:\wil> scripts\init.cmd -c clang -g ninja -b debug
```

This script supports using msbuild as the generator/build-tool (pass `-g msbuild`) and the other [`CMAKE_BUILD_TYPE` values](https://cmake.org/cmake/help/latest/variable/CMAKE_BUILD_TYPE.html)
(pass `-b minsizerel`). You can execute `init.cmd --help` for a summary of available options.

The `scripts/init_all.cmd` script will run the `init.cmd` script for all combinations of Clang/MSVC and Debug/RelWithDebInfo.

> **Note**: For either script, projects will only be generated for the architecture of the current VS command window.

## Inner loop

In Visual Studio 2022, select a startup item (e.g. `witest.exe`) and use "Debug > Start Debugging". Targets will be
rebuilt as needed.  Use "Build > Rebuild All" to rebuild all tests. Use the Visual Studio Test Explorer to run the
tests.

In VS Code, use "CMake: Set Launch/Debug Target" and select a target (e.g. `witest.exe`). Use "CMake: Debug" (default keybind `Shift-F5`)
to run the selected test [in VS Code's debugger environment.](https://code.visualstudio.com/docs/cpp/cpp-debug).
Switch to [VS Code's "testing" tab,](https://code.visualstudio.com/docs/debugtest/testing) and click the "Run Tests"
option.

For command-line CMake (configured with `cmake --preset ...`) build use, some examples:
```powershell
# Build for MSVC release, all targets
C:\wil> cmake --build --preset "msvc-release"

# Build only one test (e.g. for improved compile times)
C:\wil> cmake --build --preset "msvc-release" --target "witest.noexcept"

# Clean outputs, then build one target
C:\wil> cmake --build --preset "msvc-release" --clean-first --target "witest"

# Run tests
C:\wil> ctest --preset "msvc-release"
```

For command-line Ninja (configured with `init.cmd -c clang -b debug`) build use, some examples:
```powershell
# Build all tests
C:\wil> ninja -C build\clang-x64-debug

# Build only one test
C:\wil> ninja -C build\clang-x64-debug witest

# Clean outputs, then build one target
C:\wil> ninja -C build\clang-x64-debug -j 0 clean witest.app

# Run the tests (PowerShell)
C:\wil> Get-ChildItem -Recurse -File build\clang-x64-debug\witest*.exe | %{ Write-Host $_ ; & $_ }

# Run the tests (cmd)
C:\wil> for /F %f IN ('dir /s /b build\clang\tests\witest*.exe') do %f
```

The output is a number of test executables. If you used the initialization script(s) mentioned above, or if you followed
the same directory naming convention of those scripts, you can use the [runtests.cmd](scripts/runtests.cmd) script,
which will execute any test executables that have been built, erroring out - and preserving the exit code - if any test
fails. Note that MSBuild will modify the output directory names, so this script is only compatible with using Ninja as the
generator.

> **Note:** The `witest.app` test is significantly slower than the other tests. You can use Test Explorer or the Testing
> tab to hide it while doing quick validation.

## Build everything

If you are at the tail end of of a change, you can execute the following to get a wide range of coverage:
```cmd
C:\wil> scripts\init_all.cmd
C:\wil> scripts\build_all.cmd
C:\wil> scripts\runtests.cmd
```

## Formatting

This project has adopted `clang-format` as the tool for formatting our code.
Please note that the `.clang-format` at the root of the repo is a copy from the internal Windows repo with few additions.
In general, please do not modify it.
If you find that a macro is causing bad formatting of code, you can add that macro to one of the corresponding arrays in the `.clang-format` file (e.g. `AttributeMacros`, etc.), format the code, and submit a PR.

> _NOTE: Different versions of `clang-format` may format the same code differently.
In an attempt to maintain consistency between changes, we've standardized on using the version of `clang-format` that ships with the latest version of Visual Studio.
If you have LLVM installed and added to your `PATH`, the version of `clang-format` that gets picked up by default may not be the one we expect.
If you leverage the formatting scripts we have provided in the `scripts` directory, these should automatically pick up the proper version provided you are using a Visual Studio command window._

Before submitting a PR to the WIL repo we ask that you first run `clang-format` on your changes.
There is a CI check in place that will fail the build for your PR if you have not run `clang-format`.
There are a few different ways to format your code:

### 1. Formatting with `git clang-format`

> **Important!** Git integration with `clang-format` is only available through the LLVM distribution.
You can install LLVM through their [GibHub releases page](https://github.com/llvm/llvm-project/releases), via `winget install llvm.llvm`, or through the package manager of your choice.

> **Important!** The use of `git clang-format` additionally requires Python to be installed and available on your `PATH`.

The simplest way to format just your changes is to use `clang-format`'s `git` integration.
You have the option to do this continuously as you make changes, or at the very end when you're ready to submit a PR.
To format code continuously as you make changes, you run `git clang-format` after staging your changes.
For example:
```cmd
C:\wil> git add *
C:\wil> git clang-format --style file
```
At this point, the formatted changes will be unstaged.
You can review them, stage them, and then commit.
Please note that this will use whichever version of `clang-format` is configured to run with this command.
You can pass `--binary <path>` to specify the path to `clang-format.exe` you would like the command to use.

If you'd like to format changes at the end of development, you can run `git clang-format` against a specific commit/label.
The simplest is to run against `upstream/master` or `origin/master` depending on whether or not you are developing in a fork.
Please note that you likely want to sync/merge with the master branch prior to doing this step.
You can leverage the `format-changes.cmd` script we provide, which will use the version of `clang-format` that ships with Visual Studio:
```cmd
C:\wil> git fetch upstream
C:\wil> git merge upstream/master
C:\wil> scripts\format-changes.cmd upstream/master
```

### 2. Formatting with `clang-format`

> **Important!** The path to `clang-format.exe` is not added to `PATH` automatically, even when using a Visual Studio command window.
The LLVM installer has the option to add itself to the system or user `PATH` if you'd like.
If you would like the path to the version of `clang-format` that ships with Visual Studio added to your path, you will need to do so manually.
Otherwise, the `run-clang-format.cmd` script mentioned below (or, equivalently, building the `format` target) will manually invoke the `clang-format.exe` under your Visual Studio install path.

An alternative, and generally easier option, is to run `clang-format` either on all source files or on all source files you've modified.
Note, however, that depending on how `clang-format` is invoked, the version used may not be the one that ships with Visual Studio.
Some tools such as Visual Studio Code allow you to specify the path to the version of `clang-format` that you wish to use when formatting code, however this is not always the case.
The `run-clang-format.cmd` script we provide will ensure that the version of `clang-format` used is the version that shipped with your Visual Studio install:
```cmd
C:\wil> scripts\run-clang-format.cmd
```
Additionally, we've added a build target that will invoke this script, named `format`:
```cmd
C:\wil\build\clang-x64-debug> ninja format
```
Please note that this all assumes that your Visual Studio installation is up to date.
If it's out of date, code unrelated to your changes may get formatted unexpectedly.
If that's the case, you may need to manually revert some modifications that are unrelated to your changes.

> _NOTE: Occasionally, Visual Studio will update without us knowing and the version installed for you may be newer than the version installed the last time we ran the format all script. If that's the case, please let us know so that we can re-format the code._

# Contributing

This project welcomes contributions and suggestions.  Most contributions require you to agree to a
Contributor License Agreement (CLA) declaring that you have the right to, and actually do, grant us
the rights to use your contribution. For details, visit https://cla.microsoft.com.

When you submit a pull request, a CLA-bot will automatically determine whether you need to provide
a CLA and decorate the PR appropriately (e.g., label, comment). Simply follow the instructions
provided by the bot. You will only need to do this once across all repos using our CLA.

This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/).
For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or
contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.
