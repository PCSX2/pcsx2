// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "CDVD/BlockdumpFileReader.h"
#include "CDVD/ChdFileReader.h"
#include "CDVD/CsoFileReader.h"
#include "CDVD/FlatFileReader.h"
#include "CDVD/GzippedFileReader.h"
#include "CDVD/IsoFileFormats.h"
#include "Config.h"
#include "Host.h"

#include "common/Assertions.h"
#include "common/Console.h"
#include "common/Error.h"
#include "common/Path.h"
#include "common/StringUtil.h"

#include "fmt/format.h"

static const char* nameFromType(int type)
{
	switch (type)
	{
		case ISOTYPE_CD:
			return "CD";
		case ISOTYPE_DVD:
			return "DVD";
		case ISOTYPE_AUDIO:
			return "Audio CD";
		case ISOTYPE_DVDDL:
			return "DVD9 (dual-layer)";
		case ISOTYPE_ILLEGAL:
			return "Illegal media";
		default:
			return "Unknown or corrupt";
	}
}

static std::unique_ptr<ThreadedFileReader> GetFileReader(const std::string& path)
{
	const std::string_view extension = Path::GetExtension(path);

	if (StringUtil::compareNoCase(extension, "chd"))
		return std::make_unique<ChdFileReader>();

	if (StringUtil::compareNoCase(extension, "cso") || StringUtil::compareNoCase(extension, "zso"))
		return std::make_unique<CsoFileReader>();

	if (StringUtil::compareNoCase(extension, "gz"))
		return std::make_unique<GzippedFileReader>();

	if (StringUtil::compareNoCase(extension, "dump"))
		return std::make_unique<BlockdumpFileReader>();

	return std::make_unique<FlatFileReader>();
}

int InputIsoFile::ReadSync(u8* dst, uint lsn)
{
	if (lsn >= m_blocks)
	{
		ERROR_LOG("isoFile error: Block index is past the end of file! ({} >= {}).", lsn, m_blocks);
		return -1;
	}

	return m_reader->ReadSync(dst + m_blockofs, lsn, 1);
}

void InputIsoFile::BeginRead2(uint lsn)
{
	m_current_lsn = lsn;

	if (lsn >= m_blocks)
	{
		// While this usually indicates that the ISO is corrupted, some games do attempt
		// to read past the end of the disc, so don't error here.
		ERROR_LOG("isoFile error: Block index is past the end of file! ({} >= {}).", lsn, m_blocks);
		return;
	}

	// same sector?
	if (lsn == m_read_lsn)
		return;

	m_read_lsn = lsn;

	m_reader->BeginRead(m_readbuffer, m_read_lsn, 1);
	m_read_inprogress = true;
}

int InputIsoFile::FinishRead3(u8* dst, uint mode)
{
	// Do nothing for out of bounds disc sector reads. It prevents some games
	// from hanging (All-Star Baseball 2005, Hello Kitty: Roller Rescue,
	// Hot Wheels: Beat That! (NTSC), Ratchet & Clank 3 (PAL),
	// Test Drive: Eve of Destruction, etc.).
	if (m_current_lsn >= m_blocks)
		return 0;

	if (m_read_inprogress)
	{
		const int ret = m_reader->FinishRead();
		m_read_inprogress = false;

		if (ret <= 0)
		{
			m_read_lsn = -1;
			return -1;
		}
	}

	int _offset = 0;
	int length = 0;

	switch (mode)
	{
		case CDVD_MODE_2352:
			_offset = 0;
			length = 2352;
			break;
		case CDVD_MODE_2340:
			_offset = 12;
			length = 2340;
			break;
		case CDVD_MODE_2328:
			_offset = 24;
			length = 2328;
			break;
		case CDVD_MODE_2048:
			_offset = 24;
			length = 2048;
			break;
	}

	int end1 = m_blockofs + m_blocksize;
	int end2 = _offset + length;
	int end = std::min(end1, end2);

	int diff = m_blockofs - _offset;
	int ndiff = 0;
	if (diff > 0)
	{
		std::memset(dst, 0, diff);
		_offset = m_blockofs;
	}
	else
	{
		ndiff = -diff;
		diff = 0;
	}

	length = end - _offset;

	std::memcpy(dst + diff, m_readbuffer + ndiff, length);

	if (m_type == ISOTYPE_CD && diff >= 12)
	{
		lsn_to_msf(dst + diff - 12, m_current_lsn);
		dst[diff - 9] = 2;
	}

	return 0;
}

