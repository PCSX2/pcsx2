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
#include "common/FileSystem.h"
#include "common/SafeArray.inl"
#include "common/StringUtil.h"
#include <wx/file.h>
#include <wx/dir.h>
#include <wx/stopwatch.h>

#include <array>
#include <chrono>

#include "MemoryCardFile.h"
#include "MemoryCardFolder.h"

#include "System.h"
#include "Config.h"
#include "Host.h"

#include "svnrev.h"

#include <wx/ffile.h>
#include <map>

static const int MCD_SIZE = 1024 * 8 * 16; // Legacy PSX card default size

static const int MC2_MBSIZE = 1024 * 528 * 2; // Size of a single megabyte of card data

static const char* s_folder_mem_card_id_file = "_pcsx2_superblock";

bool FileMcd_Open = false;

// ECC code ported from mymc
// https://sourceforge.net/p/mymc-opl/code/ci/master/tree/ps2mc_ecc.py
// Public domain license

static u32 CalculateECC(u8* buf)
{
	const u8 parity_table[256] = {0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,1,0,0,1,0,1,1,
	0,0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,0,1,1,0,1,0,0,1,1,0,0,1,0,
	1,1,0,1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,0,1,1,
	0,1,0,0,1,1,0,0,1,0,1,1,0,1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,0,
	1,1,0,1,0,0,1,0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,
	0,1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,1,0,0,1,0,
	1,1,0,0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,0,1,1,0,1,0,0,1,1,0,0,
	1,0,1,1,0};

	const u8 column_parity_mask[256] = {0,7,22,17,37,34,51,52,52,51,34,37,17,22,
	7,0,67,68,85,82,102,97,112,119,119,112,97,102,82,85,68,67,82,85,68,67,119,112,
	97,102,102,97,112,119,67,68,85,82,17,22,7,0,52,51,34,37,37,34,51,52,0,7,22,17,
	97,102,119,112,68,67,82,85,85,82,67,68,112,119,102,97,34,37,52,51,7,0,17,22,
	22,17,0,7,51,52,37,34,51,52,37,34,22,17,0,7,7,0,17,22,34,37,52,51,112,119,102,
	97,85,82,67,68,68,67,82,85,97,102,119,112,112,119,102,97,85,82,67,68,68,67,82,
	85,97,102,119,112,51,52,37,34,22,17,0,7,7,0,17,22,34,37,52,51,34,37,52,51,7,0,
	17,22,22,17,0,7,51,52,37,34,97,102,119,112,68,67,82,85,85,82,67,68,112,119,102,
	97,17,22,7,0,52,51,34,37,37,34,51,52,0,7,22,17,82,85,68,67,119,112,97,102,102,
	97,112,119,67,68,85,82,67,68,85,82,102,97,112,119,119,112,97,102,82,85,68,67,
	0,7,22,17,37,34,51,52,52,51,34,37,17,22,7,0};

	u8 column_parity = 0x77;
	u8 line_parity_0 = 0x7F;
	u8 line_parity_1 = 0x7F;

	for (int i = 0; i < 128; i++)
	{
		u8 b = buf[i];
		column_parity ^= column_parity_mask[b];
		if (parity_table[b])
		{
			line_parity_0 ^= ~i;
			line_parity_1 ^= i;
		}
	}

	return column_parity | (line_parity_0 << 8) | (line_parity_1 << 16);
}

static bool ConvertNoECCtoRAW(wxString file_in, wxString file_out)
{
	bool result = false;
	wxFFile fin(file_in, "rb");

	if (fin.IsOpened())
	{
		wxFFile fout(file_out, "wb");

		if (fout.IsOpened())
		{
			u8 buffer[512];
			size_t size = fin.Length();

			for (size_t i = 0; i < (size / 512); i++)
			{
				fin.Read(buffer, 512);
				fout.Write(buffer, 512);

				for (int j = 0; j < 4; j++)
				{
					u32 checksum = CalculateECC(&buffer[j * 128]);
					fout.Write(&checksum, 3);
				}

				fout.Write("\0\0\0\0", 4);
			}

			result = true;
		}
	}

	return result;
}

