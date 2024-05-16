// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "common/Pcsx2Defs.h"

#include <memory>
#include <string>

class Error;

class AsyncFileReader
{
protected:
	AsyncFileReader() = default;

	std::string m_filename;

	u32 m_dataoffset = 0;
	u32 m_blocksize = 2048;

public:
	virtual ~AsyncFileReader(){};

	virtual bool Open(std::string filename, Error* error) = 0;

	virtual int ReadSync(void* pBuffer, u32 sector, u32 count) = 0;

	virtual void BeginRead(void* pBuffer, u32 sector, u32 count) = 0;
	virtual int FinishRead() = 0;
	virtual void CancelRead() = 0;

	virtual void Close() = 0;

	virtual u32 GetBlockCount() const = 0;

	virtual void SetBlockSize(u32 bytes) {}
	virtual void SetDataOffset(u32 bytes) {}

	const std::string& GetFilename() const { return m_filename; }
	u32 GetBlockSize() const { return m_blocksize; }
};
