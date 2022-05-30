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
#include "common/FileSystem.h"
#include <unistd.h>
#include <fcntl.h>

// The aio module has been reported to cause issues with FreeBSD 10.3, so let's
// disable it for 10.3 and earlier and hope FreeBSD 11 and onwards is fine.
// Note: It may be worth checking whether aio provides any performance benefit.
#if defined(__FreeBSD__) && __FreeBSD__ < 11
#define DISABLE_AIO
#warning AIO has been disabled.
#endif

FlatFileReader::FlatFileReader(bool shareWrite) : shareWrite(shareWrite)
{
	m_blocksize = 2048;
	m_fd = -1;
	m_async_read_in_progress = false;
}

FlatFileReader::~FlatFileReader(void)
{
	Close();
}

bool FlatFileReader::Open(std::string fileName)
{
    m_filename = std::move(fileName);

    m_fd = FileSystem::OpenFDFile(m_filename.c_str(), O_RDONLY, 0);

	return (m_fd != -1);
}

int FlatFileReader::ReadSync(void* pBuffer, uint sector, uint count)
{
	BeginRead(pBuffer, sector, count);
	return FinishRead();
}

void FlatFileReader::BeginRead(void* pBuffer, uint sector, uint count)
{
	u64 offset = sector * (u64)m_blocksize + m_dataoffset;

	u32 bytesToRead = count * m_blocksize;

	m_async_read_in_progress = false;
#ifndef DISABLE_AIO
	m_aiocb = {0};
	m_aiocb.aio_fildes = m_fd;
	m_aiocb.aio_offset = offset;
	m_aiocb.aio_nbytes = bytesToRead;
	m_aiocb.aio_buf = pBuffer;

	if (aio_read(&m_aiocb) == 0)
	{
		m_async_read_in_progress = true;
	}
	else
	{
		switch (errno)
		{
#if defined(__FreeBSD__)
			case ENOSYS:
				Console.Error("AIO read failed: Check the aio kernel module is loaded");
				break;
#endif
			case EAGAIN:
				Console.Warning("AIO read failed: Out of resources.  Will read synchronously");
				break;
			default:
				Console.Error("AIO read failed: error code %d\n", errno);
				break;
		}
	}
#endif
	if (!m_async_read_in_progress)
	{
		m_aiocb.aio_nbytes = pread(m_fd, pBuffer, bytesToRead, offset);
		if (m_aiocb.aio_nbytes != bytesToRead)
			m_aiocb.aio_nbytes = -1;
	}
}

int FlatFileReader::FinishRead(void)
{
	if (!m_async_read_in_progress)
		return m_aiocb.aio_nbytes == (size_t)-1 ? -1 : 1;
	m_async_read_in_progress = true;

	struct aiocb *aiocb_list[] = {&m_aiocb};

	while (aio_suspend(aiocb_list, 1, nullptr) == -1 && errno == EINTR)
		;
	return aio_return(&m_aiocb);
}

void FlatFileReader::CancelRead(void)
{
	if (m_async_read_in_progress)
	{
		aio_cancel(m_fd, &m_aiocb);
		m_async_read_in_progress = false;
	}
}

void FlatFileReader::Close(void)
{
	CancelRead();
	if (m_fd != -1)
		close(m_fd);

	m_fd = -1;
}

uint FlatFileReader::GetBlockCount(void) const
{
	return (int)(FileSystem::GetPathFileSize(m_filename.c_str()) / m_blocksize);
}