static bool ConvertRAWtoNoECC(wxString file_in, wxString file_out)
{
	bool result = false;
	wxFFile fout(file_out, "wb");

	if (fout.IsOpened())
	{
		wxFFile fin(file_in, "rb");

		if (fin.IsOpened())
		{
			u8 buffer[512];
			size_t size = fin.Length();

			for (size_t i = 0; i < (size / 528); i++)
			{
				fin.Read(buffer, 512);
				fout.Write(buffer, 512);
				fin.Read(buffer, 16);
			}

			result = true;
		}
	}

	return result;
}

// --------------------------------------------------------------------------------------
//  FileMemoryCard
// --------------------------------------------------------------------------------------
// Provides thread-safe direct file IO mapping.
//
class FileMemoryCard
{
protected:
	wxFFile m_file[8];
	u8 m_effeffs[528 * 16];
	SafeArray<u8> m_currentdata;
	u64 m_chksum[8];
	bool m_ispsx[8];
	u32 m_chkaddr;

public:
	FileMemoryCard();
	virtual ~FileMemoryCard() = default;

	void Lock();
	void Unlock();

	void Open();
	void Close();

	s32 IsPresent(uint slot);
	void GetSizeInfo(uint slot, McdSizeInfo& outways);
	bool IsPSX(uint slot);
	s32 Read(uint slot, u8* dest, u32 adr, int size);
	s32 Save(uint slot, const u8* src, u32 adr, int size);
	s32 EraseBlock(uint slot, u32 adr);
	u64 GetCRC(uint slot);

protected:
	bool Seek(wxFFile& f, u32 adr);
	bool Create(const wxString& mcdFile, uint sizeInMB);

	wxString GetDisabledMessage(uint slot) const
	{
		return wxsFormat(pxE(L"The PS2-slot %d has been automatically disabled.  You can correct the problem\nand re-enable it at any time using Config:Memory cards from the main menu."), slot //TODO: translate internal slot index to human-readable slot description
		);
	}
};

uint FileMcd_GetMtapPort(uint slot)
{
	switch (slot)
	{
		case 0:
		case 2:
		case 3:
		case 4:
			return 0;
		case 1:
		case 5:
		case 6:
		case 7:
			return 1;

			jNO_DEFAULT
	}

	return 0; // technically unreachable.
}

// Returns the multitap slot number, range 1 to 3 (slot 0 refers to the standard
// 1st and 2nd player slots).
uint FileMcd_GetMtapSlot(uint slot)
{
	switch (slot)
	{
		case 0:
		case 1:
			pxFailDev("Invalid parameter in call to GetMtapSlot -- specified slot is one of the base slots, not a Multitap slot.");
			break;

		case 2:
		case 3:
		case 4:
			return slot - 1;
		case 5:
		case 6:
		case 7:
			return slot - 4;

			jNO_DEFAULT
	}

	return 0; // technically unreachable.
}

bool FileMcd_IsMultitapSlot(uint slot)
{
	return (slot > 1);
}
/*
wxFileName FileMcd_GetSimpleName(uint slot)
{
	if( FileMcd_IsMultitapSlot(slot) )
		return g_Conf->Folders.MemoryCards + wxsFormat( L"Mcd-Multitap%u-Slot%02u.ps2", FileMcd_GetMtapPort(slot)+1, FileMcd_GetMtapSlot(slot)+1 );
	else
		return g_Conf->Folders.MemoryCards + wxsFormat( L"Mcd%03u.ps2", slot+1 );
}
*/
std::string FileMcd_GetDefaultName(uint slot)
{
	if (FileMcd_IsMultitapSlot(slot))
		return StringUtil::StdStringFromFormat("Mcd-Multitap%u-Slot%02u.ps2", FileMcd_GetMtapPort(slot) + 1, FileMcd_GetMtapSlot(slot) + 1);
	else
		return StringUtil::StdStringFromFormat("Mcd%03u.ps2", slot + 1);
}

FileMemoryCard::FileMemoryCard()
{
	memset8<0xff>(m_effeffs);
	m_chkaddr = 0;
}

