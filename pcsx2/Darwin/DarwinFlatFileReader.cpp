// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "AsyncFileReader.h"

#include "common/Console.h"
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

FlatFileReader::FlatFileReader(bool shareWrite)
	: shareWrite(shareWrite)
{
	m_blocksize = 2048;
	m_fd = -1;
	m_async_read_in_progress = false;
}

FlatFileReader::~FlatFileReader()
{
	Close();
}

bool FlatFileReader::Open(std::string filename, Error* error)
{
	m_filename = std::move(filename);

	m_fd = FileSystem::OpenFDFile(m_filename.c_str(), O_RDONLY, 0, error);

	return (m_fd != -1);
}

int FlatFileReader::ReadSync(void* pBuffer, u32 sector, u32 count)
{
	BeginRead(pBuffer, sector, count);
	return FinishRead();
}

void FlatFileReader::BeginRead(void* pBuffer, u32 sector, u32 count)
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

int FlatFileReader::FinishRead()
{
	if (!m_async_read_in_progress)
		return m_aiocb.aio_nbytes == (size_t)-1 ? -1 : 1;
	m_async_read_in_progress = true;

	struct aiocb* aiocb_list[] = {&m_aiocb};

	while (aio_suspend(aiocb_list, 1, nullptr) == -1 && errno == EINTR)
		;
	return aio_return(&m_aiocb);
}

void FlatFileReader::CancelRead()
{
	if (m_async_read_in_progress)
	{
		aio_cancel(m_fd, &m_aiocb);
		m_async_read_in_progress = false;
	}
}

void FlatFileReader::Close()
{
	CancelRead();
	if (m_fd != -1)
		close(m_fd);

	m_fd = -1;
}

u32 FlatFileReader::GetBlockCount() const
{
	return static_cast<u32>(FileSystem::GetPathFileSize(m_filename.c_str()) / m_blocksize);
}
