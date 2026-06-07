# Chaos Compiler Collection

This code was originally developed in the following repository and was copied
into PCSX2 by the author:

- [https://github.com/chaoticgd/ccc](https://github.com/chaoticgd/ccc)

It includes additional resources that are not present in the PCSX2 repository.

## Documentation

### DWARF (.debug) Section

- [DWARF Debugging Information Format](https://dwarfstd.org/doc/dwarf_1_1_0.pdf)

### MIPS Debug (.mdebug) Section

- [Third Eye Software and the MIPS symbol table (Peter Rowell)](http://datahedron.com/mips.html)
- [MIPS Mdebug Debugging Information (David Anderson, 1996)](https://www.prevanders.net/Mdebug.ps)
- MIPS Assembly Language Programmer's Guide, Symbol Table Chapter (Silicon Graphics, 1992)
- Tru64 UNIX Object File and Symbol Table Format Specification, Symbol Table Chapter
- `mdebugread.c` from gdb (reading)
- `ecoff.c` from gas (writing)
- `include/coff/sym.h` from binutils (headers)

### MIPS EABI

- [MIPS EABI](https://sourceware.org/legacy-ml/binutils/2003-06/msg00436.html)

### STABS

- [The "stabs" representation of debugging information (Julia Menapace, Jim Kingdon, and David MacKenzie, 1992-???)](https://sourceware.org/gdb/onlinedocs/stabs.html)
- `stabs.c` from binutils (reading)
- `stabsread.c` from gdb (reading)
- `dbxread.c` from gdb (reading)
- `dbxout.c` from gcc (writing)
- `stab.def` from gcc (symbol codes)
