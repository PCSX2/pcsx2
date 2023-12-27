// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "AsyncFileReader.h"

#include "common/StringUtil.h"
#include "common/Error.h"

FlatFileReader::FlatFileReader(bool shareWrite) : shareWrite(shareWrite)
{
	m_blocksize = 2048;
	hOverlappedFile = INVALID_HANDLE_VALUE;
	hEvent = INVALID_HANDLE_VALUE;
	asyncInProgress = false;
}

FlatFileReader::~FlatFileReader()
{
	Close();
}

bool FlatFileReader::Open(std::string filename, Error* error)
{
	m_filename = std::move(filename);

	hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	DWORD shareMode = FILE_SHARE_READ;
	if (shareWrite)
		shareMode |= FILE_SHARE_WRITE;

	hOverlappedFile = CreateFile(
		StringUtil::UTF8StringToWideString(m_filename).c_str(),
		GENERIC_READ,
		shareMode,
		NULL,
		OPEN_EXISTING,
		FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_OVERLAPPED,
		NULL);

	if (hOverlappedFile == INVALID_HANDLE_VALUE)
	{
		Error::SetWin32(error, GetLastError());
		return false;
	}

	return true;
}

int FlatFileReader::ReadSync(void* pBuffer, u32 sector, u32 count)
{
	//LARGE_INTEGER offset;
	//offset.QuadPart = sector * (__int64)m_blocksize;
	//
	//DWORD bytesToRead = count * m_blocksize;
	//DWORD bytes;

	//if(!ReadFile(hOverlappedFile, pBuffer, bytesToRead, &bytes, NULL))
	//	return -1;

	//return bytes;
	BeginRead(pBuffer, sector, count);
	return FinishRead();
}

void FlatFileReader::BeginRead(void* pBuffer, u32 sector, u32 count)
{
	LARGE_INTEGER offset;
	offset.QuadPart = sector * (s64)m_blocksize + m_dataoffset;

	DWORD bytesToRead = count * m_blocksize;

	ZeroMemory(&asyncOperationContext, sizeof(asyncOperationContext));
	asyncOperationContext.hEvent = hEvent;
	asyncOperationContext.Offset = offset.LowPart;
	asyncOperationContext.OffsetHigh = offset.HighPart;

	ReadFile(hOverlappedFile, pBuffer, bytesToRead, NULL, &asyncOperationContext);
	asyncInProgress = true;
}

int FlatFileReader::FinishRead()
{
	DWORD bytes;

	if(!GetOverlappedResult(hOverlappedFile, &asyncOperationContext, &bytes, TRUE))
	{
		asyncInProgress = false;
		return -1;
	}

	asyncInProgress = false;
	return bytes;
}

void FlatFileReader::CancelRead()
{
	CancelIo(hOverlappedFile);
}

void FlatFileReader::Close()
{
	if(asyncInProgress)
		CancelRead();

	if(hOverlappedFile != INVALID_HANDLE_VALUE)
		CloseHandle(hOverlappedFile);

	if(hEvent != INVALID_HANDLE_VALUE)
		CloseHandle(hEvent);

	hOverlappedFile = INVALID_HANDLE_VALUE;
	hEvent = INVALID_HANDLE_VALUE;
}

u32 FlatFileReader::GetBlockCount(void) const
{
	LARGE_INTEGER fileSize;
	fileSize.LowPart = GetFileSize(hOverlappedFile, reinterpret_cast<DWORD*>(&fileSize.HighPart));

	return static_cast<u32>(fileSize.QuadPart / m_blocksize);
}
