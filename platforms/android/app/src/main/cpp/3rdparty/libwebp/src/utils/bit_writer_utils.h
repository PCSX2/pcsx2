// Copyright 2011 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// Bit writing and boolean coder
//
// Author: Skal (pascal.massimino@gmail.com)

#ifndef WEBP_UTILS_BIT_WRITER_UTILS_H_
#define WEBP_UTILS_BIT_WRITER_UTILS_H_

#include <stddef.h>

#include "src/dsp/cpu.h"
#include "src/utils/bounds_safety.h"
#include "src/webp/types.h"

WEBP_ASSUME_UNSAFE_INDEXABLE_ABI

#ifdef __cplusplus
extern "C" {
#endif

//------------------------------------------------------------------------------
// Bit-writing

typedef struct VP8BitWriter VP8BitWriter;
struct VP8BitWriter {
  int32_t range;  // range-1
  int32_t value;
  int run;      // number of outstanding bits
  int nb_bits;  // number of pending bits
  // internal buffer. Re-allocated regularly. Not owned.
  uint8_t* WEBP_SIZED_BY_OR_NULL(max_pos) buf;
  size_t pos;
  size_t max_pos;
  int error;  // true in case of error
};

// Initialize the object. Allocates some initial memory based on expected_size.
int VP8BitWriterInit(VP8BitWriter* const bw, size_t expected_size);
// Finalize the bitstream coding. Returns a pointer to the internal buffer.
uint8_t* VP8BitWriterFinish(VP8BitWriter* const bw);
// Release any pending memory and zeroes the object. Not a mandatory call.
// Only useful in case of error, when the internal buffer hasn't been grabbed!
void VP8BitWriterWipeOut(VP8BitWriter* const bw);

int VP8PutBit(VP8BitWriter* const bw, int bit, int prob);
int VP8PutBitUniform(VP8BitWriter* const bw, int bit);
void VP8PutBits(VP8BitWriter* const bw, uint32_t value, int nb_bits);
void VP8PutSignedBits(VP8BitWriter* const bw, int value, int nb_bits);

// Appends some bytes to the internal buffer. Data is copied.
int VP8BitWriterAppend(VP8BitWriter* const bw, const uint8_t* data,
                       size_t size);

// return approximate write position (in bits)
static WEBP_INLINE uint64_t VP8BitWriterPos(const VP8BitWriter* const bw) {
  const uint64_t nb_bits = 8 + bw->nb_bits;  // bw->nb_bits is <= 0, note
  return (bw->pos + bw->run) * 8 + nb_bits;
}

// Returns a pointer to the internal buffer.
static WEBP_INLINE uint8_t* VP8BitWriterBuf(const VP8BitWriter* const bw) {
  return bw->buf;
}
// Returns the size of the internal buffer.
static WEBP_INLINE size_t VP8BitWriterSize(const VP8BitWriter* const bw) {
  return bw->pos;
}

//------------------------------------------------------------------------------
// VP8LBitWriter

// 64bit
#if defined(__x86_64__) || defined(_M_X64) || WEBP_AARCH64 || defined(__wasm__)
typedef uint64_t vp8l_atype_t;  // accumulator type
typedef uint32_t vp8l_wtype_t;  // writing type
#define WSWAP HToLE32
#define VP8L_WRITER_BYTES 4      // sizeof(vp8l_wtype_t)
#define VP8L_WRITER_BITS 32      // 8 * sizeof(vp8l_wtype_t)
#define VP8L_WRITER_MAX_BITS 64  // 8 * sizeof(vp8l_atype_t)
#else
typedef uint32_t vp8l_atype_t;
typedef uint16_t vp8l_wtype_t;
#define WSWAP HToLE16
#define VP8L_WRITER_BYTES 2
#define VP8L_WRITER_BITS 16
#define VP8L_WRITER_MAX_BITS 32
#endif

typedef struct {
  vp8l_atype_t bits;                   // bit accumulator
  int used;                            // number of bits used in accumulator
  uint8_t* WEBP_ENDED_BY(end) buf;     // start of buffer
  uint8_t* WEBP_UNSAFE_INDEXABLE cur;  // current write position
  uint8_t* end;                        // end of buffer

  // After all bits are written (VP8LBitWriterFinish()), the caller must observe
  // the state of 'error'. A value of 1 indicates that a memory allocation
  // failure has happened during bit writing. A value of 0 indicates successful
  // writing of bits.
  int error;
} VP8LBitWriter;

static WEBP_INLINE size_t VP8LBitWriterNumBytes(const VP8LBitWriter* const bw) {
  return (bw->cur - bw->buf) + ((bw->used + 7) >> 3);
}

// Returns false in case of memory allocation error.
int VP8LBitWriterInit(VP8LBitWriter* const bw, size_t expected_size);
// Returns false in case of memory allocation error.
int VP8LBitWriterClone(const VP8LBitWriter* const src,
                       VP8LBitWriter* const dst);
// Finalize the bitstream coding. Returns a pointer to the internal buffer.
uint8_t* VP8LBitWriterFinish(VP8LBitWriter* const bw);
// Release any pending memory and zeroes the object.
void VP8LBitWriterWipeOut(VP8LBitWriter* const bw);
// Resets the cursor of the BitWriter bw to when it was like in bw_init.
void VP8LBitWriterReset(const VP8LBitWriter* const bw_init,
                        VP8LBitWriter* const bw);
// Swaps the memory held by two BitWriters.
void VP8LBitWriterSwap(VP8LBitWriter* const src, VP8LBitWriter* const dst);

// Internal function for VP8LPutBits flushing VP8L_WRITER_BITS bits from the
// written state.
void VP8LPutBitsFlushBits(VP8LBitWriter* const bw, int* used,
                          vp8l_atype_t* bits);

#if VP8L_WRITER_BITS == 16
// PutBits internal function used in the 16 bit vp8l_wtype_t case.
void VP8LPutBitsInternal(VP8LBitWriter* const bw, uint32_t bits, int n_bits);
#endif

// This function writes bits into bytes in increasing addresses (little endian),
// and within a byte least-significant-bit first.
// This function can write up to VP8L_WRITER_MAX_BITS bits in one go, but
// VP8LBitReader can only read 24 bits max (VP8L_MAX_NUM_BIT_READ).
// VP8LBitWriter's 'error' flag is set in case of memory allocation error.
static WEBP_INLINE void VP8LPutBits(VP8LBitWriter* const bw, uint32_t bits,
                                    int n_bits) {
#if VP8L_WRITER_BYTES == 4
  if (n_bits == 0) return;
  if (bw->used >= VP8L_WRITER_BITS) {
    VP8LPutBitsFlushBits(bw, &bw->used, &bw->bits);
  }
  bw->bits |= (vp8l_atype_t)bits << bw->used;
  bw->used += n_bits;
#else
  VP8LPutBitsInternal(bw, bits, n_bits);
#endif
}

//------------------------------------------------------------------------------

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // WEBP_UTILS_BIT_WRITER_UTILS_H_
