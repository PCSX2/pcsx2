/**
 * This fuzz target attempts to decompress the fuzzed data with the simple
 * decompression function to ensure the decompressor never crashes.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "fuzz_helpers.h"
#include "fuzz_data_producer.h"
#include "lz4.h"
#define LZ4F_STATIC_LINKING_ONLY
#include "lz4frame.h"
#include "lz4_helpers.h"

static void decompress(LZ4F_dctx* dctx, void* dst, size_t dstCapacity,
                       const void* src, size_t srcSize,
                       const void* dict, size_t dictSize,
                       const LZ4F_decompressOptions_t* opts)
{
    LZ4F_resetDecompressionContext(dctx);
    if (dictSize == 0)
        LZ4F_decompress(dctx, dst, &dstCapacity, src, &srcSize, opts);
    else
        LZ4F_decompress_usingDict(dctx, dst, &dstCapacity, src, &srcSize,
                                  dict, dictSize, opts);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FUZZ_dataProducer_t *producer = FUZZ_dataProducer_create(data, size);
    size_t const dstCapacitySeed = FUZZ_dataProducer_retrieve32(producer);
    size_t const dictSizeSeed = FUZZ_dataProducer_retrieve32(producer);
    size = FUZZ_dataProducer_remainingBytes(producer);

    size_t const dstCapacity = FUZZ_getRange_from_uint32(
      dstCapacitySeed, 0, 4 * size);
    size_t const largeDictSize = 64 * 1024;
    size_t const dictSize = FUZZ_getRange_from_uint32(
      dictSizeSeed, 0, largeDictSize);

    char* const dst = (char*)malloc(dstCapacity);
    char* const dict = (char*)malloc(dictSize);
    LZ4F_decompressOptions_t opts;
    LZ4F_dctx* dctx;
    LZ4F_createDecompressionContext(&dctx, LZ4F_VERSION);

    FUZZ_ASSERT(dctx);
    FUZZ_ASSERT(dst);
    FUZZ_ASSERT(dict);

    /* Prepare the dictionary. The data doesn't matter for decompression. */
    memset(dict, 0, dictSize);


    /* Decompress using multiple configurations. */
    memset(&opts, 0, sizeof(opts));
    opts.stableDst = 0;
    decompress(dctx, dst, dstCapacity, data, size, NULL, 0, &opts);
    opts.stableDst = 1;
    decompress(dctx, dst, dstCapacity, data, size, NULL, 0, &opts);
    opts.stableDst = 0;
    decompress(dctx, dst, dstCapacity, data, size, dict, dictSize, &opts);
    opts.stableDst = 1;
    decompress(dctx, dst, dstCapacity, data, size, dict, dictSize, &opts);

    LZ4F_freeDecompressionContext(dctx);
    free(dst);
    free(dict);
    FUZZ_dataProducer_free(producer);

    return 0;
}
