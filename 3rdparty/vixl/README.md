VIXL: Arm Runtime Code Generation Library
=========================================

Contents:

 * [Overview](#overview)
 * [Licence](#licence)
 * [Requirements](#requirements)
 * [Versioning](#versioning)
 * [Supported Arm Architecture Features](#supported-arm-architecture-features)
 * [Known limitations](#known-limitations)
 * [Bug reports](#bug-reports)
 * [Usage](#usage)

Overview
========

VIXL contains three components.

 1. Programmatic **assemblers** to generate A64, A32 or T32 code at runtime. The
    assemblers abstract some of the constraints of each ISA; for example, some
    instructions support any immediate.
 2. **Disassemblers** that can print any instruction emitted by the assemblers.
 3. An **A64 simulator** that can simulate any instruction emitted by the A64
    assembler. The simulator allows generated code to be run on another
    architecture without the need for a full ISA model.

The VIXL git repository can be found [on GitLab][vixl].

 Build status: [![Build Status](https://gitlab.arm.com/runtimes/vixl/badges/main/pipeline.svg)](https://gitlab.arm.com/runtimes/vixl/-/pipelines)


Licence
=======

This software is covered by the licence described in the [LICENCE](LICENCE)
file.

Contributions, as pull requests or via other means, are accepted under the terms
of the same [LICENCE](LICENCE).

Requirements
============

To build VIXL the following software is required:

 1. Python 3.5+
 2. SCons 2.0
 3. GCC 4.8+ or Clang 4.0+

A 64-bit host machine is required, implementing an LP64 data model. VIXL has
been tested using GCC on AArch64 Debian, GCC and Clang on amd64 Ubuntu
systems.

To run the code formatting stages of the tests, the following software is also required:

 1. clang-format 11+
 2. clang-tidy 11+

Refer to the 'Usage' section for details.

Note that in Ubuntu 18.04, clang-tidy-4.0 will only work if the clang-4.0
package is also installed.

Versioning
==========

VIXL uses [Semantic Versioning 2.0.0][semver] - see [VERSIONS](VERSIONS.md) for details.

Supported Arm Architecture Features
===================================

| Feature    | VIXL CPUFeatures Flag         | Notes                           |
|------------|-------------------------------|---------------------------------|
| BTI        | kBTI                          | Per-page enabling not supported |
| CSSC       | kCSSC                         |                                 |
| DotProd    | kDotProduct                   |                                 |
| FCMA       | kFcma                         |                                 |
| FHM        | kFHM                          |                                 |
| FP16       | kFPHalf, kNEONHalf            |                                 |
| FRINTTS    | kFrintToFixedSizedInt         |                                 |
| FlagM      | kFlagM                        |                                 |
| FlagM2     | kAXFlag                       |                                 |
| I8MM       | kI8MM                         |                                 |
| JSCVT      | kJSCVT                        |                                 |
| LOR        | kLORegions                    |                                 |
| LRCPC      | kRCpc                         |                                 |
| LRCPC2     | kRCpcImm                      |                                 |
| LSE        | kAtomics                      |                                 |
| MOPS       | kMOPS                         |                                 |
| MTE        | kMTEInstructions, kMTE, kMTE3 |                                 |
| PAuth      | kPAuth, kPAuthGeneric         | Not ERETAA, ERETAB              |
| RAS        | kRAS                          |                                 |
| RDM        | kRDM                          |                                 |
| SVE        | kSVE                          |                                 |
| SVE2       | kSVE2                         |                                 |
| SVEBitPerm | kSVEBitPerm                   |                                 |
| SVEF32MM   | kSVEF32MM                     |                                 |
| SVEF64MM   | kSVEF64MM                     |                                 |
| SVEI8MM    | kSVEI8MM                      |                                 |

Enable generating code for an architecture feature by combining a flag with
the MacroAssembler's defaults. For example, to generate code for SVE, use
`masm.GetCPUFeatures()->Combine(CPUFeatures::kSVE);`.

See [the cpu features header file](src/cpu-features.h) for more information.


Known Limitations
=================

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

Security Considerations
-----------------------

VIXL allows callers to generate any code they want. The generated code is
arbitrary, and can therefore call back into any other component in the process.
As with any self-modifying code, vulnerabilities in the client or in VIXL itself
could lead to arbitrary code generation.

For performance reasons, VIXL's Assembler only performs debug-mode checking of
instruction operands (such as immediate field encodability). This can minimise
code-generation overheads for advanced compilers that already model instructions
accurately, and might consider the Assembler's checks to be redundant. The
Assembler should only be used directly where encodability is independently
checked, and where fine control over all generated code is required.

The MacroAssembler synthesises multiple-instruction sequences to support _some_
unencodable operand combinations. The MacroAssembler can provide a useful safety
check in cases where the Assembler's precision is not required; an unexpected
unencodable operand should result in a macro with the correct behaviour, rather
than an invalid instruction.

In general, the MacroAssembler handles operands which are likely to vary with
user-supplied data, but does not usually handle inputs which are likely to be
easily covered by tests. For example, move-immediate arguments are likely to be
data-dependent, but register types (e.g. `x` vs `w`) are not.

We recommend that _all_ users use the MacroAssembler, using `ExactAssemblyScope`
to invoke the Assembler when specific instruction sequences are required. This
approach is recommended even in cases where a compiler can model the
instructions precisely, because, subject to the limitations described above, it
offers an additional layer of protection against logic bugs in instruction
selection.

Bug reports
===========

Bug reports may be made in the Issues section of GitLab, or sent to
vixl@arm.com. Please provide any steps required to recreate a bug, along with
build environment and host system information.

Usage
=====

Running all Tests
-----------------

The helper script `tools/test.py` will build and run every test that is provided
with VIXL, in both release and debug mode. It is a useful script for verifying
that all of VIXL's dependencies are in place and that VIXL is working as it
should.

By default, `tools/test.py` tests code formatting using `clang-format-4.0`,
and performs static analysis using `clang-tidy-4.0`. If you don't have these
tools, disable the test using `--noclang-format` or `--noclang-tidy`,
respectively.

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




[vixl]: https://gitlab.arm.com/runtimes/vixl
        "The VIXL repository on GitLab."

[semver]: https://semver.org/spec/v2.0.0.html
          "Semantic Versioning 2.0.0 Specification"

[getting-started-aarch32]: doc/aarch32/getting-started-aarch32.md
                           "Introduction to VIXL for AArch32."

[getting-started-aarch64]: doc/aarch64/getting-started-aarch64.md
                           "Introduction to VIXL for AArch64."
