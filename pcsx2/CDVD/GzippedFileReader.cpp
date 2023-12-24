// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "Config.h"
#include "ChunksCache.h"
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
		Console.Error(fmt::format("Invalid gzip index size: {}", size));
		return nullptr;
	}

	char fileId[GZIP_ID_LEN + 1] = {0};
	if (std::fread(fileId, GZIP_ID_LEN, 1, fp.get()) != 1 || std::memcmp(fileId, GZIP_ID, 4) != 0)
	{
		Console.Error(fmt::format("Incompatible gzip index, please delete it manually: '{}'", filename));
		return nullptr;
	}

	Access* const index = static_cast<Access*>(std::malloc(sizeof(Access)));
	const s64 datasize = size - GZIP_ID_LEN - sizeof(Access);
	if (std::fread(index, sizeof(Access), 1, fp.get()) != 1 ||
		datasize != static_cast<s64>(index->have) * static_cast<s64>(sizeof(Point)))
	{
		Console.Error(fmt::format("Unexpected size of gzip index, please delete it manually: '{}'.", filename));
		std::free(index);
		return 0;
	}

	char* buffer = static_cast<char*>(std::malloc(datasize));
	if (std::fread(buffer, datasize, 1, fp.get()) != 1)
	{
		Console.Error(fmt::format("Failed read of gzip index, please delete it manually: '{}'.", filename));
		std::free(buffer);
		std::free(index);
		return 0;
	}

	index->list = reinterpret_cast<Point*>(buffer); // adjust list pointer
	return index;
}

