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

// nVifBlock - Ordered for Hashing; the 'num' field and the lower 6 bits of upkType are
//             used as the hash bucket selector.
struct __aligned16 nVifBlock {
	u8 num;			// [00] Num Field
	u8 upkType; 	// [01] Unpack Type [usn1:mask1:upk*4]
	u16 length; 	// [02] Extra: pre computed Length
	u32 mask;		// [04] Mask Field
	u8 mode;		// [08] Mode Field
	u8 aligned; 	// [09] Packet Alignment
	u8 cl;			// [10] CL Field
	u8 wl;			// [11] WL Field
	uptr startPtr;	// [12] Start Ptr of RecGen Code
}; // 16 bytes

#define hSize 0x4000 // [usn*1:mask*1:upk*4:num*8] hash...

// HashBucket is a container which uses a built-in hash function
// to perform quick searches. It is designed around the nVifBlock structure
//
// The hash function is determined by taking the first bytes of data and
// performing a modulus the size of hSize. So the most diverse-data should
// be in the first bytes of the struct. (hence why nVifBlock is specifically sorted)
class HashBucket {
protected:
	std::array<nVifBlock*, hSize> m_bucket;

public:
	HashBucket() {
		m_bucket.fill(nullptr);
	}

	~HashBucket() throw() { clear(); }

	__fi nVifBlock* find(nVifBlock* dataPtr) {
		u32 d = *((u32*)dataPtr);
		const __m128i* chainpos = (__m128i*)m_bucket[d % m_bucket.size()];

		const __m128i data128( _mm_load_si128((__m128i*)dataPtr) );

		int result;
		do {
			// This inline SSE code is generally faster than using emitter code, since it inlines nicely. --air
			result = _mm_movemask_ps( _mm_castsi128_ps( _mm_cmpeq_epi32( data128, _mm_load_si128(chainpos) ) ) );
			// startPtr doesn't match (aka not nullptr) hence 4th bit must be 0
			if (result == 0x7) return (nVifBlock*)chainpos;

			chainpos += sizeof(nVifBlock) / sizeof(__m128i);

		} while(result < 0x8);

		return nullptr;
	}

	void add(const nVifBlock& dataPtr) {
		u32 d = (u32&)dataPtr;
		u32 b = d % m_bucket.size();

		u32 size = bucket_size( dataPtr );

		// Warning there is an extra +1 due to the empty cell
		if( (m_bucket[b] = (nVifBlock*)pcsx2_aligned_realloc( m_bucket[b], sizeof(nVifBlock)*(size+2), 16, sizeof(nVifBlock)*(size+1) )) == NULL ) {
			throw Exception::OutOfMemory(
				wxsFormat(L"HashBucket Chain (bucket size=%d)", size+2)
			);
		}

		// Replace the empty cell by the new block and create a new empty cell
		memcpy(&m_bucket[b][size++], &dataPtr, sizeof(nVifBlock));
		memset(&m_bucket[b][size], 0, sizeof(nVifBlock));

		if( size > 3 ) DevCon.Warning( "recVifUnpk: Bucket 0x%04x has %d micro-programs", b, size );
	}

	u32 bucket_size(const nVifBlock& dataPtr) {
		u32 d = (u32&)dataPtr;
		nVifBlock* chainpos = m_bucket[d % m_bucket.size()];

		u32 size = 0;

		while (chainpos->startPtr != 0) {
			size++;
			chainpos++;
		}

		return size;
	}

	void clear() {
		for (auto& bucket : m_bucket)
			safe_aligned_free(bucket);
	}

	void reset() {
		clear();

		// Allocate an empty cell for all buckets
		for (auto& bucket : m_bucket) {
			if( (bucket = (nVifBlock*)_aligned_malloc( sizeof(nVifBlock), 16 )) == nullptr ) {
				throw Exception::OutOfMemory(
						wxsFormat(L"HashBucket Chain (bucket size=%d)", 1)
						);
			}

			memset(bucket, 0, sizeof(nVifBlock));
		}
	}
};
