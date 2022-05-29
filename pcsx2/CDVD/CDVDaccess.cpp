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

#define ENABLE_TIMESTAMPS

#include <ctype.h>
#include <time.h>
#include <exception>
#include <memory>

#include "fmt/core.h"

#include "IsoFS/IsoFS.h"
#include "IsoFS/IsoFSCDVD.h"
#include "IsoFileFormats.h"

#include "common/Assertions.h"
#include "common/Exceptions.h"
#include "common/FileSystem.h"
#include "common/Path.h"
#include "common/StringUtil.h"
#include "DebugTools/SymbolMap.h"
#include "Config.h"
#include "Host.h"

CDVD_API* CDVD = NULL;

// ----------------------------------------------------------------------------
// diskTypeCached
// Internal disc type cache, to reduce the overhead of disc type checks, which are
// performed quite liberally by many games (perhaps intended to keep the PS2 DVD
// from spinning down due to idle activity?).
// Cache is set to -1 for init and when the disc is removed/changed, which invokes
// a new DiskTypeCheck.  All subsequent checks use the non-negative value here.
//
static int diskTypeCached = -1;

// used to bridge the gap between the old getBuffer api and the new getBuffer2 api.
int lastReadSize;
u32 lastLSN; // needed for block dumping

// Records last read block length for block dumping
//static int plsn = 0;

static OutputIsoFile blockDumpFile;

// Assertion check for CDVD != NULL (in devel and debug builds), because its handier than
// relying on DEP exceptions -- and a little more reliable too.
static void CheckNullCDVD()
{
	pxAssertDev(CDVD != NULL, "Invalid CDVD object state (null pointer exception)");
}

//////////////////////////////////////////////////////////////////////////////////////////
// Disk Type detection stuff (from cdvdGigaherz)
//
static int CheckDiskTypeFS(int baseType)
{
	IsoFSCDVD isofs;
	try
	{
		IsoDirectory rootdir(isofs);

		try
		{
			IsoFile file(rootdir, "SYSTEM.CNF;1");

			const int size = file.getLength();
			const std::unique_ptr<char[]> buffer = std::make_unique<char[]>(size + 1);
			file.read(buffer.get(), size);
			buffer[size] = '\0';

			char* pos = strstr(buffer.get(), "BOOT2");
			if (pos == NULL)
			{
				pos = strstr(buffer.get(), "BOOT");
				if (pos == NULL)
					return CDVD_TYPE_ILLEGAL;
				return CDVD_TYPE_PSCD;
			}

			return (baseType == CDVD_TYPE_DETCTCD) ? CDVD_TYPE_PS2CD : CDVD_TYPE_PS2DVD;
		}
		catch (Exception::FileNotFound&)
		{
		}

		// PS2 Linux disc 2, doesn't have a System.CNF or a normal ELF
		try
		{
			IsoFile file(rootdir, "P2L_0100.02;1");
			return CDVD_TYPE_PS2DVD;
		}
		catch (Exception::FileNotFound&)
		{
		}

		try
		{
			IsoFile file(rootdir, "PSX.EXE;1");
			return CDVD_TYPE_PSCD;
		}
		catch (Exception::FileNotFound&)
		{
		}

		try
		{
			IsoFile file(rootdir, "VIDEO_TS/VIDEO_TS.IFO;1");
			return CDVD_TYPE_DVDV;
		}
		catch (Exception::FileNotFound&)
		{
		}
	}
	catch (Exception::FileNotFound&)
	{
	}

#ifdef PCSX2_DEVBUILD
	return CDVD_TYPE_PS2DVD; // need this hack for some homebrew (SMS)
#endif
	return CDVD_TYPE_ILLEGAL; // << Only for discs which aren't ps2 at all.
}

