<!-- PDF METADATA STARTS ---
title: "PCSX2 Frequently Asked Questions"
date: "2021"
footer-left: "[Document Source](https://github.com/PCSX2/pcsx2/blob/{LATEST-GIT-TAG}/pcsx2/Docs/PCSX2_FAQ.md)"
urlcolor: "cyan"
... PDF METADATA ENDS -->

# PCSX2 - Frequently Asked Questions

## About the PCSX2 Project

### Question 1: What is the purpose of the PCSX2 project?
PCSX2 intends to emulate the PlayStation 2 console, allowing PS2 games to be played on a computer. This requires having an original PlayStation 2 console to supply a BIOS dump and the original games, either to be played directly off the disc or to be dumped and played as ISO/CSO images.

### Question 2: Is PCSX2 open-source?
PCSX2 is free and open-source, licensed under the GNU General Public License v3.0+. [Source code is kept on GitHub](https://github.com/pcsx2/pcsx2).

### Question 3: How can I help the project?
There are a number of ways to help the project, whether it be bug reporting, game patching, or even writing code for the emulator itself. Some examples:

*   Want to make changes to emulator code? [Check out the issue tracker](https://github.com/PCSX2/pcsx2/issues), or [fork the PCSX2 repository and work on your own ideas](https://github.com/PCSX2/pcsx2).
*   Want to patch games? [Check out the cheats and patches forum thread for inspiration](https://forums.pcsx2.net/Thread-Post-your-PCSX2-cheats-patches-here). There are other threads to find as well, [such as those dedicated to 60 FPS patches](https://forums.pcsx2.net/Thread-60-fps-codes) or [widescreen patches](https://forums.pcsx2.net/Thread-PCSX2-Widescreen-Game-Patches).
*   Want to report bugs you have discovered in your games? Head over to [the Bug Reporting section of the PCSX2 forums](https://forums.pcsx2.net/Forum-Bug-reporting), to [the PCSX2 Discord](https://pcsx2.net/discord), or to [the GitHub issues section](https://github.com/PCSX2/pcsx2/issues).
*   Want to update us on the compatibility of your games? [Take a look at the Public Compatibility List on the PCSX2 forums](https://forums.pcsx2.net/Forum-Public-compatibility-list)
*   Want to improve the PCSX2 wiki? [Here is how to contribute](https://wiki.pcsx2.net/How_to_contribute)

### Question 4: Is PCSX2 ready to run out-of-the-box?
No. First, you must dump your PlayStation 2 console's BIOS using the BIOS dumper. Instructions for the BIOS dumper are [available on the PCSX2 website](https://pcsx2.net/docs/setup/bios).

After dumping your PlayStation 2 console's BIOS and copying it to your computer, launch PCSX2, step through the first-time setup wizard, and then you may begin playing.

<div class="page"/> <!-- Because PDFs are terrible -->

---
## Technical Details of PCSX2

### Question 5: What are the PC requirements to use PCSX2?

#### Minimum

| Operating System | CPU | GPU | RAM |
| --- | --- | --- | --- |
| - Windows 10 Version 1809 or newer (64-bit) <br/> - Ubuntu 22.04/Debian or newer, Arch Linux, Fedora, or other distro (64-bit) <br/> - macOS 11.0 (Big Sur) or newer | - SSE4.1 support <br/> - [PassMark Single Thread Performance](https://www.cpubenchmark.net/singleThread.html) rating near or greater than 1500 <br/> - Two physical cores, with simultaneous multithreading (SMT) | - Direct3D11 (Feature Level 10.0) support <br/> - OpenGL 3.3 support <br/> - Vulkan 1.1 support <br/> - [PassMark G3D Mark](https://www.videocardbenchmark.net/high_end_gpus.html) rating around 3000 (GeForce GTX 750, Radeon RX 560, Intel Arc A380) <br/> - 2 GB Video Memory | 8 GB |

*Note: Recommended Single Thread Performance is based on moderately complex games. Games that pushed the PS2 hardware to its limits will struggle on CPUs at this level. Some release titles and 2D games which underutilized the PS2 hardware may run on CPUs rated as low as 1200. A quick reference for CPU **intensive games**: [Wiki](https://wiki.pcsx2.net/Category:CPU_intensive_games), [Forum](https://forums.pcsx2.net/Thread-LIST-The-Most-CPU-Intensive-Games) and CPU **light** games: [Forum](https://forums.pcsx2.net/Thread-LIST-Games-that-don-t-need-a-strong-CPU-to-emulate)*

#### Recommended

| Operating System | CPU | GPU | RAM |
| --- | --- | --- | --- |
| - Windows 10 Version 22H2 (64-bit) or newer <br/> - Ubuntu 24.04/Debian or newer, Arch Linux, Fedora, or other distro (64-bit) <br/> - macOS 11.0 (Big Sur) or newer | - AVX2 support <br/> - [PassMark Single Thread Performance](https://www.cpubenchmark.net/singleThread.html) rating near or greater than 2000 <br/> - Four or more physical cores, with or without simultaneous multithreading (SMT) | - Direct3D12 support <br/> - OpenGL 4.6 support <br/> - Vulkan 1.3 support <br/> - Metal Support (macOS only) <br/> - [PassMark G3D Mark](https://www.videocardbenchmark.net/high_end_gpus.html) rating around 6000 (GeForce GTX 1650, Radeon RX 570) <br/> - 4 GB Video Memory | 16 GB |

*Note: Recommended GPU is based on 3x Internal, ~1080p resolution requirements. Higher resolutions will require stronger cards; 6x Internal, ~4K resolution will require a [PassMark G3D Mark](https://www.videocardbenchmark.net/high_end_gpus.html) rating around 12000 (GeForce GTX 1070 Ti). Just like CPU requirements, this is also highly game dependent. A quick reference for GPU **intensive games**:  [Wiki](https://wiki.pcsx2.net/Category:GPU_intensive_games)*

#### Heavy

| Operating System | CPU | GPU | RAM |
| --- | --- | --- | --- |
| - Windows 10 Version 22H2 (64-bit) or newer <br/> - Ubuntu 24.04/Debian or newer, Arch Linux, Fedora, or other distro (64-bit) <br/> - macOS 11.0 (Big Sur) or newer | - AVX2 support <br/> - [PassMark Single Thread Performance](https://www.cpubenchmark.net/singleThread.html) rating near or greater than 2600 <br/> - Six or more physical cores with simultaneous multithreading (SMT) | - Direct3D12 support <br/> - OpenGL 4.6 support <br/> - Vulkan 1.3 support <br/> - Metal support (macOS only) <br/> - [PassMark G3D Mark](https://www.videocardbenchmark.net/high_end_gpus.html) rating around 6000 (GeForce RTX 3050, Radeon RX 5600XT) <br/> - 4 GB Video Memory | 16 GB |

### Question 6: How many CPU cores can PCSX2 use?
By default, PCSX2 uses two cores for emulation. Enabling the MTVU speedhack allows the PS2's VU1 coprocessor to run on a third core, which is safe for most games and is generally enabled by default if possible on your CPU.

Software rendering can be set to use as many or as few rendering threads as desired. We recommend using no more than 8, or the number of cores (or SMT threads) that your CPU has, whichever is the lower number.

<div class="page"/> <!-- Because PDFs are terrible -->

### Question 7: Why is my CPU not using 100% of its power?
The CPU load as reported in software such as Windows' Task Manager is usually a *summation* of CPU usage, across *all* cores. Software can only use the resources of the individual CPU cores it is actually running on, and cannot "borrow" CPU time from other cores.

### Question 8: Will multi-GPU (Nvidia SLI or AMD CrossFire) help with performance?
No. PCSX2 does not use or require multi-GPU.

### Question 9: How do I check if a game is playable?
The PCSX2 website has a [compatibility list](https://pcsx2.net/compat/) with every game that has been tested.

### Question 10: Do PS1 (PSX) games work on PCSX2?

#### If you want to enjoy your PS1 games without issues:
No, they will not work without issues. Please use a proper PS1 emulator, such as DuckStation or Mednafen.

#### If you are a tinkerer and like to break things:
PS1 games may work on PCSX2, but compatibility is very limited. Specific settings are often required to get a game to boot, and there are other universal problems including missing/pitch-shifted audio and severe FMV corruption. A [forum thread for PS1 compatibility](https://forums.pcsx2.net/Thread-PSX-Mode-Unofficial-Compatibility-List) is tracking some fixes and compatibility reports.

*While we encourage discussion about PS1 games and improving compatibility, bug reports are NOT being accepted for PS1 games. Any reports for PS1 games will be closed immediately as invalid.*

### Question 11: Why does my game not work like it did in an earlier PCSX2 version?
Any change to the emulator may fix one game, but cause problems for another. If the issue is severe and not fixable with different settings, you can always revert back to the last known PCSX2 version to work, and report the build number that broke the game. [Development builds](https://pcsx2.net/downloads/) are very helpful for finding the exact change that caused a regression.

### Question 12: Why is PCSX2 slow?
The PlayStation 2 is a complex console, and this substantially raises the PC requirements to emulate it at full speed accurately. [This forum thread](https://forums.pcsx2.net/Thread-Why-is-PCSX2-slow) helps explain some of the technical reasons behind it, and our current guidelines for PC requirements are listed above.

### Question 13: Where do I get a PS2 BIOS?
You have to dump the BIOS files from your PlayStation 2 console. There is a BIOS dumper tool [available on the PCSX2 website](https://pcsx2.net/docs/setup/bios) to do this. If you need a guide, check the bottom of this FAQ for links to BIOS guides.

<div class="page"/> <!-- Because PDFs are terrible -->

### Question 14: Where do I get PS2 games?
You can use games from your personal collection of discs, purchase them from local stores that resell old games, or look online at sites like eBay or Amazon.

### Question 15: Can I use PS2 discs directly with PCSX2?
Yes. However, some games have speed problems, because unlike the PS2 which would constantly keep the disc spinning, PCSX2 will not do the same to your PC's disc drive. It is recommended to instead dump your games to ISO images. You can find a guide for this [on the PCSX2 website](https://pcsx2.net/docs/setup/dumping).

### Question 16: How do I play a game?
If you are using an ISO image:

1) Click System > Start File.
2) Locate and click on the ISO image.

If you wish to have your ISO image(s) populate in the game list:
1) Click Settings > Game List.
2) Add the directory where you keep your games if not added already.
3) It is recommended but not required that you scan this directory recursively.

If you are using a disc:

1)  Click System > Start Disc.

### Question 17: What is Fast Boot?
Fast Boot, enabled by default, will directly mount and launch the game without first launching the PS2 BIOS. You may disable this in Settings > BIOS > Fast Boot if you wish to see the BIOS startup animation or if Fast Boot is causing an issue.

### Question 18: How do I build the PCSX2 source code?
*   [Windows build guide](https://github.com/PCSX2/pcsx2/wiki/12-Building-on-Windows)
*   [Linux build guide](https://github.com/PCSX2/pcsx2/wiki/10-Building-on-Linux)
*   [macOS build guide](https://github.com/PCSX2/pcsx2/wiki/11-Building-on-MacOS)

### Question 19: When will the next version be released?
It will be released when it is ready. Please don't waste your time and ours asking when.

<div class="page"/> <!-- Because PDFs are terrible -->

---
## PCSX2 Configuration

### Question 20: How do I configure PCSX2?
Refer to the Configuration_Guide.pdf (located in the same folder as this FAQ) for an initial setup guide and resolutions or workarounds to some common known issues.

### Question 21: Will framelimiter options increase my FPS?
The framelimiter has a few options that allow you to alter the pacing of your games. This will directly affect the game's speed itself, NOT your framerate, and can be useful for speeding through sections of games, or slowing down for precision.
*   Turbo (Tab): Bumps the framelimiter to 200%, allowing a game to run up to 2x faster than normal.
*   Slowmo (Shift + Tab): Drops the framelimiter to 50%, restricting a game to 0.5x its normal speed.
*   Framelimiter Disabled (F4): Completely disables the framelimiter. The game will run as fast as your PC can push it.

### Question 22: What is the normal speed for a PlayStation 2 game?
*   NTSC games will play at 59.94 FPS
*   PAL games will play at 50 FPS
*   Keep in mind that there is a difference between the internal framerate (iFPS) and what PCSX2 shows as virtual framerate (vFPS).

These framerates are what the PlayStation 2 console would push to a real TV through its video cable. A game itself, typically, internally generates only half of those frames, and repeats frames to fill the gaps. This is why a "full speed" game may not "feel like 60 FPS". The console's "speed" (meaning AI, sound, physics, *everything*) is tied to the playback framerate, which is what PCSX2 reports as its "FPS".

### Question 23: What are Gamefixes?
Gamefixes are specialized fixes built into PCSX2 for specific games. Gamefixes mostly fix core emulation problems that would crash or soft lock a game, rather than graphical or performance issues. By default, these are automatically enabled, meaning any games that need gamefixes will have them automatically applied, regardless of what gamefix settings you have enabled.

Most games will not need gamefixes, however if your game is having issues, you can try manually enabling them in Settings > Game Properties > Game Fixes.

Gamefixes are not FPS boosters; more often than not, they will degrade performance slightly.

<div class="page"/> <!-- Because PDFs are terrible -->

### Question 24: What are all these EE/IOP and VU options?
The PS2 EE, IOP and VU processors are substantially different from a PC CPU and require different rounding and clamping modes to do math accurately. Most games work fine on the default options, but some games might need a different setting. You can check the [PCSX2 Wiki](https://wiki.pcsx2.net/Category:Games) to see if your game needs an alternate setting, or check the [PCSX2 Forums](https://forums.pcsx2.net/) to see if anyone else has posted about it there.

### Question 25: What are PCSX2 plugins?
Older versions of PCSX2 used a plugin framework for various sections of the emulator. A plugin is a small, incomplete piece of software that, when plugged in to another piece of software, provides some sort of additional function. PCSX2 used to use plugins for:
*   Video (GSdx)
*   Sound (SPU2-X)
*   Controls (LilyPad for Windows, OnePad for Linux)
*   CDVDGigzherz (CDVD)
*   Dev9Gigaherz (DEV9)
*   USB (No plugin included)

In newer versions, these have been merged into the main emulator, and there is no longer a way to load external plugins.

### Question 26: Why is my sound garbled up?
Your device may simply be too slow to play the game. If you have changed settings, attempt to revert them to the default, as these are usually the fastest.

### Question 27: Is my controller supported?
For Windows users, PCSX2 will ship with XInput bindings already created. This will automatically work with an Xbox One controller or any other XInput controller. Bindings can easily be made for DirectInput and XInput controllers, such as Xbox 360 controllers. PS3, PS4, or Switch Pro controllers should be set up using third party compatibility software, such as Steam's Big Picture mode.

For Linux and macOS users, PCSX2 will automatically detect and bind controls to any recognizable controller via SDL.

<div class="page"/> <!-- Because PDFs are terrible -->

---
## Useful Links

### BIOS Dumping Guides
*   [pgert's guide to BIOS and memory card tools](https://forums.pcsx2.net/Thread-An-orientation-through-some-of-the-PCSX2-BIOS-memcard-tools-Windows?pid=183709#pid183709)
*   [One of the original BIOS dumping guides, originally from ngemu](https://forums.pcsx2.net/Thread-Guide-to-Dumping-Your-PS2-Bios-over-LAN)

### Memory Card and Save File Guides
*   [pgert's guide to BIOS and memory card tools](https://forums.pcsx2.net/Thread-An-orientation-through-some-of-the-PCSX2-BIOS-memcard-tools-Windows?pid=183709#pid183709)
*   [An older guide on merging GameFAQs saves into PCSX2 memory cards](https://forums.pcsx2.net/Thread-How-to-use-the-myMC-tool-to-import-export-saves-from-PCSX2). Note, while the concepts in this guide are accurate, the MyMC software is old, and probably will not run on your PC without some tinkering.
*   [MyMC+, a newer replacement for the original MyMC referenced in the above guide](https://github.com/thestr4ng3r/mymcplus),

### PCSX2 Cheats and Patches (PNACH)
*   [The currently accepted master-guide to cheats and patches](https://forums.pcsx2.net/Thread-How-PNACH-files-work-2-0).
*   [The original guide, outdated](https://forums.pcsx2.net/Thread-How-pnach-files-work).

### PCSX2 with Netplay Support
*   [The latest Netplay fork of PCSX2](https://forums.pcsx2.net/Thread-PCSX2-Online-Plus).
