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

#pragma once

#include "MultiISA.h"

#ifndef XXH_versionNumber
	#define XXH_STATIC_LINKING_ONLY 1
	#define XXH_INLINE_ALL 1
	#include <xxhash.h>
#endif

MULTI_ISA_DEF(u64 GSXXH3_64_Long(const void* data, size_t len);)
MULTI_ISA_DEF(u32 GSXXH3_64_Update(void* state, const void* data, size_t len);)
MULTI_ISA_DEF(u64 GSXXH3_64_Digest(void* state);)

static inline u64 __forceinline GSXXH3_64bits(const void* data, size_t len)
{
	// XXH3 has optimized functions for small inputs and they aren't vectorized
	if (len <= XXH3_MIDSIZE_MAX)
		return XXH3_64bits(data, len);
	return MultiISAFunctions::GSXXH3_64_Long(data, len);
}

static inline XXH_errorcode __forceinline GSXXH3_64bits_update(XXH3_state_t* state, const void* input, size_t len)
{
	// XXH3 update has no optimized functions for small inputs
	return static_cast<XXH_errorcode>(MultiISAFunctions::GSXXH3_64_Update(static_cast<void*>(state), input, len));
}

static inline u64 __forceinline GSXXH3_64bits_digest(XXH3_state_t* state)
{
	return MultiISAFunctions::GSXXH3_64_Digest(static_cast<void*>(state));
}
