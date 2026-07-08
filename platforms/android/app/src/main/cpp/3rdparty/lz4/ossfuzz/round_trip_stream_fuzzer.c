/**
 * This fuzz target performs a lz4 streaming round-trip test
 * (compress & decompress), compares the result with the original, and calls
 * abort() on corruption.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "fuzz_helpers.h"
#define LZ4_STATIC_LINKING_ONLY
#include "lz4.h"
#define LZ4_HC_STATIC_LINKING_ONLY
#include "lz4hc.h"

typedef struct {
  char const* buf;
  size_t size;
  size_t pos;
} const_cursor_t;

typedef struct {
  char* buf;
  size_t size;
  size_t pos;
} cursor_t;

typedef struct {
  LZ4_stream_t* cstream;
  LZ4_streamHC_t* cstreamHC;
  LZ4_streamDecode_t* dstream;
  const_cursor_t data;
  cursor_t compressed;
  cursor_t roundTrip;
  uint32_t seed;
  int level;
} state_t;

cursor_t cursor_create(size_t size)
{
  cursor_t cursor;
  cursor.buf = (char*)malloc(size);
  cursor.size = size;
  cursor.pos = 0;
  FUZZ_ASSERT(cursor.buf);
  return cursor;
}

typedef void (*round_trip_t)(state_t* state);

void cursor_free(cursor_t cursor)
{
    free(cursor.buf);
}

state_t state_create(char const* data, size_t size, uint32_t seed)
{
    state_t state;

    state.seed = seed;

    state.data.buf = (char const*)data;
    state.data.size = size;
    state.data.pos = 0;

    /* Extra margin because we are streaming. */
    state.compressed = cursor_create(1024 + 2 * LZ4_compressBound(size));
    state.roundTrip = cursor_create(size);

    state.cstream = LZ4_createStream();
    FUZZ_ASSERT(state.cstream);
    state.cstreamHC = LZ4_createStreamHC();
    FUZZ_ASSERT(state.cstream);
    state.dstream = LZ4_createStreamDecode();
    FUZZ_ASSERT(state.dstream);

    return state;
}

void state_free(state_t state)
{
    cursor_free(state.compressed);
    cursor_free(state.roundTrip);
    LZ4_freeStream(state.cstream);
    LZ4_freeStreamHC(state.cstreamHC);
    LZ4_freeStreamDecode(state.dstream);
}

static void state_reset(state_t* state, uint32_t seed)
{
    state->level = FUZZ_rand32(&seed, LZ4HC_CLEVEL_MIN, LZ4HC_CLEVEL_MAX);
    LZ4_resetStream_fast(state->cstream);
    LZ4_resetStreamHC_fast(state->cstreamHC, state->level);
    LZ4_setStreamDecode(state->dstream, NULL, 0);
    state->data.pos = 0;
    state->compressed.pos = 0;
    state->roundTrip.pos = 0;
    state->seed = seed;
}

static void state_decompress(state_t* state, char const* src, int srcSize)
{
    char* dst = state->roundTrip.buf + state->roundTrip.pos;
    int const dstCapacity = state->roundTrip.size - state->roundTrip.pos;
    int const dSize = LZ4_decompress_safe_continue(state->dstream, src, dst,
                                                   srcSize, dstCapacity);
    FUZZ_ASSERT(dSize >= 0);
    state->roundTrip.pos += dSize;
}

static void state_checkRoundTrip(state_t const* state)
{
    char const* data = state->data.buf;
    size_t const size = state->data.size;
    FUZZ_ASSERT_MSG(size == state->roundTrip.pos, "Incorrect size!");
    FUZZ_ASSERT_MSG(!memcmp(data, state->roundTrip.buf, size), "Corruption!");
}

/**
 * Picks a dictionary size and trims the dictionary off of the data.
 * We copy the dictionary to the roundTrip so our validation passes.
 */
static size_t state_trimDict(state_t* state)
{
    /* 64 KB is the max dict size, allow slightly beyond that to test trim. */
    uint32_t maxDictSize = MIN(70 * 1024, state->data.size);
    size_t const dictSize = FUZZ_rand32(&state->seed, 0, maxDictSize);
    DEBUGLOG(2, "dictSize = %zu", dictSize);
    FUZZ_ASSERT(state->data.pos == 0);
    FUZZ_ASSERT(state->roundTrip.pos == 0);
    memcpy(state->roundTrip.buf, state->data.buf, dictSize);
    state->data.pos += dictSize;
    state->roundTrip.pos += dictSize;
    return dictSize;
}

static void state_prefixRoundTrip(state_t* state)
{
    while (state->data.pos != state->data.size) {
        char const* src = state->data.buf + state->data.pos;
        char* dst = state->compressed.buf + state->compressed.pos;
        int const srcRemaining = state->data.size - state->data.pos;
        int const srcSize = FUZZ_rand32(&state->seed, 0, srcRemaining);
        int const dstCapacity = state->compressed.size - state->compressed.pos;
        int const cSize = LZ4_compress_fast_continue(state->cstream, src, dst,
                                                     srcSize, dstCapacity, 0);
        FUZZ_ASSERT(cSize > 0);
        state->data.pos += srcSize;
        state->compressed.pos += cSize;
        state_decompress(state, dst, cSize);
    }
}

