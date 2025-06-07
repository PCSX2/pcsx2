# D3D12 Memory Allocator

Easy to integrate memory allocation library for Direct3D 12.

**Documentation:** Browse online: [D3D12 Memory Allocator](https://gpuopen-librariesandsdks.github.io/D3D12MemoryAllocator/html/) (generated from Doxygen-style comments in [include/D3D12MemAlloc.h](include/D3D12MemAlloc.h))

**License:** MIT. See [LICENSE.txt](LICENSE.txt)

**Changelog:** See [CHANGELOG.md](CHANGELOG.md)

**Product page:** [D3D12 Memory Allocator on GPUOpen](https://gpuopen.com/gaming-product/d3d12-memory-allocator/)

[![Average time to resolve an issue](http://isitmaintained.com/badge/resolution/GPUOpen-LibrariesAndSDKs/D3D12MemoryAllocator.svg)](http://isitmaintained.com/project/GPUOpen-LibrariesAndSDKs/D3D12MemoryAllocator "Average time to resolve an issue")

# Problem

Memory allocation and resource (buffer and texture) creation in new, explicit graphics APIs (Vulkan® and Direct3D 12) is difficult comparing to older graphics APIs like Direct3D 11 or OpenGL® because it is recommended to allocate bigger blocks of memory and assign parts of them to resources. [Vulkan Memory Allocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/) is a library that implements this functionality for Vulkan. It is available online since 2017 and it is successfully used in many software projects, including some AAA game studios. This is an equivalent library for D3D12.

# Features

This library can help developers to manage memory allocations and resource creation by offering function `Allocator::CreateResource` similar to the standard `ID3D12Device::CreateCommittedResource`. It internally:

- Allocates and keeps track of bigger memory heaps, used and unused ranges inside them, finds best matching unused ranges to create new resources there as placed resources.
- Automatically respects size and alignment requirements for created resources.
- Automatically handles resource heap tier - whether it's `D3D12_RESOURCE_HEAP_TIER_1` that requires to keep certain classes of resources separate or `D3D12_RESOURCE_HEAP_TIER_2` that allows to keep them all together.

Additional features:

- Well-documented - description of all classes and functions provided, along with chapters that contain general description and example code.
- Thread-safety: Library is designed to be used in multithreaded code.
- Configuration: Fill optional members of `ALLOCATOR_DESC` structure to provide custom CPU memory allocator and other parameters.
- Customization and integration with custom engines: Predefine appropriate macros to provide your own implementation of external facilities used by the library, like assert, mutex, and atomic.
- Support for resource aliasing (overlap).
- Custom memory pools: Create a pool with desired parameters (e.g. fixed or limited maximum size, custom `D3D12_HEAP_PROPERTIES` and `D3D12_HEAP_FLAGS`) and allocate memory out of it.
- Support for GPU Upload Heaps from preview Agility SDK (needs compilation with `D3D12MA_OPTIONS16_SUPPORTED` macro).
- Linear allocator: Create a pool with linear algorithm and use it for much faster allocations and deallocations in free-at-once, stack, double stack, or ring buffer fashion.
- Defragmentation: Let the library move data around to free some memory blocks and make your allocations better compacted.
- Statistics: Obtain brief or detailed statistics about the amount of memory used, unused, number of allocated heaps, number of allocations etc. - globally and per memory heap type. Current memory usage and budget as reported by the system can also be queried.
- Debug annotations: Associate custom `void* pPrivateData` and debug `LPCWSTR pName` with each allocation.
- JSON dump: Obtain a string in JSON format with detailed map of internal state, including list of allocations, their string names, and gaps between them.
- Convert this JSON dump into a picture to visualize your memory. See [tools/GpuMemDumpVis](tools/GpuMemDumpVis/README.md).
- Virtual allocator - an API that exposes the core allocation algorithm to be used without allocating real GPU memory, to allocate your own stuff, e.g. sub-allocate pieces of one large buffer.

# Prerequisites

- Self-contained C++ library in single pair of H + CPP files. No external dependencies other than standard C, C++ library and Windows SDK. Some features of C++14 used. STL containers, C++ exceptions, and RTTI are not used.
- Object-oriented interface in a convention similar to D3D12.
- Error handling implemented by returning `HRESULT` error codes - same way as in D3D12.
- Interface documented using Doxygen-style comments.

# Example

Basic usage of this library is very simple. Advanced features are optional. After you created global `Allocator` object, a complete code needed to create a texture may look like this:

```cpp
D3D12_RESOURCE_DESC resourceDesc = {};
resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
resourceDesc.Alignment = 0;
resourceDesc.Width = 1024;
resourceDesc.Height = 1024;
resourceDesc.DepthOrArraySize = 1;
resourceDesc.MipLevels = 1;
resourceDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
resourceDesc.SampleDesc.Count = 1;
resourceDesc.SampleDesc.Quality = 0;
resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

D3D12MA::ALLOCATION_DESC allocDesc = {};
allocDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

D3D12Resource* resource;
D3D12MA::Allocation* allocation;
HRESULT hr = allocator->CreateResource(
    &allocDesc, &resourceDesc,
    D3D12_RESOURCE_STATE_COPY_DEST, NULL,
    &allocation, IID_PPV_ARGS(&resource));
```

With this one function call:

1. `ID3D12Heap` memory block is allocated if needed.
2. An unused region of the memory block is reserved for the allocation.
3. `ID3D12Resource` is created as placed resource, bound to this region.

`Allocation` is an object that represents memory assigned to this texture. It can be queried for parameters like offset and size.

# Binaries

The release comes with precompiled binary executable for "D3D12Sample" application which contains test suite. It is compiled using Visual Studio 2022, so it requires appropriate libraries to work, including "MSVCP140.dll", "VCRUNTIME140.dll", "VCRUNTIME140_1.dll". If its launch fails with error message telling about those files missing, please download and install [Microsoft Visual C++ Redistributable](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist?view=msvc-170), "X64" version.

# Copyright notice

This software package uses third party software:

- Parts of the code of [Vulkan Memory Allocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/) by AMD, license: MIT
- Parts of the code of [DirectX-Graphics-Samples](https://github.com/microsoft/DirectX-Graphics-Samples) by Microsoft, license: MIT

For more information see [NOTICES.txt](NOTICES.txt).

# See also

- **[Vcpkg](https://github.com/Microsoft/vcpkg)** dependency manager from Microsoft offers a port of this library that is easy to install.
- **[d3d12ma.c](https://github.com/milliewalky/d3d12ma.c)** - C bindings for this library. Author: Mateusz Maciejewski (Matt Walky). License: MIT.
- **[TerraFX.Interop.D3D12MemoryAllocator](https://github.com/terrafx/terrafx.interop.d3d12memoryallocator)** - interop bindings for this library for C#, as used by [TerraFX](https://github.com/terrafx/terrafx). License: MIT.
- **[Vulkan Memory Allocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/)** - equivalent library for Vulkan. License: MIT.

# Software using this library

- **[Qt Project](https://github.com/qt)**
- **[Ghost of Tsushima: Director's Cut PC](https://www.youtube.com/watch?v=cPKBDbCYctc&t=698s)** - Information avaliable in 11:38 of credits
- **[Godot Engine](https://github.com/godotengine/godot/)** - multi-platform 2D and 3D game engine. License: MIT.
- **[The Forge](https://github.com/ConfettiFX/The-Forge)** - cross-platform rendering framework. Apache License 2.0.
- **[Wicked Engine](https://github.com/turanszkij/WickedEngine)** - 3D engine with modern graphics

[Some other projects on GitHub](https://github.com/search?q=D3D12MemAlloc.h&type=Code) and some game development studios that use DX12 in their games.
