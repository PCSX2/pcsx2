# PCSX2
[![Travis Build Status](https://travis-ci.org/PCSX2/pcsx2.svg?branch=master)](https://travis-ci.org/PCSX2/pcsx2) [![AppVeyor Build Status](https://ci.appveyor.com/api/projects/status/b67odm0dd506co78/branch/master?svg=true)](https://ci.appveyor.com/project/gregory38/pcsx2/branch/master) [![Coverity Scan Build Status](https://scan.coverity.com/projects/6310/badge.svg)](https://scan.coverity.com/projects/6310)

PCSX2 is a free and open-source PlayStation 2 (PS2) emulator. Its purpose is to emulate the PS2's hardware, using a combination of MIPS CPU [Interpreters](https://en.wikipedia.org/wiki/Interpreter_\(computing\)), [Recompilers](https://en.wikipedia.org/wiki/Dynamic_recompilation) and a [Virtual Machine](https://en.wikipedia.org/wiki/Virtual_machine) which manages hardware states and PS2 system memory. This allows you to play PS2 games on your PC, with many additional features and benefits.

# Project Details

The PCSX2 project has been running for more than ten years. Past versions could only run a few public domain game demos, but newer versions can run many games at full speed, including popular titles such as Final Fantasy X and Devil May Cry 3. Visit the [PCSX2 homepage](https://pcsx2.net) to check the latest compatibility status of games (with more than 2000 titles tested), or ask for help in the [official forums](https://forums.pcsx2.net/).

The latest officially released stable version is version 1.4.0.

Installers and binaries for both Windows and Linux are available from [our website](https://pcsx2.net/download.html).

Development builds are also available from [our website](https://pcsx2.net/download/development/git.html).

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

![Okami](https://pcsx2.net/images/stories/gitsnaps/okami_n1s.jpg "Okami")
![Final Fantasy XII](https://pcsx2.net/images/stories/gitsnaps/finalfantasy12izjs_s2.jpg "Final Fantasy XII")
![Shadow of the Colossus](https://pcsx2.net/images/stories/gitsnaps/sotc6s2.jpg "Shadow of the Colossus")
![DragonBall Z Budokai Tenkaichi 3](https://pcsx2.net/images/stories/gitsnaps/DBZ-BT-3s.jpg "DragonBall Z Budokai Tenkaichi 3")
![Kingdom Hearts 2: Final Mix](https://pcsx2.net/images/stories/gitsnaps/kh2_fm_n1s2.jpg "Kingdom Hearts 2: Final Mix")
![God of War 2](https://pcsx2.net/images/stories/gitsnaps/gow2_s2.jpg "God of War 2")
![Metal Gear Solid 3: Snake Eater](https://pcsx2.net/images/stories/gitsnaps/mgs3-1_s2.jpg "Metal Gear Solid 3: Snake Eater")
![Rogue Galaxy](https://pcsx2.net/images/stories/gitsnaps/rogue_galaxy_n1s2.jpg "Rogue Galaxy")