static int FindDiskType(int mType)
{
	int dataTracks = 0;
	int audioTracks = 0;
	int iCDType = mType;
	cdvdTN tn;

	CDVD->getTN(&tn);

	if (tn.strack != tn.etrack) // multitrack == CD.
	{
		iCDType = CDVD_TYPE_DETCTCD;
	}
	else if (mType < 0)
	{
		static u8 bleh[CD_FRAMESIZE_RAW];
		cdvdTD td;

		CDVD->getTD(0, &td);
		if (td.lsn > 452849)
		{
			iCDType = CDVD_TYPE_DETCTDVDS;
		}
		else
		{
			if (DoCDVDreadSector(bleh, 16, CDVD_MODE_2048) == 0)
			{
				//const cdVolDesc& volDesc = (cdVolDesc&)bleh;
				//if(volDesc.rootToc.tocSize == 2048)

				//Horrible hack! in CD images position 166 and 171 have block size but not DVD's
				//It's not always 2048 however (can be 4096)
				//Test Impossible Mission if thia is changed.
				if (*(u16*)(bleh + 166) == *(u16*)(bleh + 171))
					iCDType = CDVD_TYPE_DETCTCD;
				else
					iCDType = CDVD_TYPE_DETCTDVDS;
			}
		}
	}

	if (iCDType == CDVD_TYPE_DETCTDVDS)
	{
		s32 dlt = 0;
		u32 l1s = 0;

		if (CDVD->getDualInfo(&dlt, &l1s) == 0)
		{
			if (dlt > 0)
				iCDType = CDVD_TYPE_DETCTDVDD;
		}
	}

	switch (iCDType)
	{
		case CDVD_TYPE_DETCTCD:
			Console.WriteLn(" * CDVD Disk Open: CD, %d tracks (%d to %d):", tn.etrack - tn.strack + 1, tn.strack, tn.etrack);
			break;

		case CDVD_TYPE_DETCTDVDS:
			Console.WriteLn(" * CDVD Disk Open: DVD, Single layer or unknown:");
			break;

		case CDVD_TYPE_DETCTDVDD:
			Console.WriteLn(" * CDVD Disk Open: DVD, Double layer:");
			break;
	}

	audioTracks = dataTracks = 0;
	for (int i = tn.strack; i <= tn.etrack; i++)
	{
		cdvdTD td, td2;

		CDVD->getTD(i, &td);

		if (tn.etrack > i)
			CDVD->getTD(i + 1, &td2);
		else
			CDVD->getTD(0, &td2);

		int tlength = td2.lsn - td.lsn;

		if (td.type == CDVD_AUDIO_TRACK)
		{
			audioTracks++;
			Console.WriteLn(" * * Track %d: Audio (%d sectors)", i, tlength);
		}
		else
		{
			dataTracks++;
			Console.WriteLn(" * * Track %d: Data (Mode %d) (%d sectors)", i, ((td.type == CDVD_MODE1_TRACK) ? 1 : 2), tlength);
		}
	}

	if (dataTracks > 0)
	{
		iCDType = CheckDiskTypeFS(iCDType);
	}

	if (audioTracks > 0)
	{
		switch (iCDType)
		{
			case CDVD_TYPE_PS2CD:
				iCDType = CDVD_TYPE_PS2CDDA;
				break;
			case CDVD_TYPE_PSCD:
				iCDType = CDVD_TYPE_PSCDDA;
				break;
			default:
				iCDType = CDVD_TYPE_CDDA;
				break;
		}
	}

	return iCDType;
}

static void DetectDiskType()
{
	if (CDVD->getTrayStatus() == CDVD_TRAY_OPEN)
	{
		diskTypeCached = CDVD_TYPE_NODISC;
		return;
	}

	int baseMediaType = CDVD->getDiskType();
	int mType = -1;

	switch (baseMediaType)
	{
#if 0
		case CDVD_TYPE_CDDA:
		case CDVD_TYPE_PSCD:
		case CDVD_TYPE_PS2CD:
		case CDVD_TYPE_PSCDDA:
		case CDVD_TYPE_PS2CDDA:
			mType = CDVD_TYPE_DETCTCD;
			break;

		case CDVD_TYPE_DVDV:
		case CDVD_TYPE_PS2DVD:
			mType = CDVD_TYPE_DETCTDVDS;
			break;

		case CDVD_TYPE_DETCTDVDS:
		case CDVD_TYPE_DETCTDVDD:
		case CDVD_TYPE_DETCTCD:
			mType = baseMediaType;
			break;
#endif

		case CDVD_TYPE_NODISC:
			diskTypeCached = CDVD_TYPE_NODISC;
			return;
	}

	diskTypeCached = FindDiskType(mType);
}

static std::string m_SourceFilename[3];
static CDVD_SourceType m_CurrentSourceType = CDVD_SourceType::NoDisc;

void CDVDsys_SetFile(CDVD_SourceType srctype, std::string newfile)
{
	m_SourceFilename[enum_cast(srctype)] = std::move(newfile);

	// look for symbol file
	if (R5900SymbolMap.IsEmpty())
	{
		std::string symName;
		std::string::size_type n = m_SourceFilename[enum_cast(srctype)].rfind('.');
		if (n == std::string::npos)
			symName = m_SourceFilename[enum_cast(srctype)] + ".sym";
		else
			symName = m_SourceFilename[enum_cast(srctype)].substr(0, n) + ".sym";

		R5900SymbolMap.LoadNocashSym(symName.c_str());
		R5900SymbolMap.UpdateActiveSymbols();
	}
}

