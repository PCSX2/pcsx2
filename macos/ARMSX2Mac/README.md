# ARMSX2 macOS UI

Native SwiftUI/AppKit desktop shell for the ARMSX2 macOS branch.

This is intentionally separate from the existing Qt UI and from the iOS UIKit
surface. It uses the ARMSX2 macOS data layout at
`~/Documents/ARMSX2-MacOS 2.0`, writes the native macOS host settings file
(`inis/PCSX2-macOS.ini`), and can launch the existing macOS emulator executable
with a selected game.

Current surface:

- PCSX2-style cover-grid and list game library.
- Game import into `iso/`.
- External game folders.
- Local cover import and serial-aware URL-template cover download.
- BIOS import and default BIOS selection.
- Fast boot, ARM64 JIT, IOP/VU JIT, fastmem, MTVU, GameDB, PNACH, graphics,
  hardware hacks, OSD, RetroAchievements, and DEV9 settings.
- Logs and storage management.
- Desktop boot launcher for the existing `pcsx2-qt`/ARMSX2 macOS executable.

Build:

```sh
./scripts/build-macos-ui.sh
```

Output:

```text
build-macos-ui/ARMSX2-MacOS 2.0.app
```

Next integration step:

- Replace the launcher backend with an embedded AppKit/Metal render host that
  reuses the native macOS `NSView` surface path, so gameplay lives inside this
  SwiftUI shell instead of a separate emulator process.
