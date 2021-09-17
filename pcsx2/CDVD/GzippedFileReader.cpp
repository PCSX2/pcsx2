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
#include <fstream>
#include <wx/stdpaths.h>
#include "Config.h"
#include "ChunksCache.h"
#include "CompressedFileReaderUtils.h"
#include "GzippedFileReader.h"
#include "zlib_indexed.h"

#define CLAMP(val, minval, maxval) (std::min(maxval, std::max(minval, val)))

static s64 fsize(const wxString& filename)
{
	if (!wxFileName::FileExists(filename))
		return -1;

	std::ifstream f(PX_wfilename(filename), std::ifstream::binary);
	f.seekg(0, f.end);
	s64 size = f.tellg();
	f.close();

	return size;
}

#define GZIP_ID "PCSX2.index.gzip.v1|"
#define GZIP_ID_LEN (sizeof(GZIP_ID) - 1) /* sizeof includes the \0 terminator */

// File format is:
// - [GZIP_ID_LEN] GZIP_ID (no \0)
// - [sizeof(Access)] index (should be allocated, contains various sizes)
// - [rest] the indexed data points (should be allocated, index->list should then point to it)
static Access* ReadIndexFromFile(const wxString& filename)
{
	s64 size = fsize(filename);
	if (size <= 0)
	{
		Console.Error(L"Error: Can't open index file: '%s'", WX_STR(filename));
		return 0;
	}
	std::ifstream infile(PX_wfilename(filename), std::ifstream::binary);

	char fileId[GZIP_ID_LEN + 1] = {0};
	infile.read(fileId, GZIP_ID_LEN);
	if (wxString::From8BitData(GZIP_ID) != wxString::From8BitData(fileId))
	{
		Console.Error(L"Error: Incompatible gzip index, please delete it manually: '%s'", WX_STR(filename));
		infile.close();
		return 0;
	}

	Access* index = (Access*)malloc(sizeof(Access));
	infile.read((char*)index, sizeof(Access));

	s64 datasize = size - GZIP_ID_LEN - sizeof(Access);
	if (datasize != (s64)index->have * sizeof(Point))
	{
		Console.Error(L"Error: unexpected size of gzip index, please delete it manually: '%s'.", WX_STR(filename));
		infile.close();
		free(index);
		return 0;
	}

	char* buffer = (char*)malloc(datasize);
	infile.read(buffer, datasize);
	infile.close();
	index->list = (Point*)buffer; // adjust list pointer
	return index;
}

static void WriteIndexToFile(Access* index, const wxString filename)
{
	if (wxFileName::FileExists(filename))
	{
		Console.Warning(L"WARNING: Won't write index - file name exists (please delete it manually): '%s'", WX_STR(filename));
		return;
	}

	std::ofstream outfile(PX_wfilename(filename), std::ofstream::binary);
	outfile.write(GZIP_ID, GZIP_ID_LEN);

	Point* tmp = index->list;
	index->list = 0; // current pointer is useless on disk, normalize it as 0.
	outfile.write((char*)index, sizeof(Access));
	index->list = tmp;

	outfile.write((char*)index->list, sizeof(Point) * index->have);
	outfile.close();

	// Verify
	if (fsize(filename) != (s64)GZIP_ID_LEN + sizeof(Access) + sizeof(Point) * index->have)
	{
		Console.Warning(L"Warning: Can't write index file to disk: '%s'", WX_STR(filename));
	}
	else
	{
		Console.WriteLn(Color_Green, L"OK: Gzip quick access index file saved to disk: '%s'", WX_STR(filename));
	}
}

static wxString INDEX_TEMPLATE_KEY(L"$(f)");
// template:
// must contain one and only one instance of '$(f)' (without the quotes)
// if if !canEndWithKey -> must not end with $(f)
// if starts with $(f) then it expands to the full path + file name.
// if doesn't start with $(f) then it's expanded to file name only (with extension)
// if doesn't start with $(f) and ends up relative,
//   then it's relative to base (not to cwd)
// No checks are performed if the result file name can be created.
// If this proves useful, we can move it into Path:: . Right now there's no need.
static wxString ApplyTemplate(const wxString& name, const wxDirName& base,
							  const wxString& fileTemplate, const wxString& filename,
							  bool canEndWithKey)
{
	wxString tem(fileTemplate);
	wxString key = INDEX_TEMPLATE_KEY;
	tem = tem.Trim(true).Trim(false); // both sides

	size_t first = tem.find(key);
	if (first == wxString::npos    // not found
		|| first != tem.rfind(key) // more than one instance
		|| !canEndWithKey && first == tem.length() - key.length())
	{
		Console.Error(L"Invalid %s template '%s'.\n"
					  L"Template must contain exactly one '%s' and must not end with it. Abotring.",
					  WX_STR(name), WX_STR(tem), WX_STR(key));
		return L"";
	}

	wxString fname(filename);
	if (first > 0)
		fname = Path::GetFilename(fname); // without path

	tem.Replace(key, fname);
	if (first > 0)
		tem = Path::Combine(base, tem); // ignores appRoot if tem is absolute

	return tem;
}

