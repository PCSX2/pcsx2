This directory contains [GitHub Actions](https://github.com/features/actions) workflow files.

# Workflow Organization

The CI/CD workflows are organized into focused, maintainable files:

## Core Testing Workflows
- **`compilers.yml`** - Tests compatibility across different C compilers (GCC, Clang versions)
- **`core-tests.yml`** - Core LZ4 functionality testing (benchmarks, fuzzing, frame format, ABI compatibility, etc.)
- **`sanitizers.yml`** - Memory safety testing (AddressSanitizer, MemorySanitizer, UBSan, ThreadSanitizer)

## Platform & Build System Testing
- **`cross-platform.yml`** - Cross-platform testing using QEMU emulation and native macOS builds
- **`build-systems.yml`** - Alternative build systems (Visual Studio, Meson)
- **`cmake-test.yml`** - CMake build system testing

## Code Quality & Analysis
- **`code-quality.yml`** - Static analysis tools (cppcheck, scan-build, valgrind, unicode lint)
- **`oss-fuzz.yml`** - OSS-Fuzz integration for continuous fuzzing

## Utilities & Environment
- **`release-environment.yml`** - Release validation and environment information gathering
- **`scorecard.yml`** - Security scorecard analysis

This organization provides:
- **Better maintainability** - Changes to specific test categories only affect relevant files
- **Improved parallelization** - Workflows run independently and in parallel
- **Clearer failure isolation** - Easier to identify which types of tests are failing
- **Enhanced readability** - Each workflow has a single, clear responsibility

# Known issues

## Sanitizers (UBSan, ASan) - `sanitizers.yml`

### UBSan (UndefinedBehaviorSanitizer)
For now, UBSan tests use the `-fsanitize-recover=pointer-overflow` flag:
there are known cases of pointer overflow arithmetic within `lz4.c` fast compression.
These cases are not dangerous with known architecture,
but they are not guaranteed to work by the C standard,
which means that, in some future, some new architecture or some new compiler
may decide to do something funny that could break this behavior.
Hence, in anticipation, it's better to remove them.
This has been already achieved in `lz4hc.c`.
However, the same attempt in `lz4.c` resulted in massive speed losses,
which is not an acceptable cost for preemptively solving a "potential future" problem
not active anywhere today.
Therefore, a more acceptable work-around will have to be found first.


## cppcheck - `code-quality.yml`

This test script ignores the exit code of `make cppcheck`.
Because this project doesn't 100% follow their recommendation.
Also sometimes it reports false positives.


