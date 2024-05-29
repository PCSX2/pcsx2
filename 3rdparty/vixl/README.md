VIXL: Armv8 Runtime Code Generation Library, 3.0.0
==================================================

Contents:

 * Overview
 * Licence
 * Requirements
 * Known limitations
 * Usage


Overview
========

VIXL contains three components.

 1. Programmatic **assemblers** to generate A64, A32 or T32 code at runtime. The
    assemblers abstract some of the constraints of each ISA; for example, most
    instructions support any immediate.
 2. **Disassemblers** that can print any instruction emitted by the assemblers.
 3. A **simulator** that can simulate any instruction emitted by the A64
    assembler. The simulator allows generated code to be run on another
    architecture without the need for a full ISA model.

The VIXL git repository can be found [on 'https://git.linaro.org'][vixl].

Changes from previous versions of VIXL can be found in the
[Changelog](doc/changelog.md).


Licence
=======

This software is covered by the licence described in the [LICENCE](LICENCE)
file.


Requirements
============

To build VIXL the following software is required:

 1. Python 2.7
 2. SCons 2.0
 3. GCC 4.8+ or Clang 3.4+

A 64-bit host machine is required, implementing an LP64 data model. VIXL has
been tested using GCC on AArch64 Debian, GCC and Clang on amd64 Ubuntu
systems.

To run the linter and code formatting stages of the tests, the following
software is also required:

 1. Git
 2. [Google's `cpplint.py`][cpplint]
 3. clang-format-3.8

Refer to the 'Usage' section for details.


Known Limitations for AArch64 code generation
=============================================

VIXL was developed for JavaScript engines so a number of features from A64 were
deemed unnecessary:

 * Limited rounding mode support for floating point.
 * Limited support for synchronisation instructions.
 * Limited support for system instructions.
 * A few miscellaneous integer and floating point instructions are missing.

The VIXL simulator supports only those instructions that the VIXL assembler can
generate. The `doc` directory contains a
[list of supported A64 instructions](doc/aarch64/supported-instructions-aarch64.md).

The VIXL simulator was developed to run on 64-bit amd64 platforms. Whilst it
builds and mostly works for 32-bit x86 platforms, there are a number of
floating-point operations which do not work correctly, and a number of tests
fail as a result.

VIXL may not build using Clang 3.7, due to a compiler warning. A workaround is
to disable conversion of warnings to errors, or to delete the offending
`return` statement reported and rebuild. This problem will be fixed in the next
release.

Debug Builds
------------

Your project's build system must define `VIXL_DEBUG` (eg. `-DVIXL_DEBUG`)
when using a VIXL library that has been built with debug enabled.

Some classes defined in VIXL header files contain fields that are only present
in debug builds, so if `VIXL_DEBUG` is defined when the library is built, but
not defined for the header files included in your project, you will see runtime
failures.

Exclusive-Access Instructions
-----------------------------

All exclusive-access instructions are supported, but the simulator cannot
accurately simulate their behaviour as described in the ARMv8 Architecture
Reference Manual.

 * A local monitor is simulated, so simulated exclusive loads and stores execute
   as expected in a single-threaded environment.
 * The global monitor is simulated by occasionally causing exclusive-access
   instructions to fail regardless of the local monitor state.
 * Load-acquire, store-release semantics are approximated by issuing a host
   memory barrier after loads or before stores. The built-in
   `__sync_synchronize()` is used for this purpose.

The simulator tries to be strict, and implements the following restrictions that
the ARMv8 ARM allows:

 * A pair of load-/store-exclusive instructions will only succeed if they have
   the same address and access size.
 * Most of the time, cache-maintenance operations or explicit memory accesses
   will clear the exclusive monitor.
    * To ensure that simulated code does not depend on this behaviour, the
      exclusive monitor will sometimes be left intact after these instructions.

Instructions affected by these limitations:
  `stxrb`, `stxrh`, `stxr`, `ldxrb`, `ldxrh`, `ldxr`, `stxp`, `ldxp`, `stlxrb`,
  `stlxrh`, `stlxr`, `ldaxrb`, `ldaxrh`, `ldaxr`, `stlxp`, `ldaxp`, `stlrb`,
  `stlrh`, `stlr`, `ldarb`, `ldarh`, `ldar`, `clrex`.


Usage
=====

Running all Tests
-----------------

The helper script `tools/test.py` will build and run every test that is provided
with VIXL, in both release and debug mode. It is a useful script for verifying
that all of VIXL's dependencies are in place and that VIXL is working as it
should.

By default, the `tools/test.py` script runs a linter to check that the source
code conforms with the code style guide, and to detect several common errors
that the compiler may not warn about. This is most useful for VIXL developers.
The linter has the following dependencies:

 1. Git must be installed, and the VIXL project must be in a valid Git
    repository, such as one produced using `git clone`.
 2. `cpplint.py`, [as provided by Google][cpplint], must be available (and
    executable) on the `PATH`.

It is possible to tell `tools/test.py` to skip the linter stage by passing
`--nolint`. This removes the dependency on `cpplint.py` and Git. The `--nolint`
option is implied if the VIXL project is a snapshot (with no `.git` directory).

Additionally, `tools/test.py` tests code formatting using `clang-format-3.8`.
If you don't have `clang-format-3.8`, disable the test using the
`--noclang-format` option.

Also note that the tests for the tracing features depend upon external `diff`
and `sed` tools. If these tools are not available in `PATH`, these tests will
fail.

Getting Started
---------------

We have separate guides for introducing VIXL, depending on what architecture you
are targeting. A guide for working with AArch32 can be found
[here][getting-started-aarch32], while the AArch64 guide is
[here][getting-started-aarch64]. Example source code is provided in the
[examples](examples) directory. You can build examples with either `scons
aarch32_examples` or `scons aarch64_examples` from the root directory, or use
`scons --help` to get a detailed list of available build targets.




[cpplint]: http://google-styleguide.googlecode.com/svn/trunk/cpplint/cpplint.py
           "Google's cpplint.py script."

[vixl]: https://git.linaro.org/arm/vixl.git
        "The VIXL repository at 'https://git.linaro.org'."

[getting-started-aarch32]: doc/aarch32/getting-started-aarch32.md
                           "Introduction to VIXL for AArch32."

[getting-started-aarch64]: doc/aarch64/getting-started-aarch64.md
                           "Introduction to VIXL for AArch64."
