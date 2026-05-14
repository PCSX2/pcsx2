// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "ZipFile.h"

#include "common/Assertions.h"
#include "common/Console.h"
#include "common/FileSystem.h"
#include "common/Path.h"

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

bool ZipArchive::ExtractFile(ZipEntryIndex index, const char* output_path, Error* error)
{
	pxAssert(m_zip);

	auto output_file = FileSystem::OpenManagedCFile(output_path, "wb");
	if (!output_file.get())
	{
		Error::SetString(error, "Failed to open output file");
		return false;
	}

	ZipEntry entry;
	if (!entry.Open(*this, index, 0, error))
		return false;

	std::optional<u64> size = entry.Size(error);
	if (!size.has_value())
		return false;

	std::vector<u8> buffer(_64kb);
	for (u64 i = 0; i < *size; i += buffer.size())
	{
		u64 block_size = std::min(*size - i, buffer.size());

		if (!entry.Read(buffer.data(), block_size, error))
			return false;

		if (std::fwrite(buffer.data(), block_size, 1, output_file.get()) != 1)
		{
			Error::SetString(error, "Failed to extract file");
			return false;
		}
	}

	return true;
}

bool ZipArchive::ExtractDirectory(std::string zip_directory, std::string output_directory, Error* error)
{
	pxAssert(m_zip);

	if (!FileSystem::DirectoryExists(output_directory.c_str()))
	{
		Error::SetString(error, "Output directory does not exist");
		return false;
	}

	// We're going to be comparing this with canonicalized paths later.
	output_directory = Path::Canonicalize(output_directory.c_str());

	if (!zip_directory.empty() && !zip_directory.ends_with('/'))
		zip_directory += '/';

	s64 entry_count = zip_get_num_entries(m_zip, 0);
	if (entry_count < 0)
	{
		Error::SetStringFmt(error, "zip_get_num_entries returned %d", static_cast<int>(entry_count));
		return false;
	}

	// Iterate over every file in the zip, and extract it if it's inside the
	// source directory.
	for (ZipEntryIndex index = 0; index < static_cast<ZipEntryIndex>(entry_count); index++)
	{
		std::optional<zip_stat_t> stat = StatFile(index, ZIP_STAT_NAME, error);
		if (!stat.has_value())
			return false;

		if (!(stat->valid & ZIP_STAT_NAME))
		{
			Error::SetString(error, "Cannot stat name");
			return false;
		}

		if (std::strncmp(stat->name, zip_directory.c_str(), zip_directory.size()) != 0)
			continue;

		// Convert to native separators.
		std::string relative_zip_path = stat->name + zip_directory.size();
		for (char& c : relative_zip_path)
			if (c == '/')
				c = FS_OSPATH_SEPARATOR_CHARACTER;

		std::string output_path = Path::Canonicalize(output_directory + relative_zip_path);

		// Prevent directory traversal.
		if (!output_path.starts_with(output_directory))
			continue;

		std::string parent_path(Path::GetDirectory(output_path));
		if (!FileSystem::CreateDirectoryPath(parent_path.c_str(), true, error))
			return false;

		if (!ExtractFile(index, output_path.c_str(), error))
			return false;
	}

	return true;
}

bool ZipArchive::AddFile(
	const char* input_path, const char* zip_path, const ZipCompressionOptions& compression, Error* error)
{
	pxAssert(m_zip);

	ZipSource source;
	if (!source.CreateFileSource(input_path, 0, ZIP_LENGTH_TO_END, error))
		return false;

	std::optional<ZipEntryIndex> index = AddFileFromSource(zip_path, source, ZIP_FL_ENC_UTF_8, error);
	if (!index.has_value())
		return false;

	return SetFileCompression(*index, compression, error);
}

bool ZipArchive::AddDirectory(
	const char* input_path,
	const std::string& zip_directory,
	const ZipCompressionOptions& compression,
	bool follow_symbolic_links,
	Error* error)
{
	pxAssert(m_zip);

	if (!FileSystem::DirectoryExists(input_path))
	{
		Error::SetString(error, "Output directory does not exist");
		return false;
	}

	std::vector<FILESYSTEM_FIND_DATA> files;
	if (!FileSystem::FindFiles(
			input_path, "*",
			FILESYSTEM_FIND_HIDDEN_FILES | FILESYSTEM_FIND_FILES | FILESYSTEM_FIND_FOLDERS, &files))
	{
		Error::SetString(error, "Failed to enumerate files in directory");
		return false;
	}

	for (const FILESYSTEM_FIND_DATA& file : files)
	{
		if (!follow_symbolic_links && FileSystem::IsSymbolicLink(file.FileName.c_str()))
			continue;

		std::string zip_path;
		if (!zip_directory.empty() && !zip_directory.ends_with("/"))
			zip_path = fmt::format("{}/{}", zip_directory, Path::GetFileName(file.FileName));
		else
			zip_path = fmt::format("{}{}", zip_directory, Path::GetFileName(file.FileName));

		if (file.Attributes & FILESYSTEM_FILE_ATTRIBUTE_DIRECTORY)
		{
			if (!AddDirectory(file.FileName.c_str(), zip_path, compression, follow_symbolic_links, error))
				return false;
		}
		else
		{
			if (!AddFile(file.FileName.c_str(), zip_path.c_str(), compression, error))
				return false;
		}
	}

	return true;
}

std::optional<ZipEntryIndex> ZipArchive::AddFileFromBuffer(
	const char* name, void* data, u64 length, zip_flags_t flags, int freep, Error* error)
{
	pxAssert(m_zip);

	ZipSource source;

	if (!source.CreateBufferSource(data, length, freep, error))
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

bool ZipArchive::SetFileCompression(ZipEntryIndex index, const ZipCompressionOptions& compression, Error* error)
{
	pxAssert(m_zip);

	if (zip_set_file_compression(m_zip, index, compression.method, compression.level) != 0)
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
	{
		Error close_error;
		if (!Close(&close_error))
			Console.ErrorFmt("Failed to close zip entry: {}", close_error.GetDescription());
	}
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
	{
		Error close_error;
		if (!Close(&close_error))
			Console.ErrorFmt("Failed to close zip entry: {}", close_error.GetDescription());
	}

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

bool ZipEntry::Close(Error* error)
{
	pxAssert(m_file);

	zip_file_t* file = m_file;

	m_archive = nullptr;
	m_index = 0;
	m_file = nullptr;

	int error_code = zip_fclose(file);
	if (error_code != 0)
	{
		zip_error_t close_error;
		zip_error_init_with_code(&close_error, error_code);
		Error::SetString(error, zip_error_strerror(&close_error));
		zip_error_fini(&close_error);

		return false;
	}

	return true;
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

	if (!(stat->valid & ZIP_STAT_SIZE))
	{
		Error::SetString(error, "Cannot stat size");
		return std::nullopt;
	}

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

bool ZipSource::CreateBufferSource(void* data, u64 length, int freep, Error* error)
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

bool ZipSource::CreateFileSource(const char* path, u64 start_offset, s64 length, Error* error)
{
	pxAssert(!m_source);

	zip_error_t create_error;
	zip_error_init(&create_error);

	m_source = zip_source_file_create(path, start_offset, length, &create_error);
	if (!m_source)
	{
		Error::SetString(error, zip_error_strerror(&create_error));
		zip_error_fini(&create_error);
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