/*
static void TestTemplate(const wxDirName &base, const wxString &fname, bool canEndWithKey)
{
	const char *ins[] = {
		"$(f).pindex.tmp",                    // same folder as the original file
		"	$(f).pindex.tmp ",                // same folder as the original file (trimmed silently)
		"cache/$(f).pindex",                  // relative to base
		"../$(f).pindex",                     // relative to base
		"%appdata%/pcsx2/cache/$(f).pindex",  // c:/Users/<user>/AppData/Roaming/pcsx2/cache/ ...
		"c:\\pcsx2-cache/$(f).pindex",        // absolute
		"~/.cache/$(f).pindex",	              // TODO: check if this works on *nix. It should...
		                                      //       (on windows ~ isn't recognized as special)
		"cache/$(f)/$(f).index",              // invalid: appears twice
		"hello",                              // invalid: doesn't contain $(f)
		"hello$(f)",                          // invalid, can't end with $(f)
		NULL
	};

	for (int i = 0; ins[i]; i++) {
		wxString tem(wxString::From8BitData(ins[i]));
		Console.WriteLn(Color_Green, L"test: '%s' -> '%s'",
		                WX_STR(tem),
		                WX_STR(ApplyTemplate(L"test", base, tem, fname, canEndWithKey)));
	}
}
*/

static wxString iso2indexname(const wxString& isoname)
{
	//testTemplate(isoname);
	wxDirName appRoot = // TODO: have only one of this in PCSX2. Right now have few...
		(wxDirName)(wxFileName(wxStandardPaths::Get().GetExecutablePath()).GetPath());
	//TestTemplate(appRoot, isoname, false);
	return ApplyTemplate(L"gzip index", appRoot, EmuConfig.GzipIsoIndexTemplate, isoname, false);
}

GzippedFileReader::GzippedFileReader(void)
	: mBytesRead(0)
	, m_pIndex(0)
	, m_zstates(0)
	, m_src(0)
	, m_cache(GZFILE_CACHE_SIZE_MB)
{
	m_blocksize = 2048;
	AsyncPrefetchReset();
};

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
void GzippedFileReader::AsyncPrefetchChunk(PX_off_t dummy){};
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
		m_filename,
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

void GzippedFileReader::AsyncPrefetchChunk(PX_off_t start)
{
	if (hOverlappedFile == INVALID_HANDLE_VALUE || asyncInProgress)
	{
		Console.Warning(L"Unexpected file handle or progress state. Aborting prefetch.");
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
		Console.Warning("canceling gz prefetch failed. following prefetching will not work.");
		return;
	}

	asyncInProgress = false;
};
#endif /* _WIN32 */

// TODO: do better than just checking existance and extension
bool GzippedFileReader::CanHandle(const wxString& fileName)
{
	return wxFileName::FileExists(fileName) && fileName.Lower().EndsWith(L".gz");
}

