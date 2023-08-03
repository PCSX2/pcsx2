/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "PrecompiledHeader.h"

#include "Elfheader.h"
#include "CDVD/IsoReader.h"
#include "DebugTools/Debug.h"
#include "DebugTools/SymbolMap.h"

#include "common/Error.h"
#include "common/FileSystem.h"
#include "common/StringUtil.h"

#pragma pack(push, 1)
struct PSXEXEHeader
{
	char id[8]; // 0x000-0x007 PS-X EXE
	char pad1[8]; // 0x008-0x00F
	u32 initial_pc; // 0x010
	u32 initial_gp; // 0x014
	u32 load_address; // 0x018
	u32 file_size; // 0x01C excluding 0x800-byte header
	u32 unk0; // 0x020
	u32 unk1; // 0x024
	u32 memfill_start; // 0x028
	u32 memfill_size; // 0x02C
	u32 initial_sp_base; // 0x030
	u32 initial_sp_offset; // 0x034
	u32 reserved[5]; // 0x038-0x04B
	char marker[0x7B4]; // 0x04C-0x7FF
};
static_assert(sizeof(PSXEXEHeader) == 0x800);
#pragma pack(pop)

ElfObject::ElfObject() = default;

ElfObject::~ElfObject() = default;

bool ElfObject::OpenIsoFile(std::string srcfile, IsoReader& isor, bool isPSXElf_, Error* error)
{
	const auto de = isor.LocateFile(srcfile, error);
	if (!de)
		return false;

	if (!CheckElfSize(de->length_le, error))
		return false;

	if (!isor.ReadFile(de.value(), &data, error))
		return false;

	filename = std::move(srcfile);
	isPSXElf = isPSXElf_;
	InitElfHeaders();
	return true;
}

bool ElfObject::OpenFile(std::string srcfile, bool isPSXElf_, Error* error)
{
	auto fp = FileSystem::OpenManagedCFile(srcfile.c_str(), "rb", error);
	FILESYSTEM_STAT_DATA sd;
	if (!fp || !FileSystem::StatFile(fp.get(), &sd))
	{
		Error::SetString(error, fmt::format("Failed to read ELF from '{}'", srcfile));
		return false;
	}

	if (!isPSXElf_ && !CheckElfSize(sd.Size, error))
		return false;

	data.resize(static_cast<size_t>(sd.Size));
	if (std::fread(data.data(), data.size(), 1, fp.get()) != 1)
	{
		Error::SetString(error, fmt::format("Failed to read ELF from '{}'", srcfile));
		return false;
	}

	filename = std::move(srcfile);
	isPSXElf = isPSXElf_;
	InitElfHeaders();
	return true;
}

void ElfObject::InitElfHeaders()
{
	if (isPSXElf)
		return;

	DevCon.WriteLn("Initializing Elf: %zu bytes", data.size());

	const ELF_HEADER& header = GetHeader();
	if (header.e_phnum > 0)
	{
		if ((header.e_phoff + sizeof(ELF_PHR)) <= data.size())
			proghead = reinterpret_cast<ELF_PHR*>(&data[header.e_phoff]);
		else
			Console.Error("(ELF) Program header offset %u is larger than file size %zu", header.e_phoff, data.size());
	}

	if (header.e_shnum > 0)
	{
		if ((header.e_shoff + sizeof(ELF_SHR)) <= data.size())
			secthead = reinterpret_cast<ELF_SHR*>(&data[header.e_shoff]);
		else
			Console.Error("(ELF) Section header offset %u is larger than file size %zu", header.e_shoff, data.size());
	}

	if ((header.e_shnum > 0) && (header.e_shentsize != sizeof(ELF_SHR)))
		Console.Error("(ELF) Size of section headers is not standard");

	if ((header.e_phnum > 0) && (header.e_phentsize != sizeof(ELF_PHR)))
		Console.Error("(ELF) Size of program headers is not standard");

	//getCRC();

	const char* elftype = NULL;
	switch( header.e_type )
	{
		default:
			ELF_LOG( "type:      unknown = %x", header.e_type );
			break;

		case 0x0: elftype = "no file type";	break;
		case 0x1: elftype = "relocatable";	break;
		case 0x2: elftype = "executable";	break;
	}

	if (elftype != NULL) ELF_LOG( "type:      %s", elftype );

	const char* machine = NULL;

	switch(header.e_machine)
	{
		case 1: machine = "AT&T WE 32100";	break;
		case 2: machine = "SPARC";			break;
		case 3: machine = "Intel 80386";	break;
		case 4: machine = "Motorola 68000";	break;
		case 5: machine = "Motorola 88000";	break;
		case 7: machine = "Intel 80860";	break;
		case 8: machine = "mips_rs3000";	break;

		default:
			ELF_LOG( "machine:  unknown = %x", header.e_machine );
			break;
	}

	if (machine != NULL) ELF_LOG( "machine:  %s", machine );

	ELF_LOG("version:   %d",header.e_version);
	ELF_LOG("entry:	    %08x",header.e_entry);
	ELF_LOG("flags:     %08x",header.e_flags);
	ELF_LOG("eh size:   %08x",header.e_ehsize);
	ELF_LOG("ph off:    %08x",header.e_phoff);
	ELF_LOG("ph entsiz: %08x",header.e_phentsize);
	ELF_LOG("ph num:    %08x",header.e_phnum);
	ELF_LOG("sh off:    %08x",header.e_shoff);
	ELF_LOG("sh entsiz: %08x",header.e_shentsize);
	ELF_LOG("sh num:    %08x",header.e_shnum);
	ELF_LOG("sh strndx: %08x",header.e_shstrndx);

	ELF_LOG("\n");

	//applyPatches();
}