void FileMemoryCard::Open()
{
	for (int slot = 0; slot < 8; ++slot)
	{
		if (FileMcd_IsMultitapSlot(slot))
		{
			if (!EmuConfig.MultitapPort0_Enabled && (FileMcd_GetMtapPort(slot) == 0))
				continue;
			if (!EmuConfig.MultitapPort1_Enabled && (FileMcd_GetMtapPort(slot) == 1))
				continue;
		}

		wxFileName fname(EmuConfig.FullpathToMcd(slot));
		wxString str(fname.GetFullPath());
		bool cont = false;

		if (fname.GetFullName().IsEmpty())
		{
			str = L"[empty filename]";
			cont = true;
		}

		if (!EmuConfig.Mcd[slot].Enabled)
		{
			str = L"[disabled]";
			cont = true;
		}

		if (EmuConfig.Mcd[slot].Type != MemoryCardType::File)
		{
			str = L"[is not memcard file]";
			cont = true;
		}

		Console.WriteLn(cont ? Color_Gray : Color_Green, L"McdSlot %u [File]: " + str, slot);
		if (cont)
			continue;

		const wxULongLong fsz = fname.GetSize();
		if ((fsz == 0) || (fsz == wxInvalidSize))
		{
			// FIXME : Ideally this should prompt the user for the size of the
			// memory card file they would like to create, instead of trying to
			// create one automatically.

			if (!Create(str, 8))
			{
#ifndef PCSX2_CORE
				Msgbox::Alert(
					wxsFormat(_("Could not create a memory card: \n\n%s\n\n"), str.c_str()) +
					GetDisabledMessage(slot));
#endif
			}
		}

		// [TODO] : Add memcard size detection and report it to the console log.
		//   (8MB, 256Mb, formatted, unformatted, etc ...)

#ifdef __WXMSW__
		NTFS_CompressFile(str, EmuConfig.McdCompressNTFS);
#endif

		if (str.EndsWith(".bin"))
		{
			wxString newname = str + "x";
			if (!ConvertNoECCtoRAW(str, newname))
			{
				Console.Error(L"Could convert memory card: " + str);
				wxRemoveFile(newname);
				continue;
			}
			str = newname;
		}

		if (!m_file[slot].Open(str.c_str(), L"r+b"))
		{
			// Translation note: detailed description should mention that the memory card will be disabled
			// for the duration of this session.
#ifndef PCSX2_CORE
			Msgbox::Alert(
				wxsFormat(_("Access denied to memory card: \n\n%s\n\n"), str.c_str()) +
				GetDisabledMessage(slot));
#endif
		}
		else // Load checksum
		{
			m_ispsx[slot] = m_file[slot].Length() == 0x20000;
			m_chkaddr = 0x210;

			if (!m_ispsx[slot] && !!m_file[slot].Seek(m_chkaddr))
				m_file[slot].Read(&m_chksum[slot], 8);
		}
	}
}

void FileMemoryCard::Close()
{
	for (int slot = 0; slot < 8; ++slot)
	{
		if (m_file[slot].IsOpened())
		{
			// Store checksum
			if (!m_ispsx[slot] && !!m_file[slot].Seek(m_chkaddr))
				m_file[slot].Write(&m_chksum[slot], 8);

			m_file[slot].Close();

			if (m_file[slot].GetName().EndsWith(".binx"))
			{
				wxString name = m_file[slot].GetName();
				wxString name_old = name.SubString(0, name.Last('.')) + "bin";
				if (ConvertRAWtoNoECC(name, name_old))
					wxRemoveFile(name);
			}
		}
	}
}

// Returns FALSE if the seek failed (is outside the bounds of the file).
bool FileMemoryCard::Seek(wxFFile& f, u32 adr)
{
	const u32 size = f.Length();

	// If anyone knows why this filesize logic is here (it appears to be related to legacy PSX
	// cards, perhaps hacked support for some special emulator-specific memcard formats that
	// had header info?), then please replace this comment with something useful.  Thanks!  -- air

	u32 offset = 0;

	if (size == MCD_SIZE + 64)
		offset = 64;
	else if (size == MCD_SIZE + 3904)
		offset = 3904;
	else
	{
		// perform sanity checks here?
	}

	return f.Seek(adr + offset);
}