bool GzippedFileReader::OkIndex()
{
	if (m_pIndex)
		return true;

	// Try to read index from disk
	wxString indexfile = iso2indexname(m_filename);
	if (indexfile.length() == 0)
		return false; // iso2indexname(...) will print errors if it can't apply the template

	if (wxFileName::FileExists(indexfile) && (m_pIndex = ReadIndexFromFile(indexfile)))
	{
		Console.WriteLn(Color_Green, L"OK: Gzip quick access index read from disk: '%s'", WX_STR(indexfile));
		if (m_pIndex->span != GZFILE_SPAN_DEFAULT)
		{
			Console.Warning(L"Note: This index has %1.1f MB intervals, while the current default for new indexes is %1.1f MB.",
							(float)m_pIndex->span / 1024 / 1024, (float)GZFILE_SPAN_DEFAULT / 1024 / 1024);
			Console.Warning(L"It will work fine, but if you want to generate a new index with default intervals, delete this index file.");
			Console.Warning(L"(smaller intervals mean bigger index file and quicker but more frequent decompressions)");
		}
		InitZstates();
		return true;
	}

	// No valid index file. Generate an index
	Console.Warning(L"This may take a while (but only once). Scanning compressed file to generate a quick access index...");

	Access* index;
	FILE* infile = PX_fopen_rb(m_filename);
	int len = build_index(infile, GZFILE_SPAN_DEFAULT, &index);
	printf("\n"); // build_index prints progress without \n's
	fclose(infile);

	if (len >= 0)
	{
		m_pIndex = index;
		WriteIndexToFile((Access*)m_pIndex, indexfile);
	}
	else
	{
		Console.Error(L"ERROR (%d): index could not be generated for file '%s'", len, WX_STR(m_filename));
		free_index(index);
		InitZstates();
		return false;
	}

	InitZstates();
	return true;
}

bool GzippedFileReader::Open(const wxString& fileName)
{
	Close();
	m_filename = fileName;
	if (!(m_src = PX_fopen_rb(m_filename)) || !CanHandle(fileName) || !OkIndex())
	{
		Close();
		return false;
	};

	AsyncPrefetchOpen();
	return true;
};

void GzippedFileReader::BeginRead(void* pBuffer, uint sector, uint count)
{
	// No a-sync support yet, implement as sync
	mBytesRead = ReadSync(pBuffer, sector, count);
	return;
};

int GzippedFileReader::FinishRead(void)
{
	int res = mBytesRead;
	mBytesRead = -1;
	return res;
};

#define PTT clock_t
#define NOW() (clock() / (CLOCKS_PER_SEC / 1000))

int GzippedFileReader::ReadSync(void* pBuffer, uint sector, uint count)
{
	PX_off_t offset = (s64)sector * m_blocksize + m_dataoffset;
	int bytesToRead = count * m_blocksize;
	int res = _ReadSync(pBuffer, offset, bytesToRead);
	if (res < 0)
		Console.Error(L"Error: iso-gzip read unsuccessful.");
	return res;
}

// If we have a valid and adequate zstate for this span, use it, else, use the index
PX_off_t GzippedFileReader::GetOptimalExtractionStart(PX_off_t offset)
{
	int span = m_pIndex->span;
	Czstate& cstate = m_zstates[offset / span];
	PX_off_t stateOffset = cstate.state.isValid ? cstate.state.out_offset : 0;
	if (stateOffset && stateOffset <= offset)
		return stateOffset; // state is faster than indexed

	// If span is not exact multiples of GZFILE_READ_CHUNK_SIZE (because it was configured badly),
	// we fallback to always GZFILE_READ_CHUNK_SIZE boundaries
	if (span % GZFILE_READ_CHUNK_SIZE)
		return offset / GZFILE_READ_CHUNK_SIZE * GZFILE_READ_CHUNK_SIZE;

	return span * (offset / span); // index direct access boundaries
}

int GzippedFileReader::_ReadSync(void* pBuffer, PX_off_t offset, uint bytesToRead)
{
	if (!OkIndex())
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
	PTT s = NOW();
	PX_off_t extractOffset = GetOptimalExtractionStart(offset); // guaranteed in GZFILE_READ_CHUNK_SIZE boundaries
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
			int available = CLAMP(res - i, 0, GZFILE_READ_CHUNK_SIZE);
			void* chunk = available ? malloc(available) : 0;
			if (available)
				memcpy(chunk, extracted + i, available);
			m_cache.Take(chunk, extractOffset + i, available, std::min(size - i, GZFILE_READ_CHUNK_SIZE));
		}
		free(extracted);
	}

	int duration = NOW() - s;
	if (duration > 10)
		Console.WriteLn(Color_Gray, L"gunzip: chunk #%5d-%2d : %1.2f MB - %d ms",
						(int)(offset / 4 / 1024 / 1024),
						(int)(offset % (4 * 1024 * 1024) / GZFILE_READ_CHUNK_SIZE),
						(float)size / 1024 / 1024,
						duration);

	return copied;
}

void GzippedFileReader::Close()
{
	m_filename.Empty();
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
		m_src = 0;
	}

	AsyncPrefetchClose();
}
