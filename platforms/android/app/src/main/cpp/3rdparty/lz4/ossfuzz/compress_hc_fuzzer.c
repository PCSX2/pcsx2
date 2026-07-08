/**
 * This fuzz target attempts to compress the fuzzed data with the simple
 * compression function with an output buffer that may be too small to
 * ensure that the compressor never crashes.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "fuzz_helpers.h"
#include "fuzz_data_producer.h"
#include "lz4.h"
#include "lz4hc.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FUZZ_dataProducer_t *producer = FUZZ_dataProducer_create(data, size);
    size_t const dstCapacitySeed = FUZZ_dataProducer_retrieve32(producer);
    size_t const levelSeed = FUZZ_dataProducer_retrieve32(producer);
    size = FUZZ_dataProducer_remainingBytes(producer);

    size_t const dstCapacity = FUZZ_getRange_from_uint32(dstCapacitySeed, 0, size);
    int const level = FUZZ_getRange_from_uint32(levelSeed, LZ4HC_CLEVEL_MIN, LZ4HC_CLEVEL_MAX);

    char* const dst = (char*)malloc(dstCapacity);
    char* const rt = (char*)malloc(size);

    FUZZ_ASSERT(dst);
    FUZZ_ASSERT(rt);

    /* If compression succeeds it must round trip correctly. */
    {
        int const dstSize = LZ4_compress_HC((const char*)data, dst, size,
                                            dstCapacity, level);
        if (dstSize > 0) {
            int const rtSize = LZ4_decompress_safe(dst, rt, dstSize, size);
            FUZZ_ASSERT_MSG(rtSize == size, "Incorrect regenerated size");
            FUZZ_ASSERT_MSG(!memcmp(data, rt, size), "Corruption!");
        }
    }

    if (dstCapacity > 0) {
        /* Compression succeeds and must round trip correctly. */
        void* state = malloc(LZ4_sizeofStateHC());
        FUZZ_ASSERT(state);
        int compressedSize = size;
        int const dstSize = LZ4_compress_HC_destSize(state, (const char*)data,
                                                     dst, &compressedSize,
                                                     dstCapacity, level);
        FUZZ_ASSERT(dstSize > 0);
        int const rtSize = LZ4_decompress_safe(dst, rt, dstSize, size);
        FUZZ_ASSERT_MSG(rtSize == compressedSize, "Incorrect regenerated size");
        FUZZ_ASSERT_MSG(!memcmp(data, rt, compressedSize), "Corruption!");
        free(state);
    }

    free(dst);
    free(rt);
    FUZZ_dataProducer_free(producer);

    return 0;
}
