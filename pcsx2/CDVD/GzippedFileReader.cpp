// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "Config.h"
#include "GzippedFileReader.h"
#include "Host.h"
#include "CDVD/zlib_indexed.h"

#include "common/Console.h"
#include "common/FileSystem.h"
#include "common/Error.h"
#include "common/Path.h"
#include "common/StringUtil.h"
#include "common/Timer.h"

#include "fmt/format.h"

#define GZIP_ID "PCSX2.index.gzip.v1|"
#define GZIP_ID_LEN (sizeof(GZIP_ID) - 1) /* sizeof includes the \0 terminator */

// File format is:
// - [GZIP_ID_LEN] GZIP_ID (no \0)
// - [sizeof(Access)] index (should be allocated, contains various sizes)
// - [rest] the indexed data points (should be allocated, index->list should then point to it)
static Access* ReadIndexFromFile(const char* filename)
{
	auto fp = FileSystem::OpenManagedCFile(filename, "rb");
	if (!fp)
		return nullptr;

	s64 size;
	if ((size = FileSystem::FSize64(fp.get())) <= 0)
	{
		ERROR_LOG("Invalid gzip index size: {}", size);
		return nullptr;
	}

	char fileId[GZIP_ID_LEN + 1] = {0};
	if (std::fread(fileId, GZIP_ID_LEN, 1, fp.get()) != 1 || std::memcmp(fileId, GZIP_ID, 4) != 0)
	{
		ERROR_LOG("Incompatible gzip index: '{}'", filename);
		return nullptr;
	}

	Access* const index = static_cast<Access*>(std::malloc(sizeof(Access)));
	const s64 datasize = size - GZIP_ID_LEN - sizeof(Access);
	if (std::fread(index, sizeof(Access), 1, fp.get()) != 1 ||
		datasize != static_cast<s64>(index->have) * static_cast<s64>(sizeof(Point)))
	{
		ERROR_LOG("Unexpected size of gzip index: '{}'.", filename);
		std::free(index);
		return 0;
	}

	char* buffer = static_cast<char*>(std::malloc(datasize));
	if (std::fread(buffer, datasize, 1, fp.get()) != 1)
	{
		ERROR_LOG("Failed read of gzip index: '{}'.");
		std::free(buffer);
		std::free(index);
		return 0;
	}

	index->list = reinterpret_cast<Point*>(buffer); // adjust list pointer
	return index;
}

static void WriteIndexToFile(Access* index, const char* filename)
{
	auto fp = FileSystem::OpenManagedCFile(filename, "wb");
	if (!fp)
		return;

	bool success = (std::fwrite(GZIP_ID, GZIP_ID_LEN, 1, fp.get()) == 1);

	Point* tmp = index->list;
	index->list = 0; // current pointer is useless on disk, normalize it as 0.
	std::fwrite((char*)index, sizeof(Access), 1, fp.get());
	index->list = tmp;

	success = success && (std::fwrite((char*)index->list, sizeof(Point) * index->have, 1, fp.get()) == 1);

	// Verify
	if (!success)
		ERROR_LOG("Warning: Can't write index file to disk: '{}'", filename);
	else
		INFO_LOG("Gzip quick access index file saved to disk: '{}'", filename);
}

static const char* INDEX_TEMPLATE_KEY = "$(f)";

// template:
// must contain one and only one instance of '$(f)' (without the quotes)
// if if !canEndWithKey -> must not end with $(f)
// if starts with $(f) then it expands to the full path + file name.
// if doesn't start with $(f) then it's expanded to file name only (with extension)
// if doesn't start with $(f) and ends up relative,
//   then it's relative to base (not to cwd)
// No checks are performed if the result file name can be created.
// If this proves useful, we can move it into Path:: . Right now there's no need.
static std::string ApplyTemplate(const std::string& name, const std::string& base,
	const std::string& fileTemplate, const std::string& filename,
	bool canEndWithKey, Error* error)
{
	// both sides
	std::string trimmedTemplate(StringUtil::StripWhitespace(fileTemplate));

	std::string::size_type first = trimmedTemplate.find(INDEX_TEMPLATE_KEY);
	if (first == std::string::npos // not found
		|| first != trimmedTemplate.rfind(INDEX_TEMPLATE_KEY) // more than one instance
		|| (!canEndWithKey && first == trimmedTemplate.length() - std::strlen(INDEX_TEMPLATE_KEY)))
	{
		Error::SetStringFmt(error, "Invalid {} template '{}'.\n"
								   "Template must contain exactly one '%s' and must not end with it. Aborting.",
			name, trimmedTemplate, INDEX_TEMPLATE_KEY);
		return {};
	}

	std::string fname(filename);
	if (first > 0)
		fname = Path::GetFileName(fname); // without path

	StringUtil::ReplaceAll(&trimmedTemplate, INDEX_TEMPLATE_KEY, fname);
	if (!Path::IsAbsolute(trimmedTemplate))
		trimmedTemplate = Path::Combine(base, trimmedTemplate); // ignores appRoot if tem is absolute

	return trimmedTemplate;
}

