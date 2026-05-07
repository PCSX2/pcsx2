# PCSX2 Reliquary

PCSX2 Reliquary is a PCSX2 fork focused on accurate emulation of security flows and esoteric PS2-derived hardware.

It targets platforms and software outside the usual retail console path, including systems such as the PSX DESR, Konami Python 1 & 2, and Namco System 246/256 families.

## Features

- Full security process support end to end.
- Bring your own keys.
- Support for all Konami Python 2 titles.
- Support for CHD-compressed internal HDD images with writable overlays.
- Switch keying mode between Developer, Retail, Arcade, and Prototype on both mechacon and memory cards.
- Support raw PS2 memory card dumps with proper keying.
- Support for utility discs such as HDD installers and DVD installers.
- Initial support for COH-based machine functionality, including Python 1 and System 2x6.
- Boots COH memory card dongles when configured in Arcade mode.
- FireWire stub.
- Can be configured for Conquest cards, but that path is not functional yet.

## What This Fork Is For
PCSX2 Reliquary exists for preservation, research, and compatibility work around the parts of the PS2 ecosystem that most emulators never needed to care about.

## Status

Current work is centered on bringing up uncommon PS2-adjacent hardware cleanly while keeping the security and memory card paths accurate and configurable.

This is an experimental fork aimed at preservation, research, and hardware-specific compatibility work.

## Configuration

### Mechacon
Mechacon keystore is configured under the advanced menu tab
![mechacon-config.png](docs/mechacon-config.png)

### BIOS
You should be running off a **FULL BIOS DUMP**. This means you need not only the bios bin file from the system you wish to boot, but also it's associated NV Ram and Mechacon config sector. This is essential for proper iLinkID matching and security to pass. [biosdrain](https://github.com/F0bes/biosdrain) is a good utility for this.
The associated files should share the name of the base bios dump (.bin/.rom0) and live in the same folder
![bios-dump.png](docs/bios-dump.png)

### Memory Card
Each memory card slot has its own configuration. Each memory card has it's own security processor with it's own keys, particularly the conquest cards for Soul Calibur have different key configuration than the booting dongle.

Here is an example for a Konami Python 1 and Namco System 2x6 config
![memorycard-config.png](docs/memorycard-config.png)

Memory card IDs are currently hard-coded as `MechaPwn`. This is the same ID that is used in sd2psx so raw memory card images can be used between real hardware and this emulator easily.
PCSX2 memory cards expect the ECC data to be present. Some memorycard dumping utilities dump without hte ECC data, but that can easily be recovered using a tool like [this](https://github.com/ffgriever-pl/PS2-ECC-Memory-Card-Converter).

### Konami Python 2
Python 2 games require pairing of the game hdd image, the associated nvram, the white and black dongle data, as well as other hardware specifics like your e-amuse card id. This can all be configured through a `.py2` file that the game library scanner can read and interpret. Details on this file format are listed in this [wiki article](https://github.com/987123879113/pcsx2/wiki/PY2-Game-Entry-File-Example).

Python 2 HDD images can be provided as either raw `.raw` files or CHD-compressed `.chd` files. CHD images are opened read-only as the base image to reduce collection size, while any writes made by the emulated HDD are stored in a separate writable overlay under `hdd-overlays/` in the emulator settings directory. This keeps the compressed source CHD unchanged and allows per-install or per-user runtime data to persist. To reset a CHD-backed HDD to its base image, close the emulator and delete the matching `.overlay` and `.map` files.

The Python 2 IO board (P2IO) is available as a USB device in the controller configuration screen. It must be plugged into port 1 for the inputs and dongles to be authenticated correctly.
![p2io-config.png](docs/p2io-config.png)

### Retail/Utility Disks
If your bios is a proper dump, and your mechacon and memory cards are setup in Retail mode, then any HDD based functionality will work like a real console. This lets you do things like run the HDD Utility disks, boot FMCB, install game HDD functionality or boot DVD update payloads.
![hdd-utility.png](docs/hdd-utility.png)
