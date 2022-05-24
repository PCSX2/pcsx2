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
#include "Common.h"
#include "common/FileSystem.h"
#include "common/StringUtil.h"

#include "GS.h"			// for sending game crc to mtgs
#include "Elfheader.h"
#include "DebugTools/SymbolMap.h"

#ifndef PCSX2_CORE
#include "gui/AppCoreThread.h"
#endif

u32 ElfCRC;
u32 ElfEntry;
std::pair<u32,u32> ElfTextRange;
std::string LastELF;
bool isPSXElf;

// All of ElfObjects functions.
ElfObject::ElfObject(std::string srcfile, IsoFile& isofile, bool isPSXElf)
	: data(isofile.getLength(), "ELF headers")
	, filename(std::move(srcfile))
	, header(*(ELF_HEADER*)data.GetPtr())
{
	checkElfSize(data.GetSizeInBytes());
	readIso(isofile);
	initElfHeaders(isPSXElf);
}

ElfObject::ElfObject(std::string srcfile, u32 hdrsize, bool isPSXElf)
	: data(hdrsize, "ELF headers")
	, filename(std::move(srcfile))
	, header(*(ELF_HEADER*)data.GetPtr())
{
	checkElfSize(data.GetSizeInBytes());
	readFile();
	initElfHeaders(isPSXElf);
}

