# PCSX2
PCSX2 is a free and open-source PlayStation 2 (PS2) emulator. Its purpose is to emulate the PS2's hardware, using a combination of MIPS CPU [Interpreters](https://en.wikipedia.org/wiki/Interpreter_\(computing\)), [Recompilers](https://en.wikipedia.org/wiki/Dynamic_recompilation) and a [Virtual Machine](https://en.wikipedia.org/wiki/Virtual_machine) which manages hardware states and PS2 system memory. This allows you to play PS2 games on your PC, with many additional features and benefits.

# Fork Details
This is a fork of a build from 2019 where Sly CRC hacks were still a thing, a PNACH pointer related bugfix was released, performance & graphics were nicely balanced (even for older PCs like mine) and game enhancing cheats are included with the release. And it's all thanks to these people:
- [**NiV**](https://github.com/NiV-L-A) - developed a custom code-type to allow conditional cheats for pointers
- [**Meos**](https://www.youtube.com/channel/UCBjGlnrNZmHVLnqePH6A8vQ) - made the legendary **No Motion Blur** cheat for all versions of Sly
- [**Asasega**](https://forums.pcsx2.net/User-asasega) - created a method for patching "sceGsSetHalfOffset" routine which removes screen shakiness/"interlacing"

# Project Details

The PCSX2 project has been running for more than ten years. Past versions could only run a few public domain game demos, but newer versions can run many games at full speed, including popular titles such as s*y cooper

# System Requirements
## Minimum
* OS: Windows 7 or GNU/Linux
* CPU: Any that supports SSE2 @ [1600 STR](#Notes)
* GPU: DirectX 10 support
* RAM: 2GB

## Recommended
* OS: Windows 10 (64-bit) or GNU/Linux (64-bit)
* CPU: Any that supports AVX2 (Core series Haswell or Ryzen and newer) @ [2000 STR](#Notes) or better
* GPU: DirectX 11 support or better
* RAM: 4GB or more

## Notes
- You need the [Visual C++ 2015 x86 Redistributables](https://www.microsoft.com/en-us/download/details.aspx?id=48145) for this version to work.   
Note: Visual C++ 2017 is directly compatible with Visual C++ 2015. While the project is built with Visual C++ 2015, either version will work.

- PCSX2 1.4.0 is the last stable version to support Windows XP and Direct3D9. Windows XP is no longer getting updates (including security-related updates), and graphics drivers for Windows XP are older and no longer maintained.

- Make sure to update your operating system, drivers, and DirectX (if applicable) to ensure you have the best experience possible. Having a newer GPU is also recommended so you have the latest supported drivers.

- Because of copyright issues, and the complexity of trying to work around it, you need a BIOS dump extracted from a legitimately-owned PS2 console to use the emulator. For more information about the BIOS and how to get it from your console, visit [this page](https://pcsx2.net/config-guide/official-english-pcsx2-configuration-guide.html#Bios).

- PCSX2 mainly takes advantage of 2 CPU cores. As of [this commit](https://github.com/PCSX2/pcsx2/commit/ac9bf45) PCSX2 can now take advantage of more than 2 cores using the MTVU speedhack. This can be a significant speedup on CPUs with 3+ cores, but it may be a slowdown on GS-limited games (or on CPUs with fewer than 2 cores).

- Requirements benchmarks are based on a statistic from the Passmark CPU bench marking software. When we say "STR", we are referring to Passmark's "Single Thread Rating" statistic. You can look up your CPU on https://cpubenchmark.net to see how it compares to PCSX2's requirements.

# Screenshots