InputIsoFile::InputIsoFile()
{
	_init();
}

InputIsoFile::~InputIsoFile()
{
	Close();
}

void InputIsoFile::_init()
{
	m_type = ISOTYPE_ILLEGAL;
	m_flags = 0;

	m_offset = 0;
	m_blockofs = 0;
	m_blocksize = 0;
	m_blocks = 0;

	m_read_inprogress = false;
	m_current_lsn = -1;
	m_read_lsn = -1;
	m_reader.reset();
}

bool InputIsoFile::Open(std::string srcfile, Error* error)
{
	Close();
	m_filename = std::move(srcfile);
	m_reader = GetFileReader(m_filename);
	if (!m_reader->Open(m_filename, error))
	{
		m_reader.reset();
		return false;
	}

	if (!Detect())
	{
		Error::SetStringFmt(error, "Unable to identify the ISO image type for '{}'", Path::GetFileName(m_filename));
		Close();
		return false;
	}

	m_blocks = m_reader->GetBlockCount();

	Console.WriteLn(Color_StrongBlue, "isoFile open ok: %s", m_filename.c_str());

	Console.WriteLn("  Image type  = %s", nameFromType(m_type));
	//Console.WriteLn("  Fileparts   = %u", m_numparts); // Pointless print, it's 1 unless it says otherwise above
	DevCon.WriteLn("  blocks      = %u", m_blocks);
	DevCon.WriteLn("  offset      = %d", m_offset);
	DevCon.WriteLn("  blocksize   = %u", m_blocksize);
	DevCon.WriteLn("  blockoffset = %d", m_blockofs);

	return true;
}

bool InputIsoFile::Precache(ProgressCallback* progress, Error* error)
{
	return m_reader->Precache(progress, error);
}

void InputIsoFile::Close()
{
	if (m_reader)
	{
		m_reader->Close();
		m_reader.reset();
	}

	_init();
}

bool InputIsoFile::IsOpened() const
{
	return m_reader != nullptr;
}

bool InputIsoFile::tryIsoType(u32 size, u32 offset, u32 blockofs)
{
	u8 buf[2456];

	m_blocksize = size;
	m_offset = offset;
	m_blockofs = blockofs;

	m_reader->SetDataOffset(offset);
	m_reader->SetBlockSize(size);

	if (ReadSync(buf, 16) < 0)
		return false;

	if (strncmp((char*)(buf + 25), "CD001", 5)) // Not ISO 9660 compliant
		return false;

	m_type = (*(u16*)(buf + 190) == 2048) ? ISOTYPE_CD : ISOTYPE_DVD;

	return true; // We can deal with this.
}

// based on florin's CDVDbin detection code :)
// Parameter:
//
//
// Returns true if the image is valid/known/supported, or false if not (type == ISOTYPE_ILLEGAL).
bool InputIsoFile::Detect(bool readType)
{
	m_type = ISOTYPE_ILLEGAL;

	// First sanity check: no sane CD image has less than 16 sectors, since that's what
	// we need simply to contain a TOC.  So if the file size is not large enough to
	// accommodate that, it is NOT a CD image --->

	int sectors = m_reader->GetBlockCount();

	if (sectors < 17)
		return false;

	m_blocks = 17;

	if (tryIsoType(2048, 0, 24))
		return true; // ISO 2048
	if (tryIsoType(2336, 0, 16))
		return true; // RAW 2336
	if (tryIsoType(2352, 0, 0))
		return true; // RAW 2352
	if (tryIsoType(2448, 0, 0))
		return true; // RAWQ 2448

	if (tryIsoType(2048, 150 * 2048, 24))
		return true; // NERO ISO 2048
	if (tryIsoType(2352, 150 * 2048, 0))
		return true; // NERO RAW 2352
	if (tryIsoType(2448, 150 * 2048, 0))
		return true; // NERO RAWQ 2448

	m_offset = 0;
	m_blocksize = CD_FRAMESIZE_RAW;
	m_blockofs = 0;
	m_type = ISOTYPE_AUDIO;

	m_reader->SetDataOffset(m_offset);
	m_reader->SetBlockSize(m_blocksize);

	//BUG: This also detects a memory-card-file as a valid Audio-CD ISO... -avih
	return true;
}
