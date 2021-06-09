<!-- PDF METADATA STARTS ---
title: "PCSX2 - Debugger Documentation"
date: "2021"
footer-left: "[Document Source](https://github.com/PCSX2/pcsx2/blob/{LATEST-GIT-TAG}/pcsx2/Docs/Debugger.md)"
urlcolor: "cyan"
... PDF METADATA ENDS -->

# Debugger Key Bindings

## Disassembly View
-   `G`   -   goto
-   `E`   -   edit breakpoint
-   `D`   -   enable/disable breakpoint
-   `B`   -   add breakpoint
-   `M`   -   assemble opcode
-   `Right Arrow`   -   follow branch/position memory view to accessed address
-   `Left Arrow`   -   go back one branch level/goto pc
-   `Up Arrow`   -   move cursor up one line
-   `Down Arrow`   -   move cursor down one line
-   `Page Up`   -   move visible area up one page
-   `Page Down`   -   move visible area down one page
-   `F10`   -   step over
-   `F11`   -   step into
-   `Tab`   -   toggle display symbols
-   `Left Click`   -   select line/toggle breakpoint if line is already highlighted
-   `Right Click`   -   open context menu

## Memory View

-   `G`   -   goto
-   `Ctrl+B`   -   add breakpoint
-   `Left Arrow`   -   move cursor back one byte/nibble
-   `Right Arrow`   -   move cursor ahead one byte/nibble
-   `Up Arrow`   -   move cursor up one line
-   `Down Arrow`   -   move cursor down one line
-   `Page Up`   -   move cursor up one page
-   `Page Down`   -   move cursor down one page
-   `0-9,A-F`   -   overwrite hex nibble
-   `any`   -   overwrite ansi byte
-   `Left Click`   -   select byte/nibble
-   `Right Click`   -   open context menu
-   `Ctrl+Mouse Wheel`   -   zoom memory view
-   `Esc`   -   return to previous goto address
-   `Ctrl+V`   -   paste a hex string into memory

## Breakpoint List

-   `Up Arrow`   -   select previous item
-   `Down Arrow`   -   select next item
-   `Delete`   - remove   selected breakpoint
-   `Enter/Return`   -   edit selected breakpoint
-   `Space`   - toggle   enable state of selected breakpoint
