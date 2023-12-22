// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "common/Pcsx2Defs.h"

#include <string>
#include <vector>

class Error;
class IsoReader;

struct ELF_HEADER {
	u8	e_ident[16];	//0x7f,"ELF"  (ELF file identifier)
	u16	e_type;			//ELF type: 0=NONE, 1=REL, 2=EXEC, 3=SHARED, 4=CORE
	u16	e_machine;	  //Processor: 8=MIPS R3000
	u32	e_version;	  //Version: 1=current
	u32	e_entry;		//Entry point address
	u32	e_phoff;		//Start of program headers (offset from file start)
	u32	e_shoff;		//Start of section headers (offset from file start)
	u32	e_flags;		//Processor specific flags = 0x20924001 noreorder, mips
	u16	e_ehsize;	   //ELF header size (0x34 = 52 bytes)
	u16	e_phentsize;	//Program headers entry size
	u16	e_phnum;		//Number of program headers
	u16	e_shentsize;	//Section headers entry size
	u16	e_shnum;		//Number of section headers
	u16	e_shstrndx;	 //Section header stringtable index
};

struct ELF_PHR {
	u32 p_type;		 //see notes1
	u32 p_offset;	   //Offset from file start to program segment.
	u32 p_vaddr;		//Virtual address of the segment
	u32 p_paddr;		//Physical address of the segment
	u32 p_filesz;	   //Number of bytes in the file image of the segment
	u32 p_memsz;		//Number of bytes in the memory image of the segment
	u32 p_flags;		//Flags for segment
	u32 p_align;		//Alignment. The address of 0x08 and 0x0C must fit this alignment. 0=no alignment
};

/*
notes1
------
0=Inactive
1=Load the segment into memory, no. of bytes specified by 0x10 and 0x14
2=Dynamic linking
3=Interpreter. The array element must specify a path name
4=Note. The array element must specify the location and size of aux. info
5=reserved
6=The array element must specify location and size of the program header table.
*/

struct ELF_SHR {
	u32	sh_name;		//No. to the index of the Section header stringtable index
	u32	sh_type;		//See notes2
	u32	sh_flags;	   //see notes3
	u32	sh_addr;		//Section start address
	u32	sh_offset;	  //Offset from start of file to section
	u32	sh_size;		//Size of section
	u32	sh_link;		//Section header table index link
	u32	sh_info;		//Info
	u32	sh_addralign;   //Alignment. The adress of 0x0C must fit this alignment. 0=no alignment.
	u32	sh_entsize;	 //Fixed size entries.
};

/*
notes 2
-------
Type:
0=Inactive
1=PROGBITS
2=SYMTAB symbol table
3=STRTAB string table
4=RELA relocation entries
5=HASH hash table
6=DYNAMIC dynamic linking information
7=NOTE
8=NOBITS
9=REL relocation entries
10=SHLIB
0x70000000=LOPROC processor specifc
0x7fffffff=HIPROC
0x80000000=LOUSER lower bound
0xffffffff=HIUSER upper bound

notes 3
-------
Section Flags:  (1 bit, you may combine them like 3 = alloc & write permission)
1=Write section contains data the is be writeable during execution.
2=Alloc section occupies memory during execution
4=Exec section contains executable instructions
0xf0000000=Mask bits processor-specific
*/

struct Elf32_Sym {
	u32	st_name;
	u32	st_value;
	u32	st_size;
	u8	st_info;
	u8	st_other;
	u16	st_shndx;
};

#define ELF32_ST_TYPE(i) ((i)&0xf)

struct Elf32_Rel {
	u32	r_offset;
	u32	r_info;
};

class ElfObject final
{
public:
	ElfObject();
	ElfObject(const ElfObject&) = delete;
	~ElfObject();

	__fi const std::vector<u8>& GetData() const { return data; }
	__fi const ELF_HEADER& GetHeader() const { return *reinterpret_cast<const ELF_HEADER*>(data.data()); }
	__fi u32 GetSize() const { return static_cast<u32>(data.size()); }

	bool OpenFile(std::string srcfile, bool isPSXElf_, Error* error);
	bool OpenIsoFile(std::string srcfile, IsoReader& isor, bool isPSXElf_, Error* error);

	void LoadHeaders();

	bool HasProgramHeaders() const;
	bool HasSectionHeaders() const;
	bool HasHeaders() const;

	std::pair<u32, u32> GetTextRange() const;
	u32 GetEntryPoint() const;
	u32 GetCRC() const;

private:
	std::vector<u8> data;
	ELF_PHR* proghead = nullptr;
	ELF_SHR* secthead = nullptr;
	std::string filename;
	bool isPSXElf;

	bool CheckElfSize(s64 size, Error* error);

	void InitElfHeaders();
	void LoadProgramHeaders();
	void LoadSectionHeaders();

	bool HasValidPSXHeader() const;
};

//-------------------
