// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "ZipFile.h"

#include "common/Assertions.h"

ZipArchive::ZipArchive()
{
}

ZipArchive::~ZipArchive()
{
	if (m_zip)
		DiscardChangesAndClose();
}

ZipArchive::ZipArchive(ZipArchive&& rhs)
	: m_zip(rhs.m_zip)
{
	rhs.m_zip = nullptr;
}

ZipArchive& ZipArchive::operator=(ZipArchive&& rhs)
{
	if (m_zip)
		DiscardChangesAndClose();

	m_zip = rhs.m_zip;
	rhs.m_zip = nullptr;

	return *this;
}

bool ZipArchive::Open(const char* path, int flags, int* error_code, Error* error)
{
	pxAssert(!m_zip);

	int err;
	m_zip = zip_open(path, flags, &err);
	if (!m_zip)
	{
		if (error_code)
			*error_code = err;

		zip_error_t open_error;
		zip_error_init_with_code(&open_error, err);
		Error::SetString(error, zip_error_strerror(&open_error));
		zip_error_fini(&open_error);

		return false;
	}

	return true;
}

bool ZipArchive::SaveChangesAndClose(Error* error)
{
	if (zip_close(m_zip) != 0)
	{
		Error::SetString(error, zip_strerror(m_zip));
		return false;
	}

	m_zip = nullptr;
	return true;
}

void ZipArchive::DiscardChangesAndClose()
{
	pxAssert(m_zip);

	zip_discard(m_zip);
	m_zip = nullptr;
}

bool ZipArchive::IsValid()
{
	return m_zip != nullptr;
}

std::optional<ZipEntryIndex> ZipArchive::LocateFile(const char* name, zip_flags_t flags, Error* error)
{
	pxAssert(m_zip);

	s64 index = zip_name_locate(m_zip, name, flags);
	if (index < 0)
	{
		Error::SetString(error, zip_strerror(m_zip));
		return std::nullopt;
	}

	return static_cast<ZipEntryIndex>(index);
}

std::optional<zip_stat_t> ZipArchive::StatFile(ZipEntryIndex index, zip_flags_t flags, Error* error)
{
	pxAssert(m_zip);

	zip_stat_t stat = {};
	if (zip_stat_index(m_zip, index, flags, &stat) != 0)
	{
		Error::SetString(error, zip_strerror(m_zip));
		return std::nullopt;
	}

	return stat;
}

std::optional<std::string> ZipArchive::ReadTextFile(ZipEntryIndex index, zip_flags_t flags, Error* error)
{
	pxAssert(m_zip);

	ZipEntry file;
	if (!file.Open(*this, index, flags, error))
		return std::nullopt;

	return file.ReadText(error);
}

std::optional<std::vector<u8>> ZipArchive::ReadBinaryFile(ZipEntryIndex index, zip_flags_t flags, Error* error)
{
	pxAssert(m_zip);

	ZipEntry file;
	if (!file.Open(*this, index, flags, error))
		return std::nullopt;

	return file.ReadBinary(error);
}

std::optional<ZipEntryIndex> ZipArchive::AddFileFromBuffer(
	const char* name, void* data, u64 length, zip_flags_t flags, int freep, Error* error)
{
	pxAssert(m_zip);

	ZipSource source;

	if (!source.CreateBuffer(data, length, freep, error))
		return false;

	return AddFileFromSource(name, source, flags, error);
}

std::optional<ZipEntryIndex> ZipArchive::AddFileFromSource(
	const char* name, ZipSource& source, zip_flags_t flags, Error* error)
{
	pxAssert(m_zip);

	s64 index = zip_file_add(m_zip, name, source.m_source, flags);
	if (index < 0)
	{
		Error::SetString(error, zip_strerror(m_zip));
		return std::nullopt;
	}

	// Ownership of the source has been transferred.
	source.m_source = nullptr;

	return static_cast<ZipEntryIndex>(index);
}

bool ZipArchive::SetFileCompression(ZipEntryIndex index, s32 comp, u32 comp_flags, Error* error)
{
	pxAssert(m_zip);

	if (zip_set_file_compression(m_zip, index, comp, comp_flags) != 0)
	{
		Error::SetString(error, zip_strerror(m_zip));
		return false;
	}

	return true;
}

// *****************************************************************************

ZipEntry::ZipEntry()
{
}

ZipEntry::~ZipEntry()
{
	if (m_file)
		Close();
}

ZipEntry::ZipEntry(ZipEntry&& rhs)
	: m_archive(rhs.m_archive)
	, m_index(rhs.m_index)
	, m_file(rhs.m_file)
{
	rhs.m_archive = nullptr;
	rhs.m_index = 0;
	rhs.m_file = nullptr;
}

ZipEntry& ZipEntry::operator=(ZipEntry&& rhs)
{
	if (m_file)
		Close();

	m_archive = rhs.m_archive;
	m_index = rhs.m_index;
	m_file = rhs.m_file;

	rhs.m_archive = nullptr;
	rhs.m_index = 0;
	rhs.m_file = nullptr;

	return *this;
}