static std::string iso2indexname(const std::string& isoname, Error* error)
{
	const std::string& appRoot = EmuFolders::DataRoot;
	return ApplyTemplate("gzip index", appRoot, Host::GetBaseStringSettingValue("EmuCore", "GzipIsoIndexTemplate", "$(f).pindex.tmp"), isoname, false, error);
}


GzippedFileReader::GzippedFileReader() = default;

GzippedFileReader::~GzippedFileReader() = default;

bool GzippedFileReader::LoadOrCreateIndex(Error* error)
{
	// Try to read index from disk
	const std::string indexfile(iso2indexname(m_filename, error));
	if (indexfile.empty())
	{
		// iso2indexname(...) will set errors if it can't apply the template
		return false;
	}

	if ((m_index = ReadIndexFromFile(indexfile.c_str())) != nullptr)
	{
		INFO_LOG("Gzip quick access index read from disk: '{}'", indexfile);
		return true;
	}

	// No valid index file. Generate an index
	Console.Warning("This may take a while (but only once). Scanning compressed file to generate a quick access index...");

	const s64 prevoffset = FileSystem::FTell64(m_src);
	Access* index = nullptr;
	int len = build_index(m_src, GZFILE_SPAN_DEFAULT, &index);
	printf("\n"); // build_index prints progress without \n's
	FileSystem::FSeek64(m_src, prevoffset, SEEK_SET);

	if (len >= 0)
	{
		m_index = index;
		WriteIndexToFile(m_index, indexfile.c_str());
	}
	else
	{
		Error::SetStringFmt(error, "ERROR ({}): Index could not be generated for file '{}'", len, m_filename);
		free_index(index);
		return false;
	}

	return true;
}

bool GzippedFileReader::Open2(std::string filename, Error* error)
{
	Close();

	m_filename = std::move(filename);
	if (!(m_src = FileSystem::OpenCFile(m_filename.c_str(), "rb", error)) || !LoadOrCreateIndex(error))
	{
		Close();
		return false;
	}

	return true;
}

void GzippedFileReader::Close2()
{
	if (m_z_state.isValid)
	{
		inflateEnd(&m_z_state.strm);
		m_z_state = {};
	}

	if (m_src)
	{
		std::fclose(m_src);
		m_src = nullptr;
	}

	if (m_index)
	{
		free_index(m_index);
		m_index = nullptr;
	}
}

ThreadedFileReader::Chunk GzippedFileReader::ChunkForOffset(u64 offset)
{
	ThreadedFileReader::Chunk chunk = {};
	if (static_cast<s64>(offset) >= m_index->uncompressed_size)
	{
		chunk.chunkID = -1;
	}
	else
	{
		chunk.chunkID = static_cast<s64>(offset) / m_index->span;
		chunk.length = static_cast<u32>(std::min<u64>(m_index->uncompressed_size - offset, m_index->span));
		chunk.offset = static_cast<u64>(chunk.chunkID) * m_index->span;
	}

	return chunk;
}

int GzippedFileReader::ReadChunk(void* dst, s64 chunkID)
{
	if (chunkID < 0)
		return -1;

	const s64 file_offset = chunkID * m_index->span;
	const u32 read_len = static_cast<u32>(std::min<s64>(m_index->uncompressed_size - file_offset, m_index->span));
	return extract(m_src, m_index, file_offset, static_cast<unsigned char*>(dst), read_len, &m_z_state);
}

u32 GzippedFileReader::GetBlockCount() const
{
	return (m_index->uncompressed_size + (m_blocksize - 1)) / m_blocksize;
}
