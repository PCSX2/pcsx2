/*  PCSX2 - PS2 Emulator for PCs
*  Copyright (C) 2002-2016  PCSX2 Dev Team
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
#include "AppConfig.h"
#include "LzmaFileReader.h"

#include <fstream>

#ifdef ISO_LZMA_READER

LzmaBlock::LzmaBlock(const std::shared_ptr<wxFile>& file, const lzma_index_iter& iter)
{
	m_fd = file;
	m_error = false;

	m_compressed_size = iter.block.total_size;
	m_compressed_offset = iter.block.compressed_file_offset;
	m_uncompressed_size = iter.block.uncompressed_size;
	m_uncompressed_offset = iter.block.uncompressed_file_offset;
	m_block_number = iter.block.number_in_file - 1;

	m_fd->Seek(iter.block.compressed_file_offset);

	std::vector<u8> header(1);
	m_fd->Read(header.data(), 1);

	u32 header_size = lzma_block_header_size_decode(header[0]);
	header.resize(header_size);

	m_fd->Read(&header[1], header_size - 1);

	m_header.version = 0; // FIXME
	m_header.check = iter.stream.flags->check;
	m_header.filters = m_filters;
	m_header.header_size = header_size;
	m_header.ignore_check = false; // XXX speed => true ???

	lzma_ret ret = lzma_block_header_decode(&m_header, nullptr, header.data());
	if (ret != LZMA_OK) {
		Console.Error("Failed to uncompress header %lu (%d)", m_block_number, ret);
		m_error = true;
	}

	ret = lzma_block_compressed_size(&m_header, iter.block.unpadded_size);
	if (ret != LZMA_OK) {
		Console.Error("Failed to validate header size %lu (%d)", m_block_number, ret);
		m_error = true;
	}
}

void LzmaBlock::Uncompress(u8* dest)
{
	std::vector<u8> compressed_data;
	compressed_data.resize(m_compressed_size);
	m_fd->Seek(m_compressed_offset); // FIXME skip the header ? (would need to update in_pos, in_size below)
	m_fd->Read(compressed_data.data(), m_compressed_size);

	size_t in_pos = m_header.header_size;
	size_t out_pos = 0;

	// Ensure filter pointer is still valid if data was moved
	m_header.filters = m_filters;

	lzma_ret ret = lzma_block_buffer_decode(&m_header, nullptr,
			compressed_data.data(), &in_pos, m_compressed_size,
			dest, &out_pos, m_uncompressed_size);

	if (ret != LZMA_OK) {
		m_error = true;
		Console.Error("Failed to uncompress block %lu (%d)", m_block_number, ret);
	}
}

//////////////////////////////////////////////////////////////////////

LzmaFileCache::LzmaFileCache(const wxString& file, int block_nb, int block_size) :
	 m_filename(file), m_data(block_size), m_old_index(~0ULL)
{
	// wxFile::read_write doesn't create the file. So you must first use wxFile::write
	// to create an empty file
	m_cache = std::make_unique<wxFile>(m_filename, wxFile::write);
	m_cache = std::make_unique<wxFile>(m_filename, wxFile::read_write);

	for (int i = 0; i < block_nb; i++)
		m_state.push_back(STATE::INVALID);
}

LzmaFileCache::~LzmaFileCache()
{
	wxRemoveFile(m_filename);
}

u8* LzmaFileCache::MapBlock(LzmaBlock& block)
{
	u64 index = block.Index();
	if (index >= m_state.size())
		return nullptr;

	if (index == m_old_index)
		return m_data.data();

	switch (m_state[index]) {
		case STATE::INVALID:
			block.Uncompress(m_data.data());
			m_state[index] = STATE::VALID;

			m_cache->Seek(index * m_data.size());
			m_cache->Write(m_data.data(), m_data.size());

			break;

		case STATE::VALID:
			m_cache->Seek(index * m_data.size());
			m_cache->Read(m_data.data(), m_data.size());

			break;
	}

	return m_data.data();
}

//////////////////////////////////////////////////////////////////////


bool LzmaFileReader::CanHandle(const wxString& fileName) {
	return wxFileName::FileExists(fileName) && fileName.Lower().EndsWith(L".xz");
}

LzmaFileReader::LzmaFileReader()
{
	m_strm = LZMA_STREAM_INIT;
	m_uncompressed_file_size = 0;
	// XXX maybe it would be better to handle iso block logic in a common class
	m_blocksize = 2048;
}

lzma_index* LzmaFileReader::GetIndex(const std::shared_ptr<wxFile>& file)
{
	lzma_index *index = nullptr;
	u64 memlimit = _1gb;
	u8 buf[BUFSIZ];

#if LZMA_VERSION_MAJOR >= 5 && LZMA_VERSION_MINOR >= 3
	lzma_ret ret = lzma_file_info_decoder(&m_strm, &index, memlimit, file->Length());
#else
	lzma_ret ret = LZMA_STREAM_END; // A random error to abort nicely
#endif
	if (ret != LZMA_OK)
		throw Exception::BadStream()
			.SetUserMsg(_("Failed to initialize LZMA file info decoder"))
			.SetDiagMsg(L"Failed to initialize LZMA file info decoder");

	// Search indexes
	while (index == nullptr) {
		if (m_strm.avail_in == 0) {
			m_strm.next_in = buf;
			m_strm.avail_in = file->Read(buf, BUFSIZ);
		}

		ret = lzma_code(&m_strm, LZMA_RUN);

		switch (ret) {
			case LZMA_OK:
				// m_str.avail_in isn't yet 0
				break;

			case LZMA_SEEK_NEEDED:
				file->Seek(m_strm.seek_pos);
				// Ask to read more data
				m_strm.avail_in = 0;
				break;

			case LZMA_STREAM_END:
				// Job done, index is now valid :)
				break;

			default:
				throw Exception::BadStream()
					.SetUserMsg(_("Failed to decode LZMA index"))
					.SetDiagMsg(L"Failed to decode LZMA index");
		}
	}

	return index;
}

bool LzmaFileReader::Open(const wxString& fileName)
{
	std::shared_ptr<wxFile> file = std::make_shared<wxFile>(fileName);
	if (!file->IsOpened())
		throw Exception::BadStream()
			.SetUserMsg(wxsFormat(_("Failed to open %s"), WX_STR(fileName)))
			.SetDiagMsg(L"Failed to open LZMA file");

	lzma_index *index = GetIndex(file);

	lzma_index_iter iter;
	lzma_index_iter_init(&iter, index);

	m_uncompressed_file_size = lzma_index_uncompressed_size(index);

	m_blocks.reserve(lzma_index_block_count(index));

	// Decode all block headers
	m_lzma_blocksize = 0;
	while (!lzma_index_iter_next(&iter, LZMA_INDEX_ITER_BLOCK)) {
		m_blocks.emplace_back(file, iter);

		// Keep the uncompressed size of the first block
		if (!m_lzma_blocksize)
			m_lzma_blocksize = iter.block.uncompressed_size;
	}

	lzma_index_end(index, nullptr);

	for (const auto&b : m_blocks) {
		if (!b.IsGood())
			throw Exception::BadStream()
				.SetUserMsg(wxsFormat(_("Failed to lzma decode headers of %s"), WX_STR(fileName)))
				.SetDiagMsg(L"Failed to decode LZMA headers");
	}

	// FIXME option ? or better default dir ? Both :)
	m_cache = std::make_unique<LzmaFileCache>("/tmp/PCSX2-cache-xz.iso", m_blocks.size(), m_lzma_blocksize);

	return true;
}

void LzmaFileReader::Close()
{
	m_blocks.clear();
	lzma_end(&m_strm);
}

void LzmaFileReader::BeginRead(void* pBuffer, uint sector, uint count)
{
	m_bytes_read = ReadSync(pBuffer, sector, count);
}

int LzmaFileReader::FinishRead() {
	int res = m_bytes_read;
	m_bytes_read = -1;
	return res;
}

int LzmaFileReader::ReadSync(void* pBuffer, uint sector, uint count)
{
	u8* dest = (u8*)pBuffer;
	s64 pos = (s64)sector * (s64)m_blocksize + (s64)m_dataoffset;
	int remaining = count * m_blocksize;
	int bytes = 0;

	s64 lzma_block_index = pos / m_lzma_blocksize;
	s64 lzma_block_offset = pos - m_lzma_blocksize * lzma_block_index;

	while (remaining > 0) {
		LzmaBlock& b = m_blocks[lzma_block_index];

		u8* src = m_cache->MapBlock(b);
		int len = std::min(remaining, m_lzma_blocksize - (int)lzma_block_offset);

		memcpy(dest, &src[lzma_block_offset], len);

		remaining -= len;
		dest += len;
		bytes += len;

		lzma_block_index++;
		lzma_block_offset = 0;
	}

	return bytes;
}

#endif
