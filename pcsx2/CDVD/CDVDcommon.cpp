// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "CDVD/CDVDcommon.h"
#include "CDVD/IsoReader.h"
#include "CDVD/IsoFileFormats.h"
#include "DebugTools/SymbolMap.h"
#include "Config.h"
#include "Host.h"
#include "IconsFontAwesome5.h"

#include "common/Assertions.h"
#include "common/Console.h"
#include "common/EnumOps.h"
#include "common/FileSystem.h"
#include "common/Path.h"
#include "common/StringUtil.h"

#include <ctype.h>
#include <exception>
#include <memory>
#include <time.h>

#include "fmt/core.h"

// TODO: FIXME! Should be platform specific.
#ifdef _WIN32
#include "common/RedtapeWindows.h"
#endif

#define ENABLE_TIMESTAMPS

const CDVD_API* CDVD = nullptr;

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
	pxAssertMsg(CDVD, "Invalid CDVD object state (null pointer exception)");
}

//////////////////////////////////////////////////////////////////////////////////////////
// Disk Type detection stuff (from cdvdGigaherz)
//
static int CheckDiskTypeFS(int baseType)
{
	IsoReader isor;
	if (isor.Open())
	{
		std::vector<u8> data;
		if (isor.ReadFile("SYSTEM.CNF", &data))
		{
			if (StringUtil::ContainsSubString(data, "BOOT2"))
			{
				// PS2 DVD/CD.
				return (baseType == CDVD_TYPE_DETCTCD) ? CDVD_TYPE_PS2CD : CDVD_TYPE_PS2DVD;
			}

			if (StringUtil::ContainsSubString(data, "BOOT"))
			{
				// PSX CD.
				return CDVD_TYPE_PSCD;
			}

			return CDVD_TYPE_ILLEGAL;
		}

		// PS2 Linux disc 2, doesn't have a System.CNF or a normal ELF
		if (isor.FileExists("P2L_0100.02"))
			return CDVD_TYPE_PS2DVD;

		if (isor.FileExists("PSX.EXE"))
			return CDVD_TYPE_PSCD;

		if (isor.FileExists("VIDEO_TS/VIDEO_TS.IFO"))
			return CDVD_TYPE_DVDV;
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
#ifdef WIN32
	if (Path::IsAbsolute(newfile))
	{
		const auto splitPath = Path::SplitNativePath(newfile);
		// GetDriveType() Requires trailing backslashes
		const auto root = fmt::format("{}\\", splitPath.at(0));

		const auto driveType = GetDriveType(StringUtil::UTF8StringToWideString(root).c_str());
		if (driveType == DRIVE_REMOVABLE)
		{
			Host::AddIconOSDMessage("RemovableDriveWarning", ICON_FA_EXCLAMATION_TRIANGLE,
				TRANSLATE_SV("CDVD", "Game disc location is on a removable drive, performance issues such as jittering "
									 "and freezing may occur."),
				Host::OSD_WARNING_DURATION);
		}
	}
#endif

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
		R5900SymbolMap.SortSymbols();
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

void CDVDsys_ClearFiles()
{
	for (u32 i = 0; i < std::size(m_SourceFilename); i++)
		m_SourceFilename[i] = {};
}

void CDVDsys_ChangeSource(CDVD_SourceType type)
{
	if (CDVD)
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

bool DoCDVDopen(Error* error)
{
	CheckNullCDVD();

	CDVD->newDiskCB(cdvdNewDiskCB);

	auto CurrentSourceType = enum_cast(m_CurrentSourceType);
	if (!CDVD->open(m_SourceFilename[CurrentSourceType], error))
		return false; // error! (handled by caller)

	int cdtype = DoCDVDdetectDiskType();

	if (!EmuConfig.CdvdDumpBlocks || (cdtype == CDVD_TYPE_NODISC))
	{
		blockDumpFile.Close();
		return true;
	}

	std::string dump_name(Path::GetFileTitle(m_SourceFilename[CurrentSourceType]));
	if (dump_name.empty())
		dump_name = "Untitled";

	if (EmuConfig.CurrentBlockdump.empty())
		EmuConfig.CurrentBlockdump = FileSystem::GetWorkingDirectory();

	std::string temp(Path::Combine(EmuConfig.CurrentBlockdump, dump_name));

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

	Host::AddKeyedOSDMessage("BlockDumpCreate",
		fmt::format(TRANSLATE_FS("CDVD", "Saving CDVD block dump to '{}'."), temp), Host::OSD_INFO_DURATION);

	if (blockDumpFile.Create(std::move(temp), 2))
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

	blockDumpFile.Close();

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



static bool NODISCopen(std::string filename, Error* error)
{
	return true;
}

static void NODISCclose()
{
}

static s32 NODISCreadTrack(u32 lsn, int mode)
{
	return -1;
}

static s32 NODISCgetBuffer(u8* buffer)
{
	return -1;
}

static s32 NODISCreadSubQ(u32 lsn, cdvdSubQ* subq)
{
	return -1;
}

static s32 NODISCgetTN(cdvdTN* Buffer)
{
	return -1;
}

static s32 NODISCgetTD(u8 Track, cdvdTD* Buffer)
{
	return -1;
}

static s32 NODISCgetTOC(void* toc)
{
	return -1;
}

static s32 NODISCgetDiskType()
{
	return CDVD_TYPE_NODISC;
}

static s32 NODISCgetTrayStatus()
{
	return CDVD_TRAY_CLOSE;
}

static s32 NODISCdummyS32()
{
	return 0;
}

static void NODISCnewDiskCB(void (*/* callback */)())
{
}

static s32 NODISCreadSector(u8* tempbuffer, u32 lsn, int mode)
{
	return -1;
}

static s32 NODISCgetDualInfo(s32* dualType, u32* _layer1start)
{
	return -1;
}

const CDVD_API CDVDapi_NoDisc =
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