void ElfObject::initElfHeaders(bool isPSXElf)
{
	if (isPSXElf)
	{
		return;
	}

	DevCon.WriteLn("Initializing Elf: %d bytes", data.GetSizeInBytes());

	if (header.e_phnum > 0)
	{
		if ((header.e_phoff + sizeof(ELF_PHR)) <= static_cast<u32>(data.GetSizeInBytes()))
			proghead = reinterpret_cast<ELF_PHR*>(&data[header.e_phoff]);
		else
			Console.Error("(ELF) Program header offset %u is larger than file size %u", header.e_phoff, data.GetSizeInBytes());
	}

	if (header.e_shnum > 0)
	{
		if ((header.e_shoff + sizeof(ELF_SHR)) <= static_cast<u32>(data.GetSizeInBytes()))
			secthead = reinterpret_cast<ELF_SHR*>(&data[header.e_shoff]);
		else
			Console.Error("(ELF) Section header offset %u is larger than file size %u", header.e_shoff, data.GetSizeInBytes());
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

bool ElfObject::hasProgramHeaders() { return (proghead != NULL); }
bool ElfObject::hasSectionHeaders() { return (secthead != NULL); }
bool ElfObject::hasHeaders() { return (hasProgramHeaders() && hasSectionHeaders()); }

std::pair<u32,u32> ElfObject::getTextRange()
{
	for (int i = 0; i < header.e_phnum; i++)
	{
		u32 start = proghead[i].p_vaddr;
		u32 size = proghead[i].p_memsz;

		if (start <= header.e_entry && (start+size) > header.e_entry)
			return std::make_pair(start,size);
	}
	
	return std::make_pair(0,0);
}

void ElfObject::readIso(IsoFile& file)
{
	int rsize = file.read(data.GetPtr(), data.GetSizeInBytes());
	if (rsize < data.GetSizeInBytes()) throw Exception::EndOfStream(filename);
}

void ElfObject::readFile()
{
	int rsize = 0;
	FILE *f = FileSystem::OpenCFile( filename.c_str(), "rb");
	if (f == NULL) throw Exception::FileNotFound(filename);

	fseek(f, 0, SEEK_SET);
	rsize = fread(data.GetPtr(), 1, data.GetSizeInBytes(), f);
	fclose( f );

	if (rsize < data.GetSizeInBytes()) throw Exception::EndOfStream(filename);
}

static std::string GetMsg_InvalidELF()
{
	return
		"Cannot load ELF binary image.  The file may be corrupt or incomplete."
		"\n\n"
		"If loading from an ISO image, this error may be caused by an unsupported ISO image type or a bug in PCSX2 ISO image support.";
}


void ElfObject::checkElfSize(s64 elfsize)
{
	const char* diagMsg = NULL;
	if		(elfsize > 0xfffffff)	diagMsg = "Illegal ELF file size over 2GB!";
	else if	(elfsize == -1)			diagMsg = "ELF file does not exist!";
	else if	(elfsize == 0)			diagMsg = "Unexpected end of ELF file.";

	if (diagMsg)
		throw Exception::BadStream(filename)
			.SetDiagMsg(diagMsg)
			.SetUserMsg(GetMsg_InvalidELF());
}

u32 ElfObject::getCRC()
{
	u32 CRC = 0;

	const u32* srcdata = (u32*)data.GetPtr();
	for(u32 i=data.GetSizeInBytes()/4; i; --i, ++srcdata)
		CRC ^= *srcdata;

	return CRC;
}

void ElfObject::loadProgramHeaders()
{
	if (proghead == NULL) return;

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

void ElfObject::loadSectionHeaders()
{
	if (secthead == NULL || header.e_shoff > (u32)data.GetLength()) return;

	const u8* sections_names = data.GetPtr( secthead[ (header.e_shstrndx == 0xffff ? 0 : header.e_shstrndx) ].sh_offset );

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
		const char * SymNames;
		Elf32_Sym * eS;

		SymNames = (char*)data.GetPtr(secthead[i_dt].sh_offset);
		eS = (Elf32_Sym*)data.GetPtr(secthead[i_st].sh_offset);
		Console.WriteLn("found %d symbols", secthead[i_st].sh_size / sizeof(Elf32_Sym));

		for(uint i = 1; i < (secthead[i_st].sh_size / sizeof(Elf32_Sym)); i++) {
			if ((eS[i].st_value != 0) && (ELF32_ST_TYPE(eS[i].st_info) == 2))
			{
				R5900SymbolMap.AddLabel(&SymNames[eS[i].st_name],eS[i].st_value);
			}
		}
	}
}

void ElfObject::loadHeaders()
{
	loadProgramHeaders();
	loadSectionHeaders();
}

// return value:
//   0 - Invalid or unknown disc.
//   1 - PS1 CD
//   2 - PS2 CD
int GetPS2ElfName( std::string& name )
{
	int retype = 0;

	try {
		IsoFSCDVD isofs;
		IsoFile file( isofs, "SYSTEM.CNF;1");

		int size = file.getLength();
		if( size == 0 ) return 0;

		while( !file.eof() )
		{
			const std::string line(file.readLine());
			std::string_view key, value;
			if (!StringUtil::ParseAssignmentString(line, &key, &value))
				continue;

			if( value.empty() && file.getLength() != file.getSeekPos() )
			{ // Some games have a character on the last line of the file, don't print the error in those cases.
				Console.Warning( "(SYSTEM.CNF) Unusual or malformed entry in SYSTEM.CNF ignored:" );
				Console.Indent().WriteLn(line);
				continue;
			}

			if( key == "BOOT2" )
			{
				Console.WriteLn( Color_StrongBlue, "(SYSTEM.CNF) Detected PS2 Disc = %.*s",
					static_cast<int>(value.size()), value.data());
				name = value;
				retype = 2;
			}
			else if( key == "BOOT" )
			{
				Console.WriteLn( Color_StrongBlue, "(SYSTEM.CNF) Detected PSX/PSone Disc = %.*s",
					static_cast<int>(value.size()), value.data());
				name = value;
				retype = 1;
			}
			else if( key == "VMODE" )
			{
				Console.WriteLn( Color_Blue, "(SYSTEM.CNF) Disc region type = %.*s",
					static_cast<int>(value.size()), value.data());
			}
			else if( key == "VER" )
			{
				Console.WriteLn( Color_Blue, "(SYSTEM.CNF) Software version = %.*s",
					static_cast<int>(value.size()), value.data());
#ifndef PCSX2_CORE
				GameInfo::gameVersion = StringUtil::UTF8StringToWxString(value);
#endif
			}
		}

		if( retype == 0 )
		{
			Console.Error("(GetElfName) Disc image is *not* a PlayStation or PS2 game!");
			return 0;
		}
	}
	catch( Exception::FileNotFound& )
	{
		//Console.Warning(ex.FormatDiagnosticMessage());
		return 0;		// no SYSTEM.CNF, not a PS1/PS2 disc.
	}
	catch (Exception::BadStream& ex)
	{
		Console.Error(ex.FormatDiagnosticMessage());
		return 0;		// ISO error
	}

	return retype;
}