bool ZipEntry::Open(ZipArchive& archive, ZipEntryIndex index, zip_flags_t flags, Error* error)
{
	pxAssert(!m_file);
	pxAssert(archive.m_zip);

	m_file = zip_fopen_index(archive.m_zip, index, flags);
	if (!m_file)
	{
		Error::SetString(error, zip_strerror(archive.m_zip));
		return false;
	}

	m_archive = &archive;
	m_index = index;

	return true;
}

void ZipEntry::Close()
{
	pxAssert(m_file);

	zip_fclose(m_file);

	m_archive = nullptr;
	m_index = 0;
	m_file = nullptr;
}

bool ZipEntry::IsValid()
{
	return m_file != nullptr;
}

bool ZipEntry::Read(void* buffer, u64 nbytes, Error* error)
{
	pxAssert(m_file);

	s64 bytes = zip_fread(m_file, buffer, nbytes);
	if (bytes < 0)
	{
		Error::SetString(error, zip_file_strerror(m_file));
		return false;
	}

	if (static_cast<u64>(bytes) != nbytes)
	{
		Error::SetString(error, "Tried to read past end of file");
		return false;
	}

	return true;
}

bool ZipEntry::Seek(s64 offset, int whence, Error* error)
{
	pxAssert(m_file);

	if (zip_fseek(m_file, offset, whence) != 0)
	{
		Error::SetString(error, zip_file_strerror(m_file));
		return false;
	}

	return true;
}

std::optional<u64> ZipEntry::Tell(Error* error)
{
	pxAssert(m_file);

	s64 offset = zip_ftell(m_file);
	if (offset < 0)
	{
		Error::SetString(error, zip_file_strerror(m_file));
		return std::nullopt;
	}

	return static_cast<u64>(offset);
}

std::optional<u64> ZipEntry::Size(Error* error)
{
	pxAssert(m_file);

	std::optional<zip_stat_t> stat = m_archive->StatFile(m_index, ZIP_STAT_SIZE, error);
	if (!stat.has_value())
		return std::nullopt;

	return stat->size;
}

std::optional<std::string> ZipEntry::ReadText(Error* error)
{
	pxAssert(m_file);

	std::optional<u64> size = Size(error);
	if (!size.has_value())
		return std::nullopt;

	std::string string(*size, '\0');
	if (!Read(string.data(), string.size(), error))
		return std::nullopt;

	return string;
}

std::optional<std::vector<u8>> ZipEntry::ReadBinary(Error* error)
{
	pxAssert(m_file);

	std::optional<u64> size = Size(error);
	if (!size.has_value())
		return std::nullopt;

	std::vector<u8> binary(*size, '\0');
	if (!Read(binary.data(), binary.size(), error))
		return std::nullopt;

	return binary;
}

// *****************************************************************************

ZipSource::ZipSource()
{
}

ZipSource::~ZipSource()
{
	if (m_source)
		Free();
}

ZipSource::ZipSource(ZipSource&& rhs)
	: m_source(rhs.m_source)
{
	rhs.m_source = nullptr;
}

ZipSource& ZipSource::operator=(ZipSource&& rhs)
{
	if (m_source)
		Free();

	m_source = rhs.m_source;
	rhs.m_source = nullptr;

	return *this;
}

bool ZipSource::CreateBuffer(void* data, u64 length, int freep, Error* error)
{
	pxAssert(!m_source);

	zip_error_t create_error;
	zip_error_init(&create_error);
	m_source = zip_source_buffer_create(data, length, freep, &create_error);
	if (!m_source)
	{
		Error::SetString(error, zip_error_strerror(&create_error));
		zip_error_fini(&create_error);
		if (freep)
			std::free(data);
		return false;
	}

	zip_error_fini(&create_error);

	return true;
}

void ZipSource::Free()
{
	pxAssert(m_source);

	zip_source_free(m_source);
	m_source = nullptr;
}

bool ZipSource::IsValid()
{
	return m_source != nullptr;
}

bool ZipSource::BeginWrite(Error* error)
{
	pxAssert(m_source);

	if (zip_source_begin_write(m_source) != 0)
	{
		Error::SetString(error, zip_error_strerror(zip_source_error(m_source)));
		return false;
	}

	return true;
}

bool ZipSource::Write(const void* data, u64 length, Error* error)
{
	pxAssert(m_source);

	if (zip_source_write(m_source, data, length) != 0)
	{
		Error::SetString(error, zip_error_strerror(zip_source_error(m_source)));
		return false;
	}

	return true;
}

bool ZipSource::CommitWrite(Error* error)
{
	pxAssert(m_source);

	if (zip_source_commit_write(m_source) != 0)
	{
		Error::SetString(error, zip_error_strerror(zip_source_error(m_source)));
		return false;
	}

	return true;
}