// returns FALSE if an error occurred (either permission denied or disk full)
bool FileMemoryCard::Create(const wxString& mcdFile, uint sizeInMB)
{
	//int enc[16] = {0x77,0x7f,0x7f,0x77,0x7f,0x7f,0x77,0x7f,0x7f,0x77,0x7f,0x7f,0,0,0,0};

	Console.WriteLn(L"(FileMcd) Creating new %uMB memory card: " + mcdFile, sizeInMB);

	wxFFile fp(mcdFile, L"wb");
	if (!fp.IsOpened())
		return false;

	for (uint i = 0; i < (MC2_MBSIZE * sizeInMB) / sizeof(m_effeffs); i++)
	{
		if (fp.Write(m_effeffs, sizeof(m_effeffs)) == 0)
			return false;
	}
	return true;
}

s32 FileMemoryCard::IsPresent(uint slot)
{
	return m_file[slot].IsOpened();
}

void FileMemoryCard::GetSizeInfo(uint slot, McdSizeInfo& outways)
{
	outways.SectorSize = 512;             // 0x0200
	outways.EraseBlockSizeInSectors = 16; // 0x0010
	outways.Xor = 18;                     // 0x12, XOR 02 00 00 10

	if (pxAssert(m_file[slot].IsOpened()))
		outways.McdSizeInSectors = m_file[slot].Length() / (outways.SectorSize + outways.EraseBlockSizeInSectors);
	else
		outways.McdSizeInSectors = 0x4000;

	u8* pdata = (u8*)&outways.McdSizeInSectors;
	outways.Xor ^= pdata[0] ^ pdata[1] ^ pdata[2] ^ pdata[3];
}

bool FileMemoryCard::IsPSX(uint slot)
{
	return m_ispsx[slot];
}

s32 FileMemoryCard::Read(uint slot, u8* dest, u32 adr, int size)
{
	wxFFile& mcfp(m_file[slot]);
	if (!mcfp.IsOpened())
	{
		DevCon.Error("(FileMcd) Ignoring attempted read from disabled slot.");
		memset(dest, 0, size);
		return 1;
	}
	if (!Seek(mcfp, adr))
		return 0;
	return mcfp.Read(dest, size) != 0;
}

s32 FileMemoryCard::Save(uint slot, const u8* src, u32 adr, int size)
{
	wxFFile& mcfp(m_file[slot]);

	if (!mcfp.IsOpened())
	{
		DevCon.Error("(FileMcd) Ignoring attempted save/write to disabled slot.");
		return 1;
	}

	if (m_ispsx[slot])
	{
		m_currentdata.MakeRoomFor(size);
		for (int i = 0; i < size; i++)
			m_currentdata[i] = src[i];
	}
	else
	{
		if (!Seek(mcfp, adr))
			return 0;
		m_currentdata.MakeRoomFor(size);
		mcfp.Read(m_currentdata.GetPtr(), size);


		for (int i = 0; i < size; i++)
		{
			if ((m_currentdata[i] & src[i]) != src[i])
				Console.Warning("(FileMcd) Warning: writing to uncleared data. (%d) [%08X]", slot, adr);
			m_currentdata[i] &= src[i];
		}

		// Checksumness
		{
			if (adr == m_chkaddr)
				Console.Warning("(FileMcd) Warning: checksum sector overwritten. (%d)", slot);

			u64* pdata = (u64*)&m_currentdata[0];
			u32 loops = size / 8;

			for (u32 i = 0; i < loops; i++)
				m_chksum[slot] ^= pdata[i];
		}
	}

	if (!Seek(mcfp, adr))
		return 0;

	int status = mcfp.Write(m_currentdata.GetPtr(), size);

	if (status)
	{
		static auto last = std::chrono::time_point<std::chrono::system_clock>();

		std::chrono::duration<float> elapsed = std::chrono::system_clock::now() - last;
		if (elapsed > std::chrono::seconds(5))
		{
			wxString name, ext;
			wxFileName::SplitPath(m_file[slot].GetName(), NULL, NULL, &name, &ext);
			Host::AddOSDMessage(StringUtil::StdStringFromFormat("Memory Card %s written.", (const char*)(name + "." + ext).c_str()), 10.0f);
			last = std::chrono::system_clock::now();
		}
		return 1;
	}

	return 0;
}

