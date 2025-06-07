# 3.0.1 (2025-05-08)

- Fixed macros `D3D12MA_RECOMMENDED_ALLOCATOR_FLAGS`, `D3D12MA_RECOMMENDED_POOL_FLAGS` (#73).

# 3.0.0 (2025-05-05)

It has been a long time since the previous official release, so hopefully everyone has been using the latest code from "master" branch, which is always maintained in a good state, not the old version. For completeness, here is the list of changes since v2.0.1. The major version number has changed, so there are some compatibility-breaking changes, but the basic API stays the same and is mostly backward-compatible.

- Added helper structs: `CALLOCATION_DESC`, `CPOOL_DESC`, `CVIRTUAL_BLOCK_DESC`, `CVIRTUAL_ALLOCATION_DESC`.
- Added macros: `D3D12MA_RECOMMENDED_ALLOCATOR_FLAGS`, `D3D12MA_RECOMMENDED_HEAP_FLAGS`, `D3D12MA_RECOMMENDED_POOL_FLAGS`.
- Added functions: `Allocator::CreateResource3`, `CreateAliasingResource2`.
    - They support parameters: `D3D12_BARRIER_LAYOUT InitialLayout`, `const DXGI_FORMAT* pCastableFormats`.
    - They require recent DirectX 12 Agility SDK. To use them, `ID3D12Device10` must be available.
      To use non-empty list of castable formats, `ID3D12Device12` must be available.
- Added support for GPU Upload Heaps (`D3D12_HEAP_TYPE_GPU_UPLOAD`).
    - Requires recent DirectX 12 Agility SDK. Support on the user's machine is available only when supported by the motherboard, GPU, drivers, and enabled as "Resizable BAR" in UEFI settings. It can be queried using new `Allocator::IsGPUUploadHeapSupported` function.
    - `TotalStatistics::HeapType` array was extended from 4 to 5 elements.
- Added missing function `Allocator::CreateAliasingResource1`.
- Added `POOL_DESC::ResidencyPriority` member.
- Removed `Allocation::WasZeroInitialized` function. It wasn't fully implemented anyway.
- Added `POOL_FLAG_ALWAYS_COMMITTED`.
- Added a heuristic that prefers creating small buffers as committed to save memory.
    - It is enabled by default. It can be disabled by new flag `ALLOCATOR_FLAG_DONT_PREFER_SMALL_BUFFERS_COMMITTED`.
- Macro `D3D12MA_OPTIONS16_SUPPORTED` is no longer exposed in the header or Cmake script.
  It is defined automatically based on the Agility SDK version.
- Added macro `D3D12MA_DEBUG_LOG`, which can be used to log unfreed allocations.
- Many improvements in the documentation, including new chapters: "Frequently asked questions", "Optimal resource allocation".
- Countless fixes and improvements, including performance optimizations, compatibility with various compilers, tests.
- Major changes in the Cmake script.
- Fixes in "GpuMemDumpVis.py" script.

# 2.0.1 (2022-04-05)

A maintenance release with some bug fixes and improvements. There are no changes in the library API.

- Fixed an assert failing when detailed JSON dump was made while a custom pool was present with specified string name (#36, thanks @rbertin-aso).
- Fixed image height calculation in JSON dump visualization tool "GpuMemDumpVis.py" (#37, thanks @rbertin-aso).
- Added JSON Schema for JSON dump format - see file "tools\GpuMemDumpVis\GpuMemDump.schema.json".
- Added documentation section "Resource reference counting".

# 2.0.0 (2022-03-25)

So much has changed since the first release that it doesn’t make much sense to compare the differences. Here are the most important features that the library now provides:

- Powerful custom pools, which give an opportunity to not only keep certain resources together, reserve some minimum or limit the maximum amount of memory they can take, but also to pass additional allocation parameters unavailable to simple allocations. Among them, probably the most interesting is `POOL_DESC::HeapProperties`, which allows you to specify parameters of a custom memory type, which may be useful on UMA platforms. Committed allocations can now also be created in custom pools.
- The API for statistics and budget has been redesigned - see structures `Statistics`, `Budget`, `DetailedStatistics`, `TotalStatistics`.
- The library exposes its core allocation algorithm via the “virtual allocator” interface. This can be used to allocate pieces of custom memory or whatever you like, even something completely unrelated to graphics.
- The allocation algorithm has been replaced with the new, more efficient TLSF.
- Added support for defragmentation.
- Objects of the library can be used with smart pointers designed for COM objects.

# 1.0.0 (2019-09-02)

First published version.