bool ElfObject::HasValidPSXHeader() const
{
	if (data.size() < sizeof(PSXEXEHeader))
		return false;

	const PSXEXEHeader* header = reinterpret_cast<const PSXEXEHeader*>(data.data());

	static constexpr char expected_id[] = {'P', 'S', '-', 'X', ' ', 'E', 'X', 'E'};
	if (std::memcmp(header->id, expected_id, sizeof(expected_id)) != 0)
		return false;

	if ((header->file_size + sizeof(PSXEXEHeader)) > data.size())
	{
		Console.Warning("Incorrect file size in PS-EXE header: %u bytes should not be greater than %u bytes",
			header->file_size, static_cast<unsigned>(data.size() - sizeof(PSXEXEHeader)));
	}

	return true;
}

bool ElfObject::HasProgramHeaders() const
{
	return (proghead != nullptr);
}

bool ElfObject::HasSectionHeaders() const
{
	return (secthead != nullptr);
}

bool ElfObject::HasHeaders() const
{
	return (HasProgramHeaders() && HasSectionHeaders());
}

u32 ElfObject::GetEntryPoint() const
{
	if (isPSXElf)
	{
		if (HasValidPSXHeader())
			return reinterpret_cast<const PSXEXEHeader*>(data.data())->initial_pc;
		else
			return 0xFFFFFFFFu;
	}
	else
	{
		return GetHeader().e_entry;
	}
}

std::pair<u32,u32> ElfObject::GetTextRange() const
{
	if (!isPSXElf && HasProgramHeaders())
	{
		const ELF_HEADER& header = GetHeader();
		for (int i = 0; i < header.e_phnum; i++)
		{
			const u32 start = proghead[i].p_vaddr;
			const u32 size = proghead[i].p_memsz;

			if (start <= header.e_entry && (start + size) > header.e_entry)
				return std::make_pair(start, size);
		}
	}

	return std::make_pair(0,0);
}

bool ElfObject::CheckElfSize(s64 size, Error* error)
{
	if (size > 0xfffffff)
		Error::SetString(error, "Illegal ELF file size over 2GB!");
	else if (size == -1)
		Error::SetString(error, "ELF file does not exist!");
	else if (size <= static_cast<s64>(sizeof(ELF_HEADER)))
		Error::SetString(error, "Unexpected end of ELF file.");
	else
		return true;

	return false;
}

u32 ElfObject::GetCRC() const
{
	u32 CRC = 0;

	const u32* srcdata = reinterpret_cast<const u32*>(data.data());
	for (u32 i = static_cast<u32>(data.size()) / 4; i; --i, ++srcdata)
		CRC ^= *srcdata;

	return CRC;
}

