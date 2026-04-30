<div align="center">

![ARMSX2](app_icons/icon.png)

# ARMSX2

[![License](https://img.shields.io/github/license/ARMSX2/ARMSX2)](https://www.gnu.org/licenses/gpl-3.0.html)
[![Discord](https://img.shields.io/discord/914421153827794975?logo=discord&logoColor=white&label=ARMSX2%20Discord&color=5865F2)](https://discord.gg/6yyawTtCnX)
(https://patreon.com/ARMSX2)
[![Nightly Build](https://github.com/ARMSX2/ARMSX2/actions/workflows/android_nightly_build.yml/badge.svg)](https://github.com/ARMSX2/ARMSX2/actions/workflows/android_nightly_build.yml)

</div>

ARMSX2 is a free and open-source PlayStation 2 (PS2) emulator for ARM devices based on PCSX2 and PCSX2_ARM64. Its purpose is to emulate the PS2's hardware for ARM devices, using a recompiler that operates as x86 -> arm64, not native arm64, this is subject to change as development continues. ARMSX2 allows you to play PS2 games on your mobile android phone, as well as on iOS, Linux, and Windows devices.

## Project Details

ARMSX2 began after years of there being no open source PS2 emulator for ARM systems, and so developer [@MoonPower](https://github.com/momo-AUX1) with the support of [@jpolo1224](https://github.com/jpolo1224) decided to try their hand at porting a new PS2 emulator for Android, forking from the repository PCSX2_ARM64 by developer Pontos. Moon has and will continue doing his best to fill in the gaps and make this into a complete emulator, with the goal to have version parity with PCSX2. This project is not officially associated with PCSX2, and we are not associated with any other forks made from the original repository. This is our own attempt at continuing PS2 emulation on Android, iOS, and MacOS. The emulator currently operates as x86 -> arm64, not native arm64, so the performance may not be as good as AetherSX2 currently, however things are subject to change as development goes on.

## System Requirements

ARMSX2 supports any ARM capable device, including Android, iOS, Linux, and Windows platforms (eventually, should work as well). Please note that performance will also depend on your devices hardware capabilities, we have done our best to optimize for low end devices and will continue to do so.

Please note that a BIOS dump from a legitimately-owned PS2 console is required to use the emulator.

## Website

→ <https://armsx2.net/>

Any other website is not affiliated with ARMSX2. 

## Translation 

[Help translate ARMSX2](https://crowdin.com/project/armsx2-translations/invite?h=940eaf6355b31b5fdb1771183c694ca32710218)

## Download

ARMSX2 is available on the Google Play Store once released. 

[<img src="https://play.google.com/intl/en_us/badges/static/images/badges/en_badge_web_generic.png" alt="Get it on Google Play" height="80"/>](https://play.google.com/store/apps/details?id=come.nanodata.armsx2)

## Affiliation

We are NOT affiliated with ARM Holding LTD in any way shape or form. We chose the name ARMSX2 since it runs on ARM devices, and seek no commercial incentive from the emulator. The most we accept is voluntary donations. Thank you. 

## Additional Credits

[PCSX2](https://github.com/PCSX2/pcsx2) - ARMSX2 would not be possible without the legendary work from the PCSX2 team and their patience and understanding regarding this project!

[PCSX2_ARM64](https://github.com/pontos2024/PCSX2_ARM64) - ARMSX2 originally started off as a fork of developer Pontos work. 

Thank you to [@Vivimagic](https://github.com/Vivimagic) for creating and working on the logo! 

Thank you to developers [@tanosshi](https://github.com/tanosshi) [@jpolo1224](https://github.com/jpolo1224) [@MoonPower](https://github.com/momo-AUX1) for working on the ARMSX2 website!

## Roadmap

Here's a roadmap of the things you can expect from ARMSX2 in the future:

| Task | Priority |
| --- | --- |
| Fix Eclipse GPUs | High |
| Fix Mali Crashes | Highest |
| Nintendo Switch support | Medium |
| Update to latest core | High |
| Update design to Material expressive | Low |
| Migrate to Kotlin | Medium | 


## Why are there .js and .jsx files?

Originally as a curious idea the react native screens were just an experiment i decided to keep they are extremely barebones and will either be finalized in a seperate branch (armsx2-rn) or removed altogether They do not affect performance as they are hidden by default and not executed. Any PR to them is welcome!

### To start developing with ARMSX2 RN do the following:

1. First install the deps:
```sh
(npm/pnpm/bun) install
```


2. Compile ARMSX2 With the react native core:
```sh
./gradlew assembleDebug -PenableRN=true
```

And now you will have a new button appear on the top right of the game selector screen click it and start developing with hot reload and see your changes without recompiling (note: compiling RN switches the emucore from static to shared).
