// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "MultiISA.h"

// These get pulled in by xxhash.h in non-PCH mode, so we need to include them in global namespace scope.
#include <cmath>
#include <cstdlib>

#define XXH_STATIC_LINKING_ONLY 1
#define XXH_INLINE_ALL 1
namespace CURRENT_ISA // XXH doesn't seem to use symbols that allow the compiler to deduplicate, but just in case...
{
#include <xxhash.h>
}

MULTI_ISA_UNSHARED_IMPL;

// Include this after xxhash so we can add namespaces (GSXXH is set up to not include xxhash header if it's already been included)
#include "GSXXH.h"

u64 __noinline CURRENT_ISA::GSXXH3_64_Long(const void* data, size_t len)
{
	// XXH marks its function that calls this noinline, and it would be silly to stack noinline functions, so call the internal function directly
	return XXH3_hashLong_64b_internal(data, len, XXH3_kSecret, sizeof(XXH3_kSecret), XXH3_accumulate_512, XXH3_scrambleAcc);
}

u32 CURRENT_ISA::GSXXH3_64_Update(void* state, const void* data, size_t len)
{
	return XXH3_64bits_update(static_cast<XXH3_state_t*>(state), static_cast<const xxh_u8*>(data), len);
}

u64 CURRENT_ISA::GSXXH3_64_Digest(void* state)
{
	return XXH3_64bits_digest(static_cast<XXH3_state_t*>(state));
}