void ElfObject::LoadProgramHeaders()
{
	if (proghead == NULL) return;

	const ELF_HEADER& header = GetHeader();
	for( int i = 0 ; i < header.e_phnum ; i++ )
	{
		ELF_LOG( "Elf32 Program Header" );
		ELF_LOG( "type:      " );

		switch(proghead[ i ].p_type)
		{
			default:
				ELF_LOG( "unknown %x", (int)proghead[ i ].p_type );
				break;

			case 0x1:
			{
				ELF_LOG("load");
			}
			break;
		}

		ELF_LOG("\n");
		ELF_LOG("offset:    %08x",proghead[i].p_offset);
		ELF_LOG("vaddr:     %08x",proghead[i].p_vaddr);
		ELF_LOG("paddr:     %08x",proghead[i].p_paddr);
		ELF_LOG("file size: %08x",proghead[i].p_filesz);
		ELF_LOG("mem size:  %08x",proghead[i].p_memsz);
		ELF_LOG("flags:     %08x",proghead[i].p_flags);
		ELF_LOG("palign:    %08x",proghead[i].p_align);
		ELF_LOG("\n");
	}
}

void ElfObject::LoadSectionHeaders()
{
	const ELF_HEADER& header = GetHeader();
	if (!secthead || header.e_shoff > data.size())
		return;

	// This function scares me a lot. There's a lot of potential for buffer overreads.
	// All the accesses should be wrapped in bounds checked read() calls.

	const u32 section_names_offset = secthead[(header.e_shstrndx == 0xffff ? 0 : header.e_shstrndx)].sh_offset;
	const u8* sections_names = data.data() + section_names_offset;

	int i_st = -1, i_dt = -1;

	for( int i = 0 ; i < header.e_shnum ; i++ )
	{
		ELF_LOG( "ELF32 Section Header [%x] %s", i, &sections_names[ secthead[ i ].sh_name ] );

		// used by parseCommandLine
		//if ( secthead[i].sh_flags & 0x2 )
		//	args_ptr = std::min( args_ptr, secthead[ i ].sh_addr & 0x1ffffff );

		ELF_LOG("\n");

		const char* sectype = NULL;
		switch(secthead[ i ].sh_type)
		{
			case 0x0: sectype = "null";		break;
			case 0x1: sectype = "progbits";	break;
			case 0x2: sectype = "symtab";	break;
			case 0x3: sectype = "strtab";	break;
			case 0x4: sectype = "rela";		break;
			case 0x8: sectype = "no bits";	break;
			case 0x9: sectype = "rel";		break;

			default:
				ELF_LOG("type:      unknown %08x",secthead[i].sh_type);
			break;
		}

		ELF_LOG("type:      %s", sectype);
		ELF_LOG("flags:     %08x", secthead[i].sh_flags);
		ELF_LOG("addr:      %08x", secthead[i].sh_addr);
		ELF_LOG("offset:    %08x", secthead[i].sh_offset);
		ELF_LOG("size:      %08x", secthead[i].sh_size);
		ELF_LOG("link:      %08x", secthead[i].sh_link);
		ELF_LOG("info:      %08x", secthead[i].sh_info);
		ELF_LOG("addralign: %08x", secthead[i].sh_addralign);
		ELF_LOG("entsize:   %08x", secthead[i].sh_entsize);
		// dump symbol table

		if (secthead[ i ].sh_type == 0x02)
		{
			i_st = i;
			i_dt = secthead[i].sh_link;
		}
	}

	if ((i_st >= 0) && (i_dt >= 0))
	{
		const char* SymNames;
		Elf32_Sym* eS;

		if (secthead[i_dt].sh_offset < data.size() &&
			secthead[i_st].sh_offset < data.size())
		{
			SymNames = (char*)(data.data() + secthead[i_dt].sh_offset);
			eS = (Elf32_Sym*)(data.data() + secthead[i_st].sh_offset);
			Console.WriteLn("found %d symbols", secthead[i_st].sh_size / sizeof(Elf32_Sym));

			R5900SymbolMap.Clear();
			for (uint i = 1; i < (secthead[i_st].sh_size / sizeof(Elf32_Sym)); i++)
			{
				if ((eS[i].st_value != 0) && (ELF32_ST_TYPE(eS[i].st_info) == 2))
				{
					R5900SymbolMap.AddLabel(&SymNames[eS[i].st_name], eS[i].st_value);
				}
			}
		}
	}
}

void ElfObject::LoadHeaders()
{
	if (isPSXElf)
		return;

	LoadProgramHeaders();
	LoadSectionHeaders();
}
