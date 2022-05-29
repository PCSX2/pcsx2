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

#include "common/Threading.h"

// Make sure buffer size is bigger than the cutoff where PCSX2 emulates a seek
// If buffers are smaller than that, we can't keep up with linear reads
static constexpr u32 MINIMUM_SIZE = 128 * 1024;

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

		void* ptr;
		u64 requestOffset;
		u32 requestSize;

		bool ok = true;
		m_running = true;

		for (;;)
		{
			ptr = m_requestPtr.load(std::memory_order_acquire);
			requestOffset = m_requestOffset;
			requestSize = m_requestSize;
			lock.unlock();

			if (ptr)
				ok = Decompress(ptr, requestOffset, requestSize);

			// There's a potential for a race here when doing synchronous reads. Basically, another request can come in,
			// after we release the lock, but before we store null to indicate we're finished. So, we do a compare-exchange
			// instead, to detect when this happens, and if so, reload all the inputs and try again.
			if (!m_requestPtr.compare_exchange_strong(ptr, nullptr, std::memory_order_release))
			{
				lock.lock();
				continue;
			}

			m_condition.notify_one();
			break;
		}

		if (ok)
		{
			// Readahead
			Chunk chunk = ChunkForOffset(requestOffset + requestSize);
			if (chunk.chunkID >= 0)
			{
				int buffersFilled = 0;
				Buffer* buf = GetBlockPtr(chunk);
				// Cancel readahead if a new request comes in
				while (buf && !m_requestPtr.load(std::memory_order_acquire))
				{
					u32 bufsize = buf->size.load(std::memory_order_relaxed);
					chunk = ChunkForOffset(buf->offset + bufsize);
					if (chunk.chunkID < 0)
						break;
					if (buf->offset + bufsize != chunk.offset || chunk.length + bufsize > buf->cap)
					{
						buffersFilled++;
						if (buffersFilled >= 2)
							break;
						buf = GetBlockPtr(chunk);
					}
					else
					{
						int amt = ReadChunk(static_cast<char*>(buf->ptr) + bufsize, chunk.chunkID);
						if (amt <= 0)
							break;
						buf->size.store(bufsize + amt, std::memory_order_release);
					}
				}
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

ThreadedFileReader::Buffer* ThreadedFileReader::GetBlockPtr(const Chunk& block)
{
	for (int i = 0; i < static_cast<int>(std::size(m_buffer)); i++)
	{
		u32 size = m_buffer[i].size.load(std::memory_order_relaxed);
		u64 offset = m_buffer[i].offset;
		if (size && offset <= block.offset && offset + size >= block.offset + block.length)
		{
			m_nextBuffer = (i + 1) % std::size(m_buffer);
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
		u32 size = std::max(block.length, MINIMUM_SIZE);
		if (buf.cap < size)
		{
			buf.ptr = realloc(buf.ptr, size);
			buf.cap = size;
		}
		buf.size.store(0, std::memory_order_relaxed);
	}
	int size = ReadChunk(buf.ptr, block.chunkID);
	if (size > 0)
	{
		buf.offset = block.offset;
		buf.size.store(size, std::memory_order_release);
		m_nextBuffer = (m_nextBuffer + 1) % std::size(m_buffer);
		return &buf;
	}
	return nullptr;
}

bool ThreadedFileReader::Decompress(void* target, u64 begin, u32 size)
{
	char* write = static_cast<char*>(target);
	u32 remaining = size;
	u64 off = begin;
	while (remaining)
	{
		Chunk chunk = ChunkForOffset(off);
		if (m_internalBlockSize || chunk.offset != off || chunk.length > remaining)
		{
			Buffer* buf = GetBlockPtr(chunk);
			if (!buf)
				return false;
			u32 bufoff = off - buf->offset;
			u32 bufsize = buf->size.load(std::memory_order_relaxed);
			if (bufsize <= bufoff)
				return false;
			u32 len = std::min(bufsize - bufoff, remaining);
			write += CopyBlocks(write, static_cast<char*>(buf->ptr) + bufoff, len);
			remaining -= len;
			off += len;
		}
		else
		{
			int amt = ReadChunk(write, chunk.chunkID);
			if (amt < static_cast<int>(chunk.length))
				return false;
			write += chunk.length;
			remaining -= chunk.length;
			off += chunk.length;
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
	for (int i = 0; i < static_cast<int>(std::size(m_buffer) * 2); i++)
	{
		Buffer& buf = m_buffer[i % std::size(m_buffer)];
		u32 bufsize = buf.size.load(std::memory_order_acquire);
		if (!bufsize)
			continue;
		if (buf.offset <= offset && buf.offset + bufsize > offset)
		{
			u32 off = offset - buf.offset;
			u32 cpysize = std::min(size, bufsize - off);
			size_t read = CopyBlocks(buffer, static_cast<char*>(buf.ptr) + off, cpysize);
			m_amtRead += read;
			size -= cpysize;
			offset += cpysize;
			buffer = static_cast<char*>(buffer) + read;
			if (size == 0)
				end = buf.offset + bufsize;
		}
		// Do buffers contain the current and next block?
		if (end > 0 && buf.offset == end)
			allDone = true;
	}
	return allDone;
}

bool ThreadedFileReader::Open(std::string fileName)
{
	CancelAndWaitUntilStopped();
	return Open2(std::move(fileName));
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
	while (m_requestPtr.load(std::memory_order_acquire))
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
		buf.size.store(0, std::memory_order_relaxed);
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
