# PCSX2-RR
# pcsx2-1.4.0-rr
This README proposes a translation (and an update of certain parts, of course) of the original README (be wary that it may not be accurate as I don't speak nor understand Japanese and most of the translation comes from Google Translation). Consider this project as version 4 of PCSX2-rr.<br>
This informal emulator adds features needed to create a TAS to [pcsx2-1.4.0](https://github.com/PCSX2/pcsx2).<br>
This is inspired by [pcsx2-rr](<https://code.google.com/archive/p/pcsx2-rr/>). However, since the content has changed a lot and simple merging can not be done, the source code has changed considerably.
  

# TAS
Here are some TAS examples (made with the original pcsx2-rr v.1):
* [TAS of Chulip part0 (WIP) pcsx2-1.4.0-rr (nicovideo)](http://www.nicovideo.jp/watch/sm30385451)  
* [TAS of Chulip part0 (WIP) pcsx2-1.4.0-rr (youtube)](https://youtu.be/Ib2MnRfCCzc)  


# About desync
See this [wiki page](https://github.com/pocokhc/pcsx2-1.4.0-rr/wiki#%E3%82%AD%E3%83%BC%E3%83%A0%E3%83%BC%E3%83%93%E3%83%BC%E4%BD%9C%E6%88%90%E6%83%B3%E5%AE%9A%E6%89%8B%E9%A0%86201749%E6%9B%B4%E6%96%B0) (in Japanese).

# Download
You can download the Windows 10 executable from [here](TODO)
  
You will need the followings to run the executable:
* Visual C++ 2015 x86 Redistributable. Download it [HERE](https://www.microsoft.com/en-us/download/details.aspx?id=48145)  
  
This was tested only on Windows 10, but the application should work on the other OS supported by PCSX2.

# Added features
* KeyMovie recording
* Pause/Unpause/FrameAdvance
* Lua
* An editor for KeyMovie files
* A Virtual Pad
* Save/Load states to/from files
  
For more details, see the wikis:
* [PCSX2-rr v3](https://github.com/pocokhc/pcsx2-1.4.0-rr/wiki) (in Japanese)
* [PCSX2-rr v4](https://github.com/DocSkellington/pcsx2-1.4.0-rr/wiki) (only explains changes)


# How to build (Windows 10)
You can build this version as you would build the original PCSX2. It is easier with Visual Studio 2015 (works with 2017 version as well).
  
You will need:
1. Visual Studio Community 2015(<https://www.visualstudio.com/vs/community/>)  
2. DirectX Software Development Kit (June 2010)(<https://www.microsoft.com/en-us/download/details.aspx?id=6812>)  
3. Open the file "PCSX2_suite.sln" in Visual Studio and build.
  
# Changes from PCSX2-rr v3
* Lua engine keeps running when the end of the script is reached (allows to display data each frame);
* Save/Load states to/from files;
* Virtual Pad;
* TAS Input Manager:
*  Calls LuaManager::ControllerInterrupt
*  Reads, if it is open, the virtual pad. Be wary that the virtual pad overwrites the other sources of inputs (lua and user).
  
The added files can be found "./pcsx2/TAS"

# Activate Shortcuts for save/load
If you wish to activate the keyboard shortcuts for saving to (or loading from) a specific slot without having to manually switch the current slot to this one (so, if you don't want to use only F1, F2 and F3), here are the steps:
1. In the `PCSX2-rr_keys.ini` remove the `#` before States_SaveSlot0, States_SaveSlot1 (and so on)
2. Change the 10 into a 0 (zero)
3. In LilyPad settings, switch the Keyboard API to Raw input

# Titlebar during FrameAdvance
I had to cheat a little to force the frame counter to update in the titlebar. So, the other information displayed is not accurate (because it's not updated).

# Comments
I implemented what I find useful. If you have other ideas (or if find bugs), don't hesitate to open an issue (or write a post in TASVideos' forum).

Have fun while creating TAS! :D


------------------------
# Licence
Same as PCSX2: [License GNU LGPL](http://www.gnu.org/licenses/lgpl.html)  


# PCSX2 README (Will Clean this Up Later)
[![Travis Build Status](https://travis-ci.org/PCSX2/pcsx2.svg?branch=master)](https://travis-ci.org/PCSX2/pcsx2) [![AppVeyor Build Status](https://ci.appveyor.com/api/projects/status/b67odm0dd506co78/branch/master?svg=true)](https://ci.appveyor.com/project/gregory38/pcsx2/branch/master) [![Coverity Scan Build Status](https://scan.coverity.com/projects/6310/badge.svg)](https://scan.coverity.com/projects/6310)

PCSX2 is an open-source PlayStation 2 (AKA PS2) emulator. Its purpose is to emulate the PS2 hardware, using a combination of MIPS CPU [Interpreters](http://en.wikipedia.org/wiki/Interpreter_\(computing\)), [Recompilers](http://en.wikipedia.org/wiki/Dynamic_recompilation) and a [Virtual Machine](http://en.wikipedia.org/wiki/Virtual_machine) which manages hardware states and PS2 system memory. This allows you to play PS2 games on your PC, with many additional features and benefits.

# Project Details

The PCSX2 project has been running for more than ten years. Once only able to run a few public domain demos, newer versions enable many games to work at full speed, including popular titles such as Final Fantasy X or Devil May Cry 3. Visit the [PCSX2 homepage](http://pcsx2.net) to check the latest compatibility status of games (with more than 2000 titles tested), or ask for help in the [official forums](http://forums.pcsx2.net/).

The latest officially released stable version is version 1.4.0.

Installers and binaries for both Windows and Linux are available from [our website](http://pcsx2.net/download.html).

Development builds are also available from [our website](http://pcsx2.net/download/development/git.html).

# System Requirements

## Minimum
* OS: Windows Vista SP2 or newer or GNU/Linux (32-bit or 64-bit)
* CPU: Any that supports SSE2 (Pentium 4 and up, Athlon64 and up)
* GPU: DirectX 10 GPU or better
* RAM: 2GB or more

## Recommended
* OS: Windows 7/8/8.1/10 (64-bit) or GNU/Linux (64-bit)
* CPU: Intel Haswell (or AMD equivalent) @ 3.2GHz or better
* GPU: DirectX 11 GPU or greater
* RAM: 4GB or more

## Notes

- You need the [Visual C++ 2015 x86 Redistributables](https://www.microsoft.com/en-us/download/details.aspx?id=48145) for this version to work.

- PCSX2 1.4.0 is the last version to support Windows XP. Windows XP is no longer getting updates (including security-related updates), and graphics drivers for Windows XP are older and no longer maintained.

- Make sure to update your operating system, drivers, and DirectX (if applicable) to ensure you have the best experience possible. Having a newer GPU is also recommended so you have the latest supported drivers.

- Because of copyright issues, and the complexity of trying to work around it, you need a BIOS dump extracted from a legitimately-owned PS2 console to use the emulator. For more information about the BIOS and how to get it from your console, visit [this page](http://pcsx2.net/config-guide/official-english-pcsx2-configuration-guide.html#Bios).

- PCSX2 mainly takes advantage of 2 CPU cores. As of [this commit](https://github.com/PCSX2/pcsx2/commit/ac9bf45) PCSX2 can now take advantage of more than 2 cores using the MTVU speedhack. This can be a significant speedup on CPUs with 3+ cores, however on GS-limited games (or on CPUs with less than 2 cores) it may be a slowdown.

# Screenshots

![Okami](http://pcsx2.net/images/stories/gitsnaps/okami_n1s.jpg "Okami")
![Final Fantasy XII](http://pcsx2.net/images/stories/gitsnaps/finalfantasy12izjs_s2.jpg "Final Fantasy XII")
![Shadow of the Colossus](http://pcsx2.net/images/stories/gitsnaps/sotc6s2.jpg "Shadow of the Colossus")
![DragonBall Z Budokai Tenkaichi 3](http://pcsx2.net/images/stories/gitsnaps/DBZ-BT-3s.jpg "DragonBall Z Budokai Tenkaichi 3")
![Kingdom Hearts 2: Final Mix](http://pcsx2.net/images/stories/gitsnaps/kh2_fm_n1s2.jpg "Kingdom Hearts 2: Final Mix")
![God of War 2](http://pcsx2.net/images/stories/gitsnaps/gow2_s2.jpg "God of War 2")
![Metal Gear Solid 3: Snake Eater](http://pcsx2.net/images/stories/gitsnaps/mgs3-1_s2.jpg "Metal Gear Solid 3: Snake Eater")
![Rogue Galaxy](http://pcsx2.net/images/stories/gitsnaps/rogue_galaxy_n1s2.jpg "Rogue Galaxy")