s32 FileMemoryCard::EraseBlock(uint slot, u32 adr)
{
	wxFFile& mcfp(m_file[slot]);

	if (!mcfp.IsOpened())
	{
		DevCon.Error("MemoryCard: Ignoring erase for disabled slot.");
		return 1;
	}

	if (!Seek(mcfp, adr))
		return 0;
	return mcfp.Write(m_effeffs, sizeof(m_effeffs)) != 0;
}

u64 FileMemoryCard::GetCRC(uint slot)
{
	wxFFile& mcfp(m_file[slot]);
	if (!mcfp.IsOpened())
		return 0;

	u64 retval = 0;

	if (m_ispsx[slot])
	{
		if (!Seek(mcfp, 0))
			return 0;

		// Process the file in 4k chunks.  Speeds things up significantly.

		u64 buffer[528 * 8]; // use 528 (sector size), ensures even divisibility

		const uint filesize = mcfp.Length() / sizeof(buffer);
		for (uint i = filesize; i; --i)
		{
			mcfp.Read(&buffer, sizeof(buffer));
			for (uint t = 0; t < std::size(buffer); ++t)
				retval ^= buffer[t];
		}
	}
	else
	{
		retval = m_chksum[slot];
	}

	return retval;
}

// --------------------------------------------------------------------------------------
//  MemoryCard Component API Bindings
// --------------------------------------------------------------------------------------
namespace Mcd
{
	FileMemoryCard impl; // class-based implementations we refer to when API is invoked
	FolderMemoryCardAggregator implFolder;
}; // namespace Mcd

uint FileMcd_ConvertToSlot(uint port, uint slot)
{
	if (slot == 0)
		return port;
	if (port == 0)
		return slot + 1; // multitap 1
	return slot + 4;     // multitap 2
}

void FileMcd_EmuOpen()
{
	if(FileMcd_Open)
		return;
	FileMcd_Open = true;
	// detect inserted memory card types
	for (uint slot = 0; slot < 8; ++slot)
	{
		if (EmuConfig.Mcd[slot].Enabled)
		{
			MemoryCardType type = MemoryCardType::File; // default to file if we can't find anything at the path so it gets auto-generated

			const wxString path = EmuConfig.FullpathToMcd(slot);
			if (wxFileExists(path))
			{
				type = MemoryCardType::File;
			}
			else if (wxDirExists(path))
			{
				type = MemoryCardType::Folder;
			}

			EmuConfig.Mcd[slot].Type = type;
		}
	}

	Mcd::impl.Open();
	Mcd::implFolder.SetFiltering(EmuConfig.McdFolderAutoManage);
	Mcd::implFolder.Open();
}

void FileMcd_EmuClose()
{
	if(!FileMcd_Open)
		return;
	FileMcd_Open = false;
	Mcd::implFolder.Close();
	Mcd::impl.Close();
}

s32 FileMcd_IsPresent(uint port, uint slot)
{
	const uint combinedSlot = FileMcd_ConvertToSlot(port, slot);
	switch (EmuConfig.Mcd[combinedSlot].Type)
	{
		case MemoryCardType::File:
			return Mcd::impl.IsPresent(combinedSlot);
		case MemoryCardType::Folder:
			return Mcd::implFolder.IsPresent(combinedSlot);
		default:
			return false;
	}
}

void FileMcd_GetSizeInfo(uint port, uint slot, McdSizeInfo* outways)
{
	const uint combinedSlot = FileMcd_ConvertToSlot(port, slot);
	switch (EmuConfig.Mcd[combinedSlot].Type)
	{
		case MemoryCardType::File:
			Mcd::impl.GetSizeInfo(combinedSlot, *outways);
			break;
		case MemoryCardType::Folder:
			Mcd::implFolder.GetSizeInfo(combinedSlot, *outways);
			break;
		default:
			return;
	}
}