static void WriteIndexToFile(Access* index, const char* filename)
{
	if (FileSystem::FileExists(filename))
	{
		Console.Warning("WARNING: Won't write index - file name exists (please delete it manually): '%s'", filename);
		return;
	}

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
	{
		Console.Warning("Warning: Can't write index file to disk: '%s'", filename);
	}
	else
	{
		Console.WriteLn(Color_Green, "OK: Gzip quick access index file saved to disk: '%s'", filename);
	}
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
		Error::SetString(error, fmt::format("Invalid {} template '{}'.\n"
					  "Template must contain exactly one '%s' and must not end with it. Aborting.",
			name, trimmedTemplate, INDEX_TEMPLATE_KEY));
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

GzippedFileReader::GzippedFileReader()
	: m_cache(GZFILE_CACHE_SIZE_MB)
{
	m_blocksize = 2048;
	AsyncPrefetchReset();
}

GzippedFileReader::~GzippedFileReader()
{
	Close();
}

void GzippedFileReader::InitZstates()
{
	if (m_zstates)
	{
		delete[] m_zstates;
		m_zstates = 0;
	}
	if (!m_pIndex)
		return;

	// having another extra element helps avoiding logic for last (so 2+ instead of 1+)
	int size = 2 + m_pIndex->uncompressed_size / m_pIndex->span;
	m_zstates = new Czstate[size]();
}

#ifndef _WIN32
void GzippedFileReader::AsyncPrefetchReset(){};
void GzippedFileReader::AsyncPrefetchOpen(){};
void GzippedFileReader::AsyncPrefetchClose(){};
void GzippedFileReader::AsyncPrefetchChunk(s64 dummy){};
void GzippedFileReader::AsyncPrefetchCancel(){};
#else
// AsyncPrefetch works as follows:
// ater extracting a chunk from the compressed file, ask the OS to asynchronously
// read the next chunk from the file, and then completely ignore the result and
// cancel the async read before the next extract. the next extract then reads the
// data from the disk buf if it's overlapping/contained within the chunk we've
// asked the OS to prefetch, then the OS is likely to already have it cached.
// This procedure is frequently able to overcome seek time due to fragmentation of the
// compressed file on disk without any meaningful penalty.
// This system is only enabled for win32 where we have this async read request.
void GzippedFileReader::AsyncPrefetchReset()
{
	hOverlappedFile = INVALID_HANDLE_VALUE;
	asyncInProgress = false;
}

void GzippedFileReader::AsyncPrefetchOpen()
{
	hOverlappedFile = CreateFile(
		StringUtil::UTF8StringToWideString(m_filename).c_str(),
		GENERIC_READ,
		FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_OVERLAPPED,
		NULL);
};

void GzippedFileReader::AsyncPrefetchClose()
{
	AsyncPrefetchCancel();

	if (hOverlappedFile != INVALID_HANDLE_VALUE)
		CloseHandle(hOverlappedFile);

	AsyncPrefetchReset();
};

void GzippedFileReader::AsyncPrefetchChunk(s64 start)
{
	if (hOverlappedFile == INVALID_HANDLE_VALUE || asyncInProgress)
	{
		Console.Warning("Unexpected file handle or progress state. Aborting prefetch.");
		return;
	}

	LARGE_INTEGER offset;
	offset.QuadPart = start;

	DWORD bytesToRead = GZFILE_READ_CHUNK_SIZE;

	ZeroMemory(&asyncOperationContext, sizeof(asyncOperationContext));
	asyncOperationContext.hEvent = 0;
	asyncOperationContext.Offset = offset.LowPart;
	asyncOperationContext.OffsetHigh = offset.HighPart;

	ReadFile(hOverlappedFile, mDummyAsyncPrefetchTarget, bytesToRead, NULL, &asyncOperationContext);
	asyncInProgress = true;
};

void GzippedFileReader::AsyncPrefetchCancel()
{
	if (!asyncInProgress)
		return;

	if (!CancelIo(hOverlappedFile))
	{
		Console.Warning("Canceling gz prefetch failed. Following prefetching will not work.");
		return;
	}

	asyncInProgress = false;
};
#endif /* _WIN32 */

bool GzippedFileReader::OkIndex(Error* error)
{
	if (m_pIndex)
		return true;

	// Try to read index from disk
	const std::string indexfile(iso2indexname(m_filename, error));
	if (indexfile.empty())
	{
		// iso2indexname(...) will set errors if it can't apply the template
		return false; 
	}

	if (FileSystem::FileExists(indexfile.c_str()) && (m_pIndex = ReadIndexFromFile(indexfile.c_str())))
	{
		Console.WriteLn(Color_Green, "OK: Gzip quick access index read from disk: '%s'", indexfile.c_str());
		if (m_pIndex->span != GZFILE_SPAN_DEFAULT)
		{
			Console.Warning("Note: This index has %1.1f MB intervals, while the current default for new indexes is %1.1f MB.",
				(float)m_pIndex->span / 1024 / 1024, (float)GZFILE_SPAN_DEFAULT / 1024 / 1024);
			Console.Warning("It will work fine, but if you want to generate a new index with default intervals, delete this index file.");
			Console.Warning("(smaller intervals mean bigger index file and quicker but more frequent decompressions)");
		}
		InitZstates();
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
		m_pIndex = index;
		WriteIndexToFile((Access*)m_pIndex, indexfile.c_str());
	}
	else
	{
		Error::SetString(error, fmt::format("ERROR ({}): Index could not be generated for file '{}'", len, m_filename));
		free_index(index);
		InitZstates();
		return false;
	}

	InitZstates();
	return true;
}

bool GzippedFileReader::Open(std::string filename, Error* error)
{
	Close();
	m_filename = std::move(filename);
	if (!(m_src = FileSystem::OpenCFile(m_filename.c_str(), "rb", error)) || !OkIndex(error))
	{
		Close();
		return false;
	}

	AsyncPrefetchOpen();
	return true;
};

void GzippedFileReader::BeginRead(void* pBuffer, uint sector, uint count)
{
	// No a-sync support yet, implement as sync
	mBytesRead = ReadSync(pBuffer, sector, count);
}

int GzippedFileReader::FinishRead()
{
	int res = mBytesRead;
	mBytesRead = -1;
	return res;
}

void GzippedFileReader::CancelRead()
{
}

int GzippedFileReader::ReadSync(void* pBuffer, uint sector, uint count)
{
	s64 offset = (s64)sector * m_blocksize + m_dataoffset;
	int bytesToRead = count * m_blocksize;
	int res = _ReadSync(pBuffer, offset, bytesToRead);
	if (res < 0)
		Console.Error("Error: iso-gzip read unsuccessful.");
	return res;
}

// If we have a valid and adequate zstate for this span, use it, else, use the index
s64 GzippedFileReader::GetOptimalExtractionStart(s64 offset)
{
	int span = m_pIndex->span;
	Czstate& cstate = m_zstates[offset / span];
	s64 stateOffset = cstate.state.isValid ? cstate.state.out_offset : 0;
	if (stateOffset && stateOffset <= offset)
		return stateOffset; // state is faster than indexed

	// If span is not exact multiples of GZFILE_READ_CHUNK_SIZE (because it was configured badly),
	// we fallback to always GZFILE_READ_CHUNK_SIZE boundaries
	if (span % GZFILE_READ_CHUNK_SIZE)
		return offset / GZFILE_READ_CHUNK_SIZE * GZFILE_READ_CHUNK_SIZE;

	return span * (offset / span); // index direct access boundaries
}

int GzippedFileReader::_ReadSync(void* pBuffer, s64 offset, uint bytesToRead)
{
	if (!OkIndex(nullptr))
		return -1;

	if ((offset + bytesToRead) > m_pIndex->uncompressed_size)
		return -1;

	// Without all the caching, chunking and states, this would be enough:
	// return extract(m_src, m_pIndex, offset, (unsigned char*)pBuffer, bytesToRead);

	// Split request to GZFILE_READ_CHUNK_SIZE chunks at GZFILE_READ_CHUNK_SIZE boundaries
	uint maxInChunk = GZFILE_READ_CHUNK_SIZE - offset % GZFILE_READ_CHUNK_SIZE;
	if (bytesToRead > maxInChunk)
	{
		int first = _ReadSync(pBuffer, offset, maxInChunk);
		if (first != (int)maxInChunk)
			return first; // EOF or failure

		int rest = _ReadSync((char*)pBuffer + maxInChunk, offset + maxInChunk, bytesToRead - maxInChunk);
		if (rest < 0)
			return rest;

		return first + rest;
	}

	// From here onwards it's guarenteed that the request is inside a single GZFILE_READ_CHUNK_SIZE boundaries

	int res = m_cache.Read(pBuffer, offset, bytesToRead);
	if (res >= 0)
		return res;

	// Not available from cache. Decompress from optimal starting
	// point in GZFILE_READ_CHUNK_SIZE chunks and cache each chunk.
	Common::Timer start_time;
	s64 extractOffset = GetOptimalExtractionStart(offset); // guaranteed in GZFILE_READ_CHUNK_SIZE boundaries
	int size = offset + maxInChunk - extractOffset;
	unsigned char* extracted = (unsigned char*)malloc(size);

	int span = m_pIndex->span;
	int spanix = extractOffset / span;
	AsyncPrefetchCancel();
	res = extract(m_src, m_pIndex, extractOffset, extracted, size, &(m_zstates[spanix].state));
	if (res < 0)
	{
		free(extracted);
		return res;
	}
	AsyncPrefetchChunk(getInOffset(&(m_zstates[spanix].state)));

	int copied = ChunksCache::CopyAvailable(extracted, extractOffset, res, pBuffer, offset, bytesToRead);

	if (m_zstates[spanix].state.isValid && (extractOffset + res) / span != offset / span)
	{
		// The state no longer matches this span.
		// move the state to the appropriate span because it will be faster than using the index
		int targetix = (extractOffset + res) / span;
		m_zstates[targetix].Kill();
		// We have elements for the entire file, and another one.
		m_zstates[targetix].state.in_offset = m_zstates[spanix].state.in_offset;
		m_zstates[targetix].state.isValid = m_zstates[spanix].state.isValid;
		m_zstates[targetix].state.out_offset = m_zstates[spanix].state.out_offset;
		inflateCopy(&m_zstates[targetix].state.strm, &m_zstates[spanix].state.strm);

		m_zstates[spanix].Kill();
	}

	if (size <= GZFILE_READ_CHUNK_SIZE)
		m_cache.Take(extracted, extractOffset, res, size);
	else
	{ // split into cacheable chunks
		for (int i = 0; i < size; i += GZFILE_READ_CHUNK_SIZE)
		{
			int available = std::clamp(res - i, 0, GZFILE_READ_CHUNK_SIZE);
			void* chunk = available ? malloc(available) : 0;
			if (available)
				memcpy(chunk, extracted + i, available);
			m_cache.Take(chunk, extractOffset + i, available, std::min(size - i, GZFILE_READ_CHUNK_SIZE));
		}
		free(extracted);
	}

	if (const double duration = start_time.GetTimeMilliseconds(); duration > 10)
	{
		Console.WriteLn(Color_Gray, "gunzip: chunk #%5d-%2d : %1.2f MB - %d ms",
			(int)(offset / 4 / 1024 / 1024),
			(int)(offset % (4 * 1024 * 1024) / GZFILE_READ_CHUNK_SIZE),
			(float)size / 1024 / 1024,
			duration);
	}

	return copied;
}

void GzippedFileReader::Close()
{
	m_filename.clear();
	if (m_pIndex)
	{
		free_index((Access*)m_pIndex);
		m_pIndex = 0;
	}

	InitZstates(); // results in delete because no index
	m_cache.Clear();

	if (m_src)
	{
		fclose(m_src);
		m_src = nullptr;
	}

	AsyncPrefetchClose();
}

u32 GzippedFileReader::GetBlockCount() const
{
	// type and formula copied from FlatFileReader
	// FIXME? : Shouldn't it be uint and (size - m_dataoffset) / m_blocksize ?
	return (int)((m_pIndex ? m_pIndex->uncompressed_size : 0) / m_blocksize);
}

void GzippedFileReader::SetBlockSize(u32 bytes)
{
	m_blocksize = bytes;
}

void GzippedFileReader::SetDataOffset(u32 bytes)
{
	m_dataoffset = bytes;
}
