// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "AsyncFileReader.h"

#include "common/FileSystem.h"

#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>

FlatFileReader::FlatFileReader(bool shareWrite)
	: shareWrite(shareWrite)
{
	m_blocksize = 2048;
	m_fd = -1;
	m_aio_context = 0;
}

FlatFileReader::~FlatFileReader()
{
	Close();
}

bool FlatFileReader::Open(std::string filename, Error* error)
{
	m_filename = std::move(filename);

	int err = io_setup(64, &m_aio_context);
	if (err)
		return false;

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
	u64 offset;
	offset = sector * (s64)m_blocksize + m_dataoffset;

	u32 bytesToRead = count * m_blocksize;

	struct iocb iocb;
	struct iocb* iocbs = &iocb;

	io_prep_pread(&iocb, m_fd, pBuffer, bytesToRead, offset);
	io_submit(m_aio_context, 1, &iocbs);
}

int FlatFileReader::FinishRead()
{
	struct io_event event;

	int nevents = io_getevents(m_aio_context, 1, 1, &event, NULL);
	if (nevents < 1)
		return -1;

	return event.res;
}

void FlatFileReader::CancelRead()
{
	// Will be done when m_aio_context context is destroyed
	// Note: io_cancel exists but need the iocb structure as parameter
	// int io_cancel(aio_context_t ctx_id, struct iocb *iocb,
	//                struct io_event *result);
}

void FlatFileReader::Close()
{
	if (m_fd != -1)
		close(m_fd);

	io_destroy(m_aio_context);

	m_fd = -1;
	m_aio_context = 0;
}

u32 FlatFileReader::GetBlockCount() const
{
	struct stat sysStatData;
	if (fstat(m_fd, &sysStatData) < 0)
		return 0;

	return static_cast<u32>(sysStatData.st_size / m_blocksize);
}