bool FileMcd_IsPSX(uint port, uint slot)
{
	const uint combinedSlot = FileMcd_ConvertToSlot(port, slot);
	switch (EmuConfig.Mcd[combinedSlot].Type)
	{
		case MemoryCardType::File:
			return Mcd::impl.IsPSX(combinedSlot);
		case MemoryCardType::Folder:
			return Mcd::implFolder.IsPSX(combinedSlot);
		default:
			return false;
	}
}

s32 FileMcd_Read(uint port, uint slot, u8* dest, u32 adr, int size)
{
	const uint combinedSlot = FileMcd_ConvertToSlot(port, slot);
	switch (EmuConfig.Mcd[combinedSlot].Type)
	{
		case MemoryCardType::File:
			return Mcd::impl.Read(combinedSlot, dest, adr, size);
		case MemoryCardType::Folder:
			return Mcd::implFolder.Read(combinedSlot, dest, adr, size);
		default:
			return 0;
	}
}

s32 FileMcd_Save(uint port, uint slot, const u8* src, u32 adr, int size)
{
	const uint combinedSlot = FileMcd_ConvertToSlot(port, slot);
	switch (EmuConfig.Mcd[combinedSlot].Type)
	{
		case MemoryCardType::File:
			return Mcd::impl.Save(combinedSlot, src, adr, size);
		case MemoryCardType::Folder:
			return Mcd::implFolder.Save(combinedSlot, src, adr, size);
		default:
			return 0;
	}
}

s32 FileMcd_EraseBlock(uint port, uint slot, u32 adr)
{
	const uint combinedSlot = FileMcd_ConvertToSlot(port, slot);
	switch (EmuConfig.Mcd[combinedSlot].Type)
	{
		case MemoryCardType::File:
			return Mcd::impl.EraseBlock(combinedSlot, adr);
		case MemoryCardType::Folder:
			return Mcd::implFolder.EraseBlock(combinedSlot, adr);
		default:
			return 0;
	}
}

u64 FileMcd_GetCRC(uint port, uint slot)
{
	const uint combinedSlot = FileMcd_ConvertToSlot(port, slot);
	switch (EmuConfig.Mcd[combinedSlot].Type)
	{
		case MemoryCardType::File:
			return Mcd::impl.GetCRC(combinedSlot);
		case MemoryCardType::Folder:
			return Mcd::implFolder.GetCRC(combinedSlot);
		default:
			return 0;
	}
}

void FileMcd_NextFrame(uint port, uint slot)
{
	const uint combinedSlot = FileMcd_ConvertToSlot(port, slot);
	switch (EmuConfig.Mcd[combinedSlot].Type)
	{
		//case MemoryCardType::MemoryCard_File:
		//	Mcd::impl.NextFrame( combinedSlot );
		//	break;
		case MemoryCardType::Folder:
			Mcd::implFolder.NextFrame(combinedSlot);
			break;
		default:
			return;
	}
}

bool FileMcd_ReIndex(uint port, uint slot, const wxString& filter)
{
	const uint combinedSlot = FileMcd_ConvertToSlot(port, slot);
	switch (EmuConfig.Mcd[combinedSlot].Type)
	{
		//case MemoryCardType::File:
		//	return Mcd::impl.ReIndex( combinedSlot, filter );
		//	break;
		case MemoryCardType::Folder:
			return Mcd::implFolder.ReIndex(combinedSlot, EmuConfig.McdFolderAutoManage, filter);
			break;
		default:
			return false;
	}
}

// --------------------------------------------------------------------------------------
//  Library API Implementations
// --------------------------------------------------------------------------------------

