/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
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

#pragma once

#include <array>
#include "fmt/core.h"
#include "common/AlignedMalloc.h"

// nVifBlock - Ordered for Hashing; the 'num' and 'upkType' fields are
//             used as the hash bucket selector.
union nVifBlock
{
	// Warning: order depends on the newVifDynaRec code
	struct
	{
		u8 num;        // [00] Num Field
		u8 upkType;    // [01] Unpack Type [usn1:mask1:upk*4]
		u16 length;    // [02] Extra: pre computed Length
		u32 mask;      // [04] Mask Field
		u8 mode;       // [08] Mode Field
		u8 aligned;    // [09] Packet Alignment
		u8 cl;         // [10] CL Field
		u8 wl;         // [11] WL Field
		uptr startPtr; // [12] Start Ptr of RecGen Code
	};

	struct
	{
		u16 hash_key;
		u16 _pad0;
		u32 key0;
		u32 key1;
		uptr value;
	};

}; // 16 bytes

// 0x4000 is enough but 0x10000 allow
// * to skip the compare value of the first double world in lookup
// * to use a 16 bits move instead of an 'and' mask to compute the hashed key
#define hSize 0x10000 // [usn*1:mask*1:upk*4:num*8] hash...

// HashBucket is a container which uses a built-in hash function
// to perform quick searches. It is designed around the nVifBlock structure
//
// The hash function is determined by taking the first bytes of data and
// performing a modulus the size of hSize. So the most diverse-data should
// be in the first bytes of the struct. (hence why nVifBlock is specifically sorted)
class HashBucket
{
protected:
	std::array<nVifBlock*, hSize> m_bucket;

public:
	HashBucket()
	{
		m_bucket.fill(nullptr);
	}

	~HashBucket() { clear(); }

	__fi nVifBlock* find(const nVifBlock& dataPtr)
	{
		nVifBlock* chainpos = m_bucket[dataPtr.hash_key];

		while (true)
		{
			if (chainpos->key0 == dataPtr.key0 && chainpos->key1 == dataPtr.key1)
				return chainpos;

			if (chainpos->startPtr == 0)
				return nullptr;

			chainpos++;
		}
	}

	void add(const nVifBlock& dataPtr)
	{
		u32 b = dataPtr.hash_key;

		u32 size = bucket_size(dataPtr);

		// Warning there is an extra +1 due to the empty cell
		// Performance note: 64B align to reduce cache miss penalty in `find`
		if ((m_bucket[b] = (nVifBlock*)pcsx2_aligned_realloc(m_bucket[b], sizeof(nVifBlock) * (size + 2), 64, sizeof(nVifBlock) * (size + 1))) == NULL)
		{
			throw Exception::OutOfMemory(
				fmt::format("HashBucket Chain (bucket size={})", size + 2));
		}

		// Replace the empty cell by the new block and create a new empty cell
		memcpy(&m_bucket[b][size++], &dataPtr, sizeof(nVifBlock));
		memset(&m_bucket[b][size], 0, sizeof(nVifBlock));

		if (size > 3)
			DevCon.Warning("recVifUnpk: Bucket 0x%04x has %d micro-programs", b, size);
	}

	u32 bucket_size(const nVifBlock& dataPtr)
	{
		nVifBlock* chainpos = m_bucket[dataPtr.hash_key];

		u32 size = 0;

		while (chainpos->startPtr != 0)
		{
			size++;
			chainpos++;
		}

		return size;
	}

	void clear()
	{
		for (auto& bucket : m_bucket)
			safe_aligned_free(bucket);
	}

	void reset()
	{
		clear();

		// Allocate an empty cell for all buckets
		for (auto& bucket : m_bucket)
		{
			if ((bucket = (nVifBlock*)_aligned_malloc(sizeof(nVifBlock), 64)) == nullptr)
			{
				throw Exception::OutOfMemory(fmt::format("HashBucket Chain (bucket size=%d)", 1));
			}

			memset(bucket, 0, sizeof(nVifBlock));
		}
	}
};
