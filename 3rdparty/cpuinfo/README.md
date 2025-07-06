# CPU INFOrmation library

[![BSD (2 clause) License](https://img.shields.io/badge/License-BSD%202--Clause%20%22Simplified%22%20License-blue.svg)](https://github.com/pytorch/cpuinfo/blob/master/LICENSE)
[![Linux/Mac build status](https://img.shields.io/travis/pytorch/cpuinfo.svg)](https://travis-ci.org/pytorch/cpuinfo)
[![Windows build status](https://ci.appveyor.com/api/projects/status/g5khy9nr0xm458t7/branch/master?svg=true)](https://ci.appveyor.com/project/MaratDukhan/cpuinfo/branch/master)

cpuinfo is a library to detect essential for performance optimization information about host CPU.

## Features

- **Cross-platform** availability:
  - Linux, Windows, macOS, Android, iOS and FreeBSD operating systems
  - x86, x86-64, ARM, and ARM64 architectures
- Modern **C/C++ interface**
  - Thread-safe
  - No memory allocation after initialization
  - No exceptions thrown
- Detection of **supported instruction sets**, up to AVX512 (x86) and ARMv8.3 extensions
- Detection of SoC and core information:
  - **Processor (SoC) name**
  - Vendor and **microarchitecture** for each CPU core
  - ID (**MIDR** on ARM, **CPUID** leaf 1 EAX value on x86) for each CPU core
- Detection of **cache information**:
  - Cache type (instruction/data/unified), size and line size
  - Cache associativity
  - Cores and logical processors (hyper-threads) sharing the cache
- Detection of **topology information** (relative between logical processors, cores, and processor packages)
- Well-tested **production-quality** code:
  - 60+ mock tests based on data from real devices
  - Includes work-arounds for common bugs in hardware and OS kernels
  - Supports systems with heterogenous cores, such as **big.LITTLE** and Max.Med.Min
- Permissive **open-source** license (Simplified BSD)

## Examples

Log processor name:

```c
cpuinfo_initialize();
printf("Running on %s CPU\n", cpuinfo_get_package(0)->name);
```

Detect if target is a 32-bit or 64-bit ARM system:

```c
#if CPUINFO_ARCH_ARM || CPUINFO_ARCH_ARM64
    /* 32-bit ARM-specific code here */
#endif
```

Check if the host CPU supports ARM NEON

```c
cpuinfo_initialize();
if (cpuinfo_has_arm_neon()) {
    neon_implementation(arguments);
}
```

Check if the host CPU supports x86 AVX

```c
cpuinfo_initialize();
if (cpuinfo_has_x86_avx()) {
    avx_implementation(arguments);
}
```

Check if the thread runs on a Cortex-A53 core

```c
cpuinfo_initialize();
switch (cpuinfo_get_current_core()->uarch) {
    case cpuinfo_uarch_cortex_a53:
        cortex_a53_implementation(arguments);
        break;
    default:
        generic_implementation(arguments);
        break;
}
```

Get the size of level 1 data cache on the fastest core in the processor (e.g. big core in big.LITTLE ARM systems):

```c
cpuinfo_initialize();
const size_t l1_size = cpuinfo_get_processor(0)->cache.l1d->size;
```

Pin thread to cores sharing L2 cache with the current core (Linux or Android)

```c
cpuinfo_initialize();
cpu_set_t cpu_set;
CPU_ZERO(&cpu_set);
const struct cpuinfo_cache* current_l2 = cpuinfo_get_current_processor()->cache.l2;
for (uint32_t i = 0; i < current_l2->processor_count; i++) {
    CPU_SET(cpuinfo_get_processor(current_l2->processor_start + i)->linux_id, &cpu_set);
}
pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpu_set);
```

## Use via pkg-config

If you would like to provide your project's build environment with the necessary compiler and linker flags in a portable manner, the library by default when built enables `CPUINFO_BUILD_PKG_CONFIG` and will generate a [pkg-config](https://www.freedesktop.org/wiki/Software/pkg-config/) manifest (_libcpuinfo.pc_). Here are several examples of how to use it:

### Command Line

If you used your distro's package manager to install the library, you can verify that it is available to your build environment like so:

```console
$ pkg-config --cflags --libs libcpuinfo
-I/usr/include/x86_64-linux-gnu/ -L/lib/x86_64-linux-gnu/ -lcpuinfo
```

If you have installed the library from source into a non-standard prefix, pkg-config may need help finding it:

```console
$ PKG_CONFIG_PATH="/home/me/projects/cpuinfo/prefix/lib/pkgconfig/:$PKG_CONFIG_PATH" pkg-config --cflags --libs libcpuinfo
-I/home/me/projects/cpuinfo/prefix/include -L/home/me/projects/cpuinfo/prefix/lib -lcpuinfo
```

### GNU Autotools

To [use](https://autotools.io/pkgconfig/pkg_check_modules.html) with the GNU Autotools include the following snippet in your project's `configure.ac`:

```makefile
# CPU INFOrmation library...
PKG_CHECK_MODULES(
    [libcpuinfo], [libcpuinfo], [],
    [AC_MSG_ERROR([libcpuinfo missing...])])
YOURPROJECT_CXXFLAGS="$YOURPROJECT_CXXFLAGS $libcpuinfo_CFLAGS"
YOURPROJECT_LIBS="$YOURPROJECT_LIBS $libcpuinfo_LIBS"
```

### Meson

To use with Meson you just need to add `dependency('libcpuinfo')` as a dependency for your executable.

```meson
project(
    'MyCpuInfoProject',
    'cpp',
    meson_version: '>=0.55.0'
)

executable(
    'MyCpuInfoExecutable',
    sources: 'main.cpp',
    dependencies: dependency('libcpuinfo')
)
```

### Bazel

This project can be built using [Bazel](https://bazel.build/install). 

You can also use this library as a dependency to your Bazel project. Add to the `WORKSPACE` file:

```python
load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")

git_repository(
    name = "org_pytorch_cpuinfo",
    branch = "master",
    remote = "https://github.com/Vertexwahn/cpuinfo.git",
)
```

And to your `BUILD` file:

```python
cc_binary(
    name = "cpuinfo_test",
    srcs = [
        # ...
    ],
    deps = [
        "@org_pytorch_cpuinfo//:cpuinfo",
    ],
)
```

### CMake

To use with CMake use the [FindPkgConfig](https://cmake.org/cmake/help/latest/module/FindPkgConfig.html) module. Here is an example:

```cmake
cmake_minimum_required(VERSION 3.6)
project("MyCpuInfoProject")

find_package(PkgConfig)
pkg_check_modules(CpuInfo REQUIRED IMPORTED_TARGET libcpuinfo)

add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(${PROJECT_NAME} PkgConfig::CpuInfo)
```

### Makefile

To use within a vanilla makefile, you can call pkg-config directly to supply compiler and linker flags using shell substitution.

```makefile
CFLAGS=-g3 -Wall -Wextra -Werror ...
LDFLAGS=-lfoo ...
...
CFLAGS+= $(pkg-config --cflags libcpuinfo)
LDFLAGS+= $(pkg-config --libs libcpuinfo)
```

## Exposed information
- [x] Processor (SoC) name
- [x] Microarchitecture
- [x] Usable instruction sets
- [ ] CPU frequency
- [x] Cache
  - [x] Size
  - [x] Associativity
  - [x] Line size
  - [x] Number of partitions
  - [x] Flags (unified, inclusive, complex hash function)
  - [x] Topology (logical processors that share this cache level)
- [ ] TLB
  - [ ] Number of entries
  - [ ] Associativity
  - [ ] Covered page types (instruction, data)
  - [ ] Covered page sizes
- [x] Topology information
  - [x] Logical processors
  - [x] Cores
  - [x] Packages (sockets)

## Supported environments:
- [x] Android
  - [x] x86 ABI
  - [x] x86_64 ABI
  - [x] armeabi ABI
  - [x] armeabiv7-a ABI
  - [x] arm64-v8a ABI
  - [ ] ~~mips ABI~~
  - [ ] ~~mips64 ABI~~
- [x] Linux
  - [x] x86
  - [x] x86-64
  - [x] 32-bit ARM (ARMv5T and later)
  - [x] ARM64
  - [ ] PowerPC64
- [x] iOS
  - [x] x86 (iPhone simulator)
  - [x] x86-64 (iPhone simulator)
  - [x] ARMv7
  - [x] ARM64
- [x] macOS
  - [x] x86
  - [x] x86-64
  - [x] ARM64 (Apple silicon)
- [x] Windows
  - [x] x86
  - [x] x86-64
  - [x] arm64
- [x] FreeBSD
  - [x] x86-64

## Methods

- Processor (SoC) name detection
  - [x] Using CPUID leaves 0x80000002–0x80000004 on x86/x86-64
  - [x] Using `/proc/cpuinfo` on ARM
  - [x] Using `ro.chipname`, `ro.board.platform`, `ro.product.board`, `ro.mediatek.platform`, `ro.arch` properties (Android)
  - [ ] Using kernel log (`dmesg`) on ARM Linux
  - [x] Using Windows registry on ARM64 Windows
- Vendor and microarchitecture detection
  - [x] Intel-designed x86/x86-64 cores (up to Sunny Cove, Goldmont Plus, and Knights Mill)
  - [x] AMD-designed x86/x86-64 cores (up to Puma/Jaguar and Zen 2)
  - [ ] VIA-designed x86/x86-64 cores
  - [ ] Other x86 cores (DM&P, RDC, Transmeta, Cyrix, Rise)
  - [x] ARM-designed ARM cores (up to Cortex-A55, Cortex-A77, and Neoverse E1/V1/N2/V2)
  - [x] Qualcomm-designed ARM cores (Scorpion, Krait, and Kryo)
  - [x] Nvidia-designed ARM cores (Denver and Carmel)
  - [x] Samsung-designed ARM cores (Exynos)
  - [x] Intel-designed ARM cores (XScale up to 3rd-gen)
  - [x] Apple-designed ARM cores (up to Lightning and Thunder)
  - [x] Cavium-designed ARM cores (ThunderX)
  - [x] AppliedMicro-designed ARM cores (X-Gene)
- Instruction set detection
  - [x] Using CPUID (x86/x86-64)
  - [x] Using `/proc/cpuinfo` on 32-bit ARM EABI (Linux)
  - [x] Using microarchitecture heuristics on (32-bit ARM)
  - [x] Using `FPSID` and `WCID` registers (32-bit ARM)
  - [x] Using `getauxval` (Linux/ARM)
  - [x] Using `/proc/self/auxv` (Android/ARM)
  - [ ] Using instruction probing on ARM (Linux)
  - [ ] Using CPUID registers on ARM64 (Linux)
  - [x] Using IsProcessorFeaturePresent on ARM64 Windows
- Cache detection
  - [x] Using CPUID leaf 0x00000002 (x86/x86-64)
  - [x] Using CPUID leaf 0x00000004 (non-AMD x86/x86-64)
  - [ ] Using CPUID leaves 0x80000005-0x80000006 (AMD x86/x86-64)
  - [x] Using CPUID leaf 0x8000001D (AMD x86/x86-64)
  - [x] Using `/proc/cpuinfo` (Linux/pre-ARMv7)
  - [x] Using microarchitecture heuristics (ARM)
  - [x] Using chipset name (ARM)
  - [x] Using `sysctlbyname` (Mach)
  - [x] Using sysfs `typology` directories (ARM/Linux)
  - [ ] Using sysfs `cache` directories (Linux)
  - [x] Using `GetLogicalProcessorInformationEx` on ARM64 Windows
- TLB detection
  - [x] Using CPUID leaf 0x00000002 (x86/x86-64)
  - [ ] Using CPUID leaves 0x80000005-0x80000006 and 0x80000019 (AMD x86/x86-64)
  - [x] Using microarchitecture heuristics (ARM)
- Topology detection
  - [x] Using CPUID leaf 0x00000001 on x86/x86-64 (legacy APIC ID)
  - [x] Using CPUID leaf 0x0000000B on x86/x86-64 (Intel APIC ID)
  - [ ] Using CPUID leaf 0x8000001E on x86/x86-64 (AMD APIC ID)
  - [x] Using `/proc/cpuinfo` (Linux)
  - [x] Using `host_info` (Mach)
  - [x] Using `GetLogicalProcessorInformationEx` (Windows)
  - [x] Using sysfs (Linux)
  - [x] Using chipset name (ARM/Linux)