const std::string& CDVDsys_GetFile(CDVD_SourceType srctype)
{
	return m_SourceFilename[enum_cast(srctype)];
}

CDVD_SourceType CDVDsys_GetSourceType()
{
	return m_CurrentSourceType;
}

void CDVDsys_ChangeSource(CDVD_SourceType type)
{
	if (CDVD != NULL)
		DoCDVDclose();

	switch (m_CurrentSourceType = type)
	{
		case CDVD_SourceType::Iso:
			CDVD = &CDVDapi_Iso;
			break;

		case CDVD_SourceType::Disc:
			CDVD = &CDVDapi_Disc;
			break;

		case CDVD_SourceType::NoDisc:
			CDVD = &CDVDapi_NoDisc;
			break;

			jNO_DEFAULT;
	}
}

bool DoCDVDopen()
{
	CheckNullCDVD();

	CDVD->newDiskCB(cdvdNewDiskCB);

	// Win32 Fail: the old CDVD api expects MBCS on Win32 platforms, but generating a MBCS
	// from unicode is problematic since we need to know the codepage of the text being
	// converted (which isn't really practical knowledge).  A 'best guess' would be the
	// default codepage of the user's Windows install, but even that will fail and return
	// question marks if the filename is another language.

	//TODO_CDVD check if ISO and Disc use UTF8

	auto CurrentSourceType = enum_cast(m_CurrentSourceType);
	int ret = CDVD->open(!m_SourceFilename[CurrentSourceType].empty() ? m_SourceFilename[CurrentSourceType].c_str() : nullptr);
	if (ret == -1)
		return false; // error! (handled by caller)

	int cdtype = DoCDVDdetectDiskType();

	if (!EmuConfig.CdvdDumpBlocks || (cdtype == CDVD_TYPE_NODISC))
	{
		blockDumpFile.Close();
		return true;
	}

	std::string somepick(Path::StripExtension(FileSystem::GetDisplayNameFromPath(m_SourceFilename[CurrentSourceType])));
	//FWIW Disc serial availability doesn't seem reliable enough, sometimes it's there and sometime it's just null
	//Shouldn't the serial be available all time? Potentially need to look into Elfreloadinfo() reliability
	//TODO: Add extra fallback case for CRC.
	if (somepick.empty() && !DiscSerial.empty())
		somepick = StringUtil::StdStringFromFormat("Untitled-%s", DiscSerial.c_str());
	else if (somepick.empty())
		somepick = "Untitled";

	if (EmuConfig.CurrentBlockdump.empty())
		EmuConfig.CurrentBlockdump = FileSystem::GetWorkingDirectory();

	std::string temp(Path::Combine(EmuConfig.CurrentBlockdump, somepick));

#ifdef ENABLE_TIMESTAMPS
	std::time_t curtime_t = std::time(nullptr);
	struct tm curtime = {};
#ifdef _MSC_VER
	localtime_s(&curtime, &curtime_t);
#else
	localtime_r(&curtime_t, &curtime);
#endif

	temp += StringUtil::StdStringFromFormat(" (%04d-%02d-%02d %02d-%02d-%02d)",
		curtime.tm_year + 1900, curtime.tm_mon + 1, curtime.tm_mday,
		curtime.tm_hour, curtime.tm_min, curtime.tm_sec);
#endif
	temp += ".dump";

	cdvdTD td;
	CDVD->getTD(0, &td);

#ifdef PCSX2_CORE
	Host::AddKeyedOSDMessage("BlockDumpCreate", fmt::format("Saving CDVD block dump to '{}'.", temp), 10.0f);
#endif

	blockDumpFile.Create(std::move(temp), 2);

	if (blockDumpFile.IsOpened())
	{
		int blockofs = 0;
		uint blocksize = CD_FRAMESIZE_RAW;
		uint blocks = td.lsn;

		// hack: Because of limitations of the current cdvd design, we can't query the blocksize
		// of the underlying media.  So lets make a best guess:

		switch (cdtype)
		{
			case CDVD_TYPE_PS2DVD:
			case CDVD_TYPE_DVDV:
			case CDVD_TYPE_DETCTDVDS:
			case CDVD_TYPE_DETCTDVDD:
				blocksize = 2048;
				break;
		}
		blockDumpFile.WriteHeader(blockofs, blocksize, blocks);
	}


	return true;
}

void DoCDVDclose()
{
	CheckNullCDVD();

#ifdef PCSX2_CORE
	// This was commented out, presumably because pausing/resuming in wx reopens CDVD.
	// This is a non-issue in Qt, so we'll leave it behind the ifdef.
	blockDumpFile.Close();
#endif

	if (CDVD->close != NULL)
		CDVD->close();

	DoCDVDresetDiskTypeCache();
}

