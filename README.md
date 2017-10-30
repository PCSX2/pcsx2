# PCSX2-RR  
## Credit  
This is a fork of https://github.com/DocSkellington/pcsx2-1.4.0-rr which itself is a fork of https://github.com/pocokhc/pcsx2-1.4.0-rr, which itself is based off the original [pcsx2-rr](<https://code.google.com/archive/p/pcsx2-rr/>). However, since the content has changed a lot and simple merging can not be done, the source code has changed considerably from this original version.

## About  
This work tries to bring TAS tools to the latest version of the PCSX2 emulator.
If you have other ideas (or if you find bugs), don't hesitate to open an issue (or write a post in TASVideos' forum).  

If you would like to make an improvement to the project, don't hesitate to follow the [steps to build from source](https://github.com/xTVaser/pcsx2-rr/wiki/Building-from-Source) and submit a pull request.

Have fun while creating TAS! :D

## Video Examples  
Here is a simple tutorial on getting started with PCSX2-rr, note that keybindings may have changed!  
* [Video Tutorial](https://www.youtube.com/watch?v=1rgJ3jowxIo)  

Here are some TAS examples (made with the original pcsx2-rr v.1):  
* [TAS of Chulip part0 (WIP) pcsx2-1.4.0-rr (nicovideo)](http://www.nicovideo.jp/watch/sm30385451)  
* [TAS of Chulip part0 (WIP) pcsx2-1.4.0-rr (youtube)](https://youtu.be/Ib2MnRfCCzc)  

## Getting the Emulator  
Check the [release page](https://github.com/xTVaser/pcsx2-rr/releases) for the latest build, there is also a build for the old 1.4.0 release of pcsx2 that may be better suited for some games.  
You will need:  
* The [Visual C++ 2015 x86 Redistributables](https://www.microsoft.com/en-us/download/details.aspx?id=48145)

This was tested only on Windows 10, but the application should work on the other OS supported by PCSX2.  

## Features  
* KeyMovie recording  
* Pause/Unpause/FrameAdvance  
* Lua Support  
  * Lua engine keeps running when the end of the script is reached (allows to display data each frame);
* Save/Load states to/from files;  
* An editor for KeyMovie files  
* Virtual Pad;  
  * TAS Input Manager:  
    *  Calls LuaManager::ControllerInterrupt  
    *  Reads, if it is open, the virtual pad. Be wary that the virtual pad overwrites the other sources of inputs (lua and user).  


For more detail, see the wikis:
* [PCSX2-rr v3](https://github.com/pocokhc/pcsx2-1.4.0-rr/wiki) (in Japanese)
* [PCSX2-rr v4](https://github.com/DocSkellington/pcsx2-1.4.0-rr/wiki) (only explains changes)

## Activate Shortcuts for save/load  
If you wish to activate the keyboard shortcuts for saving to (or loading from) a specific slot without having to manually switch the current slot to this one (so, if you don't want to use only F1, F2 and F3), here are the steps:
1. In the `PCSX2-rr_keys.ini` remove the `#` before States_SaveSlot0, States_SaveSlot1 (and so on)
2. Change the 10 into a 0 (zero)
3. In LilyPad settings, switch the Keyboard API to Raw input

## Desync
See this [wiki page](https://github.com/xTVaser/pcsx2-rr/wiki#key-movie-creation-assumption-procedure-updated-april-9-2017)
  
## How to build for development (Windows 10) full details to come
See this [wiki article](https://github.com/xTVaser/pcsx2-rr/wiki/Building-from-Source)

## Current Issues (probably should just move to an issue page  
### Titlebar during FrameAdvance  
Had to cheat a little to force the frame counter to update in the titlebar. So, the other information displayed is not accurate (because it's not updated).  


---



## Relevant Information from PCSX2's README
### System Requirements
#### Minimum
* OS: Windows Vista SP2 or newer or GNU/Linux (32-bit or 64-bit)
* CPU: Any that supports SSE2 (Pentium 4 and up, Athlon64 and up)
* GPU: DirectX 10 GPU or better
* RAM: 2GB or more

#### Recommended
* OS: Windows 7/8/8.1/10 (64-bit) or GNU/Linux (64-bit)
* CPU: Intel Haswell (or AMD equivalent) @ 3.2GHz or better
* GPU: DirectX 11 GPU or greater
* RAM: 4GB or more

- PCSX2 1.4.0 is the last version to support Windows XP. Windows XP is no longer getting updates (including security-related updates), and graphics drivers for Windows XP are older and no longer maintained.

- Make sure to update your operating system, drivers, and DirectX (if applicable) to ensure you have the best experience possible. Having a newer GPU is also recommended so you have the latest supported drivers.

- Because of copyright issues, and the complexity of trying to work around it, you need a BIOS dump extracted from a legitimately-owned PS2 console to use the emulator. For more information about the BIOS and how to get it from your console, visit [this page](http://pcsx2.net/config-guide/official-english-pcsx2-configuration-guide.html#Bios).

- PCSX2 mainly takes advantage of 2 CPU cores. As of [this commit](https://github.com/PCSX2/pcsx2/commit/ac9bf45) PCSX2 can now take advantage of more than 2 cores using the MTVU speedhack. This can be a significant speedup on CPUs with 3+ cores, however on GS-limited games (or on CPUs with less than 2 cores) it may be a slowdown.

### Licence
[License GNU LGPL](http://www.gnu.org/licenses/lgpl.html)  