//Tests if a string is a valid name for a new file within a specified directory.
//returns true if:
//     - the file name has a minimum length of minNumCharacters chars (default is 5 chars: at least 1 char + '.' + 3-chars extension)
// and - the file name is within the basepath directory (doesn't contain .. , / , \ , etc)
// and - file name doesn't already exist
// and - can be created on current system (it is actually created and deleted for this test).
bool isValidNewFilename(wxString filenameStringToTest, wxDirName atBasePath, wxString& out_errorMessage, uint minNumCharacters)
{
	if (filenameStringToTest.Length() < 1 || filenameStringToTest.Length() < minNumCharacters)
	{
		out_errorMessage = _("File name empty or too short");
		return false;
	}

	if ((atBasePath + wxFileName(filenameStringToTest)).GetFullPath() != (atBasePath + wxFileName(filenameStringToTest).GetFullName()).GetFullPath())
	{
		out_errorMessage = _("File name outside of required directory");
		return false;
	}

	if (wxFileExists((atBasePath + wxFileName(filenameStringToTest)).GetFullPath()))
	{
		out_errorMessage = _("File name already exists");
		return false;
	}
	if (wxDirExists((atBasePath + wxFileName(filenameStringToTest)).GetFullPath()))
	{
		out_errorMessage = _("File name already exists");
		return false;
	}

	wxFile fp;
	if (!fp.Create((atBasePath + wxFileName(filenameStringToTest)).GetFullPath()))
	{
		out_errorMessage = _("The Operating-System prevents this file from being created");
		return false;
	}
	fp.Close();
	wxRemoveFile((atBasePath + wxFileName(filenameStringToTest)).GetFullPath());

	out_errorMessage = L"[OK - New file name is valid]"; //shouldn't be displayed on success, hence not translatable.
	return true;
}

static MemoryCardFileType GetMemoryCardFileTypeFromSize(s64 size)
{
	if (size == (8 * MC2_MBSIZE))
		return MemoryCardFileType::PS2_8MB;
	else if (size == (16 * MC2_MBSIZE))
		return MemoryCardFileType::PS2_16MB;
	else if (size == (32 * MC2_MBSIZE))
		return MemoryCardFileType::PS2_32MB;
	else if (size == (64 * MC2_MBSIZE))
		return MemoryCardFileType::PS2_64MB;
	else if (size == MCD_SIZE)
		return MemoryCardFileType::PS1;
	else
		return MemoryCardFileType::Unknown;
}

static bool IsMemoryCardFolder(const std::string& path)
{
	const std::string superblock_path(Path::CombineStdString(path, s_folder_mem_card_id_file));
	return FileSystem::FileExists(superblock_path.c_str());
}

std::vector<AvailableMcdInfo> FileMcd_GetAvailableCards(bool include_in_use_cards)
{
	std::vector<FILESYSTEM_FIND_DATA> files;
	FileSystem::FindFiles(EmuFolders::MemoryCards.ToUTF8(), "*",
		FILESYSTEM_FIND_FILES | FILESYSTEM_FIND_FOLDERS | FILESYSTEM_FIND_HIDDEN_FILES, &files);

	std::vector<AvailableMcdInfo> mcds;
	mcds.reserve(files.size());

	for (FILESYSTEM_FIND_DATA& fd : files)
	{
		std::string basename(FileSystem::GetFileNameFromPath(fd.FileName));
		if (!include_in_use_cards)
		{
			bool in_use = false;
			for (size_t i = 0; i < std::size(EmuConfig.Mcd); i++)
			{
				if (EmuConfig.Mcd[i].Filename == basename)
				{
					in_use = true;
					break;
				}
			}
			if (in_use)
				continue;
		}

		if (fd.Attributes & FILESYSTEM_FILE_ATTRIBUTE_DIRECTORY)
		{
			if (!IsMemoryCardFolder(fd.FileName))
				continue;

			mcds.push_back({std::move(basename), std::move(fd.FileName), MemoryCardType::Folder,
				MemoryCardFileType::Unknown, 0u});
		}
		else
		{
			if (fd.Size < MCD_SIZE)
				continue;

			mcds.push_back({std::move(basename), std::move(fd.FileName), MemoryCardType::File,
				GetMemoryCardFileTypeFromSize(fd.Size), static_cast<u32>(fd.Size)});
		}
	}

	return mcds;
}