s32 DoCDVDreadSector(u8* buffer, u32 lsn, int mode)
{
	CheckNullCDVD();
	int ret = CDVD->readSector(buffer, lsn, mode);

	if (ret == 0 && blockDumpFile.IsOpened())
	{
		if (blockDumpFile.GetBlockSize() == CD_FRAMESIZE_RAW && mode != CDVD_MODE_2352)
		{
			u8 blockDumpBuffer[CD_FRAMESIZE_RAW];
			if (CDVD->readSector(blockDumpBuffer, lsn, CDVD_MODE_2352) == 0)
				blockDumpFile.WriteSector(blockDumpBuffer, lsn);
		}
		else
		{
			blockDumpFile.WriteSector(buffer, lsn);
		}
	}

	return ret;
}

s32 DoCDVDreadTrack(u32 lsn, int mode)
{
	CheckNullCDVD();

	// TODO: The CDVD api only uses the new getBuffer style. Why is this temp?
	// lastReadSize is needed for block dumps
	switch (mode)
	{
		case CDVD_MODE_2352:
			lastReadSize = 2352;
			break;
		case CDVD_MODE_2340:
			lastReadSize = 2340;
			break;
		case CDVD_MODE_2328:
			lastReadSize = 2328;
			break;
		case CDVD_MODE_2048:
			lastReadSize = 2048;
			break;
	}

	//DevCon.Warning("CDVD readTrack(lsn=%d,mode=%d)",params lsn, lastReadSize);
	lastLSN = lsn;
	return CDVD->readTrack(lsn, mode);
}

s32 DoCDVDgetBuffer(u8* buffer)
{
	CheckNullCDVD();
	const int ret = CDVD->getBuffer(buffer);

	if (ret == 0 && blockDumpFile.IsOpened())
	{
		cdvdTD td;
		CDVD->getTD(0, &td);

		if (lastLSN >= td.lsn)
			return 0;

		if (blockDumpFile.GetBlockSize() == CD_FRAMESIZE_RAW && lastReadSize != 2352)
		{
			u8 blockDumpBuffer[CD_FRAMESIZE_RAW];
			if (CDVD->readSector(blockDumpBuffer, lastLSN, CDVD_MODE_2352) == 0)
				blockDumpFile.WriteSector(blockDumpBuffer, lastLSN);
		}
		else
		{
			blockDumpFile.WriteSector(buffer, lastLSN);
		}
	}

	return ret;
}

s32 DoCDVDdetectDiskType()
{
	CheckNullCDVD();
	if (diskTypeCached < 0)
		DetectDiskType();
	return diskTypeCached;
}

void DoCDVDresetDiskTypeCache()
{
	diskTypeCached = -1;
}

////////////////////////////////////////////////////////
//
// CDVD null interface for Run BIOS menu



s32 CALLBACK NODISCopen(const char* pTitle)
{
	return 0;
}

void CALLBACK NODISCclose()
{
}

s32 CALLBACK NODISCreadTrack(u32 lsn, int mode)
{
	return -1;
}

s32 CALLBACK NODISCgetBuffer(u8* buffer)
{
	return -1;
}

s32 CALLBACK NODISCreadSubQ(u32 lsn, cdvdSubQ* subq)
{
	return -1;
}

s32 CALLBACK NODISCgetTN(cdvdTN* Buffer)
{
	return -1;
}

s32 CALLBACK NODISCgetTD(u8 Track, cdvdTD* Buffer)
{
	return -1;
}

s32 CALLBACK NODISCgetTOC(void* toc)
{
	return -1;
}

s32 CALLBACK NODISCgetDiskType()
{
	return CDVD_TYPE_NODISC;
}

s32 CALLBACK NODISCgetTrayStatus()
{
	return CDVD_TRAY_CLOSE;
}

s32 CALLBACK NODISCdummyS32()
{
	return 0;
}

void CALLBACK NODISCnewDiskCB(void (*/* callback */)())
{
}

s32 CALLBACK NODISCreadSector(u8* tempbuffer, u32 lsn, int mode)
{
	return -1;
}

s32 CALLBACK NODISCgetDualInfo(s32* dualType, u32* _layer1start)
{
	return -1;
}

CDVD_API CDVDapi_NoDisc =
	{
		NODISCclose,
		NODISCopen,
		NODISCreadTrack,
		NODISCgetBuffer,
		NODISCreadSubQ,
		NODISCgetTN,
		NODISCgetTD,
		NODISCgetTOC,
		NODISCgetDiskType,
		NODISCgetTrayStatus,
		NODISCdummyS32,
		NODISCdummyS32,

		NODISCnewDiskCB,

		NODISCreadSector,
		NODISCgetDualInfo,
};