static void state_extDictRoundTrip(state_t* state)
{
    int i = 0;
    cursor_t data2 = cursor_create(state->data.size);
    memcpy(data2.buf, state->data.buf, state->data.size);
    while (state->data.pos != state->data.size) {
        char const* data = (i++ & 1) ? state->data.buf : data2.buf;
        char const* src = data + state->data.pos;
        char* dst = state->compressed.buf + state->compressed.pos;
        int const srcRemaining = state->data.size - state->data.pos;
        int const srcSize = FUZZ_rand32(&state->seed, 0, srcRemaining);
        int const dstCapacity = state->compressed.size - state->compressed.pos;
        int const cSize = LZ4_compress_fast_continue(state->cstream, src, dst,
                                                     srcSize, dstCapacity, 0);
        FUZZ_ASSERT(cSize > 0);
        state->data.pos += srcSize;
        state->compressed.pos += cSize;
        state_decompress(state, dst, cSize);
    }
    cursor_free(data2);
}

static void state_randomRoundTrip(state_t* state, round_trip_t rt0,
                                  round_trip_t rt1)
{
    if (FUZZ_rand32(&state->seed, 0, 1)) {
      rt0(state);
    } else {
      rt1(state);
    }
}

static void state_loadDictRoundTrip(state_t* state)
{
    char const* dict = state->data.buf;
    size_t const dictSize = state_trimDict(state);
    LZ4_loadDict(state->cstream, dict, dictSize);
    LZ4_setStreamDecode(state->dstream, dict, dictSize);
    state_randomRoundTrip(state, state_prefixRoundTrip, state_extDictRoundTrip);
}

static void state_attachDictRoundTrip(state_t* state)
{
    char const* dict = state->data.buf;
    size_t const dictSize = state_trimDict(state);
    LZ4_stream_t* dictStream = LZ4_createStream();
    LZ4_loadDict(dictStream, dict, dictSize);
    LZ4_attach_dictionary(state->cstream, dictStream);
    LZ4_setStreamDecode(state->dstream, dict, dictSize);
    state_randomRoundTrip(state, state_prefixRoundTrip, state_extDictRoundTrip);
    LZ4_freeStream(dictStream);
}

static void state_prefixHCRoundTrip(state_t* state)
{
    while (state->data.pos != state->data.size) {
        char const* src = state->data.buf + state->data.pos;
        char* dst = state->compressed.buf + state->compressed.pos;
        int const srcRemaining = state->data.size - state->data.pos;
        int const srcSize = FUZZ_rand32(&state->seed, 0, srcRemaining);
        int const dstCapacity = state->compressed.size - state->compressed.pos;
        int const cSize = LZ4_compress_HC_continue(state->cstreamHC, src, dst,
                                                   srcSize, dstCapacity);
        FUZZ_ASSERT(cSize > 0);
        state->data.pos += srcSize;
        state->compressed.pos += cSize;
        state_decompress(state, dst, cSize);
    }
}

static void state_extDictHCRoundTrip(state_t* state)
{
    int i = 0;
    cursor_t data2 = cursor_create(state->data.size);
    DEBUGLOG(2, "extDictHC");
    memcpy(data2.buf, state->data.buf, state->data.size);
    while (state->data.pos != state->data.size) {
        char const* data = (i++ & 1) ? state->data.buf : data2.buf;
        char const* src = data + state->data.pos;
        char* dst = state->compressed.buf + state->compressed.pos;
        int const srcRemaining = state->data.size - state->data.pos;
        int const srcSize = FUZZ_rand32(&state->seed, 0, srcRemaining);
        int const dstCapacity = state->compressed.size - state->compressed.pos;
        int const cSize = LZ4_compress_HC_continue(state->cstreamHC, src, dst,
                                                   srcSize, dstCapacity);
        FUZZ_ASSERT(cSize > 0);
        DEBUGLOG(2, "srcSize = %d", srcSize);
        state->data.pos += srcSize;
        state->compressed.pos += cSize;
        state_decompress(state, dst, cSize);
    }
    cursor_free(data2);
}

static void state_loadDictHCRoundTrip(state_t* state)
{
    char const* dict = state->data.buf;
    size_t const dictSize = state_trimDict(state);
    LZ4_loadDictHC(state->cstreamHC, dict, dictSize);
    LZ4_setStreamDecode(state->dstream, dict, dictSize);
    state_randomRoundTrip(state, state_prefixHCRoundTrip,
                          state_extDictHCRoundTrip);
}

static void state_attachDictHCRoundTrip(state_t* state)
{
    char const* dict = state->data.buf;
    size_t const dictSize = state_trimDict(state);
    LZ4_streamHC_t* dictStream = LZ4_createStreamHC();
    LZ4_setCompressionLevel(dictStream, state->level);
    LZ4_loadDictHC(dictStream, dict, dictSize);
    LZ4_attach_HC_dictionary(state->cstreamHC, dictStream);
    LZ4_setStreamDecode(state->dstream, dict, dictSize);
    state_randomRoundTrip(state, state_prefixHCRoundTrip,
                          state_extDictHCRoundTrip);
    LZ4_freeStreamHC(dictStream);
}

round_trip_t roundTrips[] = {
  &state_prefixRoundTrip,
  &state_extDictRoundTrip,
  &state_loadDictRoundTrip,
  &state_attachDictRoundTrip,
  &state_prefixHCRoundTrip,
  &state_extDictHCRoundTrip,
  &state_loadDictHCRoundTrip,
  &state_attachDictHCRoundTrip,
};

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    uint32_t seed = FUZZ_seed(&data, &size);
    state_t state = state_create((char const*)data, size, seed);
    const int n = sizeof(roundTrips) / sizeof(round_trip_t);
    int i;

    for (i = 0; i < n; ++i) {
        DEBUGLOG(2, "Round trip %d", i);
        state_reset(&state, seed);
        roundTrips[i](&state);
        state_checkRoundTrip(&state);
    }

    state_free(state);

    return 0;
}