std::optional<AvailableMcdInfo> FileMcd_GetCardInfo(const std::string_view& name)
{
	std::optional<AvailableMcdInfo> ret;

	std::string basename(name);
	std::string path(Path::CombineStdString(EmuFolders::MemoryCards, basename));

	FILESYSTEM_STAT_DATA sd;
	if (!FileSystem::StatFile(path.c_str(), &sd))
		return ret;

	if (sd.Attributes & FILESYSTEM_FILE_ATTRIBUTE_DIRECTORY)
	{
		if (IsMemoryCardFolder(path))
		{
			ret = {std::move(basename), std::move(path), MemoryCardType::Folder,
				MemoryCardFileType::Unknown, 0u};
		}
	}
	else
	{
		if (sd.Size >= MCD_SIZE)
		{
			ret = {std::move(basename), std::move(path), MemoryCardType::File,
				GetMemoryCardFileTypeFromSize(sd.Size), static_cast<u32>(sd.Size)};
		}
	}

	return ret;
}

bool FileMcd_CreateNewCard(const std::string_view& name, MemoryCardType type, MemoryCardFileType file_type)
{
	const std::string full_path(Path::CombineStdString(EmuFolders::MemoryCards, name));

	if (type == MemoryCardType::Folder)
	{
		Console.WriteLn("(FileMcd) Creating new PS2 folder memory card: '%*s'", static_cast<int>(name.size()), name.data());

		if (!FileSystem::CreateDirectoryPath(full_path.c_str(), false))
		{
			Host::ReportFormattedErrorAsync("Memory Card Creation Failed", "Failed to create directory '%s'.", full_path.c_str());
			return false;
		}

		// write the superblock
		auto fp = FileSystem::OpenManagedCFile(Path::CombineStdString(full_path, s_folder_mem_card_id_file).c_str(), "wb");
		if (!fp)
		{
			Host::ReportFormattedErrorAsync("Memory Card Creation Failed", "Failed to write memory card folder superblock '%s'.", full_path.c_str());
			return false;
		}

		return true;
	}

	if (type == MemoryCardType::File)
	{
		if (file_type <= MemoryCardFileType::Unknown || file_type >= MemoryCardFileType::MaxCount)
			return false;

		static constexpr std::array<u32, static_cast<size_t>(MemoryCardFileType::MaxCount)> sizes = {{0, 8 * MC2_MBSIZE, 16 * MC2_MBSIZE, 32 * MC2_MBSIZE, 64 * MC2_MBSIZE, MCD_SIZE}};

		const bool isPSX = (type == MemoryCardType::File && file_type == MemoryCardFileType::PS1);
		const u32 size = sizes[static_cast<u32>(file_type)];
		if (!isPSX && size == 0)
			return false;

		auto fp = FileSystem::OpenManagedCFile(full_path.c_str(), "wb");
		if (!fp)
		{
			Host::ReportFormattedErrorAsync("Memory Card Creation Failed", "Failed to open file '%s'.", full_path.c_str());
			return false;
		}

		if (!isPSX)
		{
			Console.WriteLn("(FileMcd) Creating new PS2 %uMB memory card: '%s'", size / MC2_MBSIZE, full_path.c_str());

			// PS2 Memory Card
			u8 m_effeffs[528 * 16];
			memset8<0xff>(m_effeffs);

			const u32 count = size / sizeof(m_effeffs);
			for (uint i = 0; i < count; i++)
			{
				if (std::fwrite(m_effeffs, sizeof(m_effeffs), 1, fp.get()) != 1)
				{
					Host::ReportFormattedErrorAsync("Memory Card Creation Failed", "Failed to write file '%s'.", full_path.c_str());
					return false;
				}
			}

			return true;
		}
		else
		{
			Console.WriteLn("(FileMcd) Creating new PSX 128 KiB memory card: '%s'", full_path.c_str());

			// PSX Memory Card; 8192 is the size in bytes of a single block of a PSX memory card (8 KiB).
			u8 m_effeffs_psx[8192];
			memset8<0xff>(m_effeffs_psx);

			// PSX cards consist of 16 blocks, each 8 KiB in size.
			for (uint i = 0; i < 16; i++)
			{
				if (std::fwrite(m_effeffs_psx, sizeof(m_effeffs_psx), 1, fp.get()) != 1)
				{
					Host::ReportFormattedErrorAsync("Memory Card Creation Failed", "Failed to write file '%s'.", full_path.c_str());
					return false;
				}
			}

			return true;
		}
	}

	return false;
}
