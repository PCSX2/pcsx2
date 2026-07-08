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
#include "lz4.h"
#include "lz4frame.h"
#include "lz4_helpers.h"
#include "fuzz_data_producer.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FUZZ_dataProducer_t *producer = FUZZ_dataProducer_create(data, size);
    LZ4F_preferences_t const prefs = FUZZ_dataProducer_preferences(producer);
    size_t const dstCapacitySeed = FUZZ_dataProducer_retrieve32(producer);
    size = FUZZ_dataProducer_remainingBytes(producer);

    size_t const compressBound = LZ4F_compressFrameBound(size, &prefs);
    size_t const dstCapacity = FUZZ_getRange_from_uint32(dstCapacitySeed, 0, compressBound);

    char* const dst = (char*)malloc(dstCapacity);
    char* const rt = (char*)malloc(size);

    FUZZ_ASSERT(dst!=NULL);
    FUZZ_ASSERT(rt!=NULL);

    /* If compression succeeds it must round trip correctly. */
    size_t const dstSize =
            LZ4F_compressFrame(dst, dstCapacity, data, size, &prefs);
    if (!LZ4F_isError(dstSize)) {
        size_t const rtSize = FUZZ_decompressFrame(rt, size, dst, dstSize);
        FUZZ_ASSERT_MSG(rtSize == size, "Incorrect regenerated size");
        FUZZ_ASSERT_MSG(!memcmp(data, rt, size), "Corruption!");
    }

    free(dst);
    free(rt);
    FUZZ_dataProducer_free(producer);

    return 0;
}
