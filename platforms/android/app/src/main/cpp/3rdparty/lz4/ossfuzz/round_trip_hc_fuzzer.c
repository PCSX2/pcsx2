/**
 * This fuzz target performs a lz4 round-trip test (compress & decompress),
 * compares the result with the original, and calls abort() on corruption.
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
    int const level = FUZZ_dataProducer_range32(producer,
        LZ4HC_CLEVEL_MIN, LZ4HC_CLEVEL_MAX);
    size = FUZZ_dataProducer_remainingBytes(producer);

    size_t const dstCapacity = LZ4_compressBound(size);
    char* const dst = (char*)malloc(dstCapacity);
    char* const rt = (char*)malloc(size);

    FUZZ_ASSERT(dst);
    FUZZ_ASSERT(rt);

    /* Compression must succeed and round trip correctly. */
    int const dstSize = LZ4_compress_HC((const char*)data, dst, size,
                                        dstCapacity, level);
    FUZZ_ASSERT(dstSize > 0);

    int const rtSize = LZ4_decompress_safe(dst, rt, dstSize, size);
    FUZZ_ASSERT_MSG(rtSize == size, "Incorrect size");
    FUZZ_ASSERT_MSG(!memcmp(data, rt, size), "Corruption!");

    free(dst);
    free(rt);
    FUZZ_dataProducer_free(producer);

    return 0;
}
