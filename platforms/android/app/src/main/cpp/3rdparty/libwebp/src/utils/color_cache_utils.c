// Copyright 2012 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// Color Cache for WebP Lossless
//
// Author: Jyrki Alakuijala (jyrki@google.com)

#include "src/utils/color_cache_utils.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "src/utils/bounds_safety.h"
#include "src/utils/utils.h"
#include "src/webp/types.h"

WEBP_ASSUME_UNSAFE_INDEXABLE_ABI

//------------------------------------------------------------------------------
// VP8LColorCache.

int VP8LColorCacheInit(VP8LColorCache* const color_cache, int hash_bits) {
  const int hash_size = 1 << hash_bits;
  uint32_t* colors = (uint32_t*)WebPSafeCalloc((uint64_t)hash_size,
                                               sizeof(*color_cache->colors));
  assert(color_cache != NULL);
  assert(hash_bits > 0);
  if (colors == NULL) {
    color_cache->colors = NULL;
    WEBP_SELF_ASSIGN(color_cache->hash_bits);
    return 0;
  }
  color_cache->hash_shift = 32 - hash_bits;
  color_cache->hash_bits = hash_bits;
  color_cache->colors = WEBP_UNSAFE_FORGE_BIDI_INDEXABLE(
      uint32_t*, colors, (size_t)hash_size * sizeof(*color_cache->colors));
  return 1;
}

void VP8LColorCacheClear(VP8LColorCache* const color_cache) {
  if (color_cache != NULL) {
    WebPSafeFree(color_cache->colors);
    color_cache->colors = NULL;
    WEBP_SELF_ASSIGN(color_cache->hash_bits);
  }
}

void VP8LColorCacheCopy(const VP8LColorCache* const src,
                        VP8LColorCache* const dst) {
  assert(src != NULL);
  assert(dst != NULL);
  assert(src->hash_bits == dst->hash_bits);
  WEBP_UNSAFE_MEMCPY(dst->colors, src->colors,
                     ((size_t)1u << dst->hash_bits) * sizeof(*dst->colors));
}
