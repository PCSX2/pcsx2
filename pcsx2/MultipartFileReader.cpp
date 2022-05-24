/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2014  PCSX2 Dev Team
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
#include "AsyncFileReader.h"
#include "common/Assertions.h"
#include "common/FileSystem.h"
#include "common/Path.h"
#include "common/StringUtil.h"

// Tests for a filename extension in both upper and lower case, if the filesystem happens
// to be case-sensitive.
static bool pxFileExists_WithExt( std::string& filename, const std::string& ext )
{
	std::string temp(Path::ReplaceExtension(filename, StringUtil::toLower(ext)));
	if (FileSystem::FileExists(temp.c_str()))
		return true;

#if defined(_WIN32) || defined(__DARWIN__)
	temp = Path::ReplaceExtension(filename, StringUtil::toUpper(ext));
	if (FileSystem::FileExists(temp.c_str()))
	{
		// make sure we open the correct one
		filename = std::move(temp);
		return true;
	}
#endif

	return false;
}

AsyncFileReader* MultipartFileReader::DetectMultipart(AsyncFileReader* reader)
{
	MultipartFileReader* multi = new MultipartFileReader(reader);

	multi->FindParts();
	if (multi->m_numparts > 1)
	{
		Console.WriteLn( Color_Blue, "isoFile: multi-part ISO detected.  %u parts found.", multi->m_numparts);

		return multi;
	}

	multi->m_parts[0].reader = NULL;
	delete multi;
	return reader;
}

MultipartFileReader::MultipartFileReader(AsyncFileReader* firstPart)
{
	memset(m_parts,0,sizeof(m_parts));

	m_filename = firstPart->GetFilename();

	m_numparts = 1;

	m_parts[0].reader = firstPart;
	m_parts[0].end = firstPart->GetBlockCount();
}

MultipartFileReader::~MultipartFileReader(void)
{
	Close();
}

void MultipartFileReader::FindParts()
{
	std::string curext(Path::GetExtension(m_filename));
	if (curext.empty())
		return;

	char prefixch = std::tolower(curext[0]);

	// Multi-part rules!
	//  * The first part can either be the proper extension (ISO, MDF, etc) or the numerical
	//    extension (I00, I01, M00, M01, etc).
	//  * Numerical extensions MUST begin at 00 (I00 etc), regardless of if the first part
	//    is proper or numerical.

	uint i = 0;

	if ((curext.length() == 3) && (curext[1] == '0') && (curext[2] == '0'))
	{
		// First file is an OO, so skip 0 in the loop below:
		i = 1;
	}

	std::string extbuf(StringUtil::StdStringFromFormat("%c%02u", prefixch, i));
	std::string nameparts(Path::ReplaceExtension(m_filename, extbuf));
	if (!pxFileExists_WithExt(nameparts, extbuf))
		return;

	DevCon.WriteLn( Color_Blue, "isoFile: multi-part %s detected...", StringUtil::toUpper(curext).c_str() );
	ConsoleIndentScope indent;

	int bsize = m_parts[0].reader->GetBlockSize();
	int blocks = m_parts[0].end;

	m_numparts = 1;

	for (; i < MaxParts; ++i)
	{
		extbuf = StringUtil::StdStringFromFormat("%c%02u", prefixch, i );
		nameparts = Path::ReplaceExtension(m_filename, extbuf);
		if (!pxFileExists_WithExt(nameparts, extbuf))
			break;

		Part* thispart = m_parts + m_numparts;
		AsyncFileReader* thisreader = new FlatFileReader();

		if (!thisreader->Open(nameparts))
		{
			delete thisreader;
			break;
		}

		thisreader->SetBlockSize(bsize);

		thispart->reader = thisreader;
		thispart->start = blocks;

		uint bcount =  thisreader->GetBlockCount();
		blocks += bcount;

		thispart->end = blocks;

		DevCon.WriteLn( Color_Blue, "\tblocks %u - %u in: %s",
			thispart->start, thispart->end,
			nameparts.c_str()
		);

		++m_numparts;
	}

	//Console.WriteLn( Color_Blue, "isoFile: multi-part ISO loaded (%u parts found)", m_numparts );
}

bool MultipartFileReader::Open(std::string fileName)
{
	// Cannot open a MultipartFileReader directly,
	// use DetectMultipart to convert a FlatFileReader
	return false;
}

uint MultipartFileReader::GetFirstPart(uint lsn)
{
	pxAssertMsg(lsn < GetBlockCount(),	"Invalid lsn passed into MultipartFileReader::GetFirstPart.");
	pxAssertMsg(m_numparts, "Invalid object state; multi-part iso file needs at least one part!");

	for (uint i = 0; i < m_numparts; ++i)
	{
		if (lsn < m_parts[i].end)
			return i;
	}

	// should never get here
	return 0xBAAAAAAD;
}

int MultipartFileReader::ReadSync(void* pBuffer, uint sector, uint count)
{
	BeginRead(pBuffer,sector,count);
	return FinishRead();
}

void MultipartFileReader::BeginRead(void* pBuffer, uint sector, uint count)
{
	u8* lBuffer = (u8*)pBuffer;

	for(uint i = GetFirstPart(sector); i < m_numparts; i++)
	{
		uint num = std::min(count, m_parts[i].end - sector);

		m_parts[i].reader->BeginRead(lBuffer, sector - m_parts[i].start, num);
		m_parts[i].isReading = true;

		lBuffer += num * m_blocksize;
		sector += num;
		count -= num;

		if(count <= 0)
			break;
	}
}

int MultipartFileReader::FinishRead(void)
{
	int ret = 0;
	for(uint i=0;i<m_numparts;i++)
	{
		if(m_parts[i].isReading)
		{
			ret = std::min(ret, m_parts[i].reader->FinishRead());
			m_parts[i].isReading = false;

			if(ret < 0)
				ret = -1;
		}
	}

	return ret;
}

void MultipartFileReader::CancelRead(void)
{
	for(uint i=0;i<m_numparts;i++)
	{
		if(m_parts[i].isReading)
		{
			m_parts[i].reader->CancelRead();
			m_parts[i].isReading = false;
		}
	}
}

void MultipartFileReader::Close(void)
{
	for(uint i=0;i<m_numparts;i++)
	{
		if(m_parts[i].reader)
		{
			m_parts[i].reader->Close();
			delete m_parts[i].reader;
		}
	}
}

uint MultipartFileReader::GetBlockCount(void) const
{
	return m_parts[m_numparts-1].end;
}

void MultipartFileReader::SetBlockSize(uint bytes)
{
	uint last_end = 0;
	for(uint i=0;i<m_numparts;i++)
	{
		m_parts[i].reader->SetBlockSize(bytes);
		uint count = m_parts[i].reader->GetBlockCount();

		m_parts[i].start = last_end;
		m_parts[i].end = last_end = m_parts[i].start + count;
	}
}

