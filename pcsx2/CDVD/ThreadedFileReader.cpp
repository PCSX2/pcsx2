/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021 PCSX2 Dev Team
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
#include "ThreadedFileReader.h"

ThreadedFileReader::ThreadedFileReader()
{
	m_readThread = std::thread([](ThreadedFileReader* r){ r->Loop(); }, this);
}

ThreadedFileReader::~ThreadedFileReader()
{
	m_quit = true;
	(void)std::lock_guard<std::mutex>{m_mtx};
	m_condition.notify_one();
	m_readThread.join();
	for (auto& buffer : m_buffer)
		if (buffer.ptr)
			free(buffer.ptr);
}

size_t ThreadedFileReader::CopyBlocks(void* dst, const void* src, size_t size) const
{
	char* cdst = static_cast<char*>(dst);
	const char* csrc = static_cast<const char*>(src);
	const char* cend = csrc + size;
	if (m_internalBlockSize)
	{
		for (; csrc < cend; csrc += m_internalBlockSize, cdst += m_blocksize)
		{
			memcpy(cdst, csrc, m_blocksize);
		}
		return cdst - static_cast<char*>(dst);
	}
	else
	{
		memcpy(dst, src, size);
		return size;
	}
}

void ThreadedFileReader::Loop()
{
	Threading::SetNameOfCurrentThread("ISO Decompress");

	std::unique_lock<std::mutex> lock(m_mtx);

	while (true)
	{
		while (!m_requestSize && !m_quit)
			m_condition.wait(lock);

		if (m_quit)
			return;

		u64 requestOffset = m_requestOffset;
		u32 requestSize = m_requestSize;
		void* ptr = m_requestPtr.load(std::memory_order_relaxed);

		m_running = true;
		lock.unlock();

		bool ok = true;

		if (ptr)
		{
			ok = Decompress(ptr, requestOffset, requestSize);
		}

		m_requestPtr.store(nullptr, std::memory_order_release);
		m_condition.notify_one();

		if (ok)
		{
			// Readahead
			Chunk blk = ChunkForOffset(requestOffset + requestSize);
			if (blk.chunkID >= 0)
			{
				(void)GetBlockPtr(blk, true);
				blk = ChunkForOffset(blk.offset + blk.length);
				if (blk.chunkID >= 0)
					(void)GetBlockPtr(blk, true);
			}
		}

		lock.lock();
		if (requestSize == m_requestSize && requestOffset == m_requestOffset && !m_requestPtr)
		{
			// If no one's added more work, mark this one as done
			m_requestSize = 0;
		}

		m_running = false;
		m_condition.notify_one(); // For things waiting on m_running == false
	}
}

ThreadedFileReader::Buffer* ThreadedFileReader::GetBlockPtr(const Chunk& block, bool isReadahead)
{
	for (int i = 0; i < static_cast<int>(ArraySize(m_buffer)); i++)
	{
		if (m_buffer[i].valid.load(std::memory_order_acquire) && m_buffer[i].offset == block.offset)
		{
			m_nextBuffer = (i + 1) % ArraySize(m_buffer);
			return m_buffer + i;
		}
	}

	Buffer& buf = m_buffer[m_nextBuffer];
	{
		// This can be called from both the read thread threads in ReadSync
		// Calls from ReadSync are done with the lock already held to keep the read thread out
		// Therefore we should only lock on the read thread
		std::unique_lock<std::mutex> lock(m_mtx, std::defer_lock);
		if (std::this_thread::get_id() == m_readThread.get_id())
			lock.lock();
		if (buf.size < block.length)
			buf.ptr = realloc(buf.ptr, block.length);
		buf.valid.store(false, std::memory_order_relaxed);

	}
	int size = ReadChunk(buf.ptr, block.chunkID);
	if (size > 0)
	{
		buf.offset = block.offset;
		buf.size = size;
		buf.valid.store(true, std::memory_order_release);
		m_nextBuffer = (m_nextBuffer + 1) % ArraySize(m_buffer);
		return &buf;
	}
	return nullptr;
}

bool ThreadedFileReader::Decompress(void* target, u64 begin, u32 size)
{
	Chunk blk = ChunkForOffset(begin);
	char* write = static_cast<char*>(target);
	u32 remaining = size;
	if (blk.offset != begin)
	{
		u32 off = begin - blk.offset;
		u32 len = std::min(blk.length - off, size);
		// Partial block
		if (Buffer* buf = GetBlockPtr(blk))
		{
			if (buf->size < blk.length)
				return false;
			write += CopyBlocks(write, static_cast<char*>(buf->ptr) + off, len);
			remaining -= len;
			blk = ChunkForOffset(blk.offset + blk.length);
		}
		else
		{
			return false;
		}
	}
	while (blk.length <= remaining)
	{
		if (m_requestCancelled.load(std::memory_order_relaxed))
		{
			return false;
		}
		if (m_internalBlockSize)
		{
			if (Buffer* buf = GetBlockPtr(blk))
			{
				if (buf->size < blk.length)
					return false;
				write += CopyBlocks(write, buf->ptr, blk.length);
			}
		}
		else
		{
			int amt = ReadChunk(write, blk.chunkID);
			if (amt < static_cast<int>(blk.length))
				return false;
			write += blk.length;
		}
		remaining -= blk.length;
		blk = ChunkForOffset(blk.offset + blk.length);
	}
	if (remaining)
	{
		if (Buffer* buf = GetBlockPtr(blk))
		{
			if (buf->size < remaining)
				return false;
			write += CopyBlocks(write, buf->ptr, remaining);
		}
		else
		{
			return false;
		}
	}
	m_amtRead += write - static_cast<char*>(target);
	return true;
}

bool ThreadedFileReader::TryCachedRead(void*& buffer, u64& offset, u32& size, const std::lock_guard<std::mutex>&)
{
	// Run through twice so that if m_buffer[1] contains the first half and m_buffer[0] contains the second half it still works
	m_amtRead = 0;
	u64 end = 0;
	bool allDone = false;
	for (int i = 0; i < static_cast<int>(ArraySize(m_buffer) * 2); i++)
	{
		Buffer& buf = m_buffer[i % ArraySize(m_buffer)];
		if (!buf.valid.load(std::memory_order_acquire))
			continue;
		if (buf.offset <= offset && buf.offset + buf.size > offset)
		{
			u32 off = offset - buf.offset;
			u32 cpysize = std::min(size, buf.size - off);
			size_t read = CopyBlocks(buffer, static_cast<char*>(buf.ptr) + off, cpysize);
			m_amtRead += read;
			size -= cpysize;
			offset += cpysize;
			buffer = static_cast<char*>(buffer) + read;
			if (size == 0)
				end = buf.offset + buf.size;
		}
		// Do buffers contain the current and next block?
		if (end > 0 && buf.offset == end)
			allDone = true;
	}
	return allDone;
}

bool ThreadedFileReader::Open(const wxString& fileName)
{
	CancelAndWaitUntilStopped();
	return Open2(fileName);
}

int ThreadedFileReader::ReadSync(void* pBuffer, uint sector, uint count)
{
	u32 blocksize = InternalBlockSize();
	u64 offset = (u64)sector * (u64)blocksize + m_dataoffset;
	u32 size = count * blocksize;
	{
		std::lock_guard<std::mutex> l(m_mtx);
		if (TryCachedRead(pBuffer, offset, size, l))
			return m_amtRead;

		if (size > 0 && !m_running)
		{
			// Don't wait for read thread to start back up
			if (Decompress(pBuffer, offset, size))
			{
				offset += size;
				size = 0;
			}
		}

		if (size == 0)
		{
			// For readahead
			m_requestOffset = offset - 1;
			m_requestSize = 1;
			m_requestPtr.store(nullptr, std::memory_order_relaxed);
		}
		else
		{
			m_requestOffset = offset;
			m_requestSize = size;
			m_requestPtr.store(pBuffer, std::memory_order_relaxed);
		}
		m_requestCancelled.store(false, std::memory_order_relaxed);
	}
	m_condition.notify_one();
	if (size == 0)
		return m_amtRead;
	return FinishRead();
}

void ThreadedFileReader::CancelAndWaitUntilStopped(void)
{
	m_requestCancelled.store(true, std::memory_order_relaxed);
	std::unique_lock<std::mutex> lock(m_mtx);
	while (m_running)
		m_condition.wait(lock);
}

void ThreadedFileReader::BeginRead(void* pBuffer, uint sector, uint count)
{
	s32 blocksize = InternalBlockSize();
	u64 offset = (u64)sector * (u64)blocksize + m_dataoffset;
	u32 size = count * blocksize;
	{
		std::lock_guard<std::mutex> l(m_mtx);
		if (TryCachedRead(pBuffer, offset, size, l))
			return;
		if (size == 0)
		{
			// For readahead
			m_requestOffset = offset - 1;
			m_requestSize = 1;
			m_requestPtr.store(nullptr, std::memory_order_relaxed);
		}
		else
		{
			m_requestOffset = offset;
			m_requestSize = size;
			m_requestPtr.store(pBuffer, std::memory_order_relaxed);
		}
		m_requestCancelled.store(false, std::memory_order_relaxed);
	}
	m_condition.notify_one();
}

int ThreadedFileReader::FinishRead(void)
{
	if (m_requestPtr.load(std::memory_order_acquire) == nullptr)
		return m_amtRead;
	std::unique_lock<std::mutex> lock(m_mtx);
	while (m_requestPtr)
		m_condition.wait(lock);
	return m_amtRead;
}

void ThreadedFileReader::CancelRead(void)
{
	if (m_requestPtr.load(std::memory_order_acquire) == nullptr)
		return;
	m_requestCancelled.store(true, std::memory_order_release);
	std::unique_lock<std::mutex> lock(m_mtx);
	while (m_requestPtr.load(std::memory_order_relaxed))
		m_condition.wait(lock);
}

void ThreadedFileReader::Close(void)
{
	CancelAndWaitUntilStopped();
	for (auto& buf : m_buffer)
		buf.valid.store(false, std::memory_order_relaxed);
	Close2();
}

void ThreadedFileReader::SetBlockSize(uint bytes)
{
	m_blocksize = bytes;
}

void ThreadedFileReader::SetDataOffset(int bytes)
{
	m_dataoffset = bytes;
}
