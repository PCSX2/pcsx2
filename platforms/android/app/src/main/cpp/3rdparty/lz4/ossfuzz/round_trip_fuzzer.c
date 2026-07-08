/**
 * This fuzz target performs a lz4 round-trip test (compress & decompress),
 * compares the result with the original, and calls abort() on corruption.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "fuzz_helpers.h"
#include "lz4.h"
#include "fuzz_data_producer.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FUZZ_dataProducer_t *producer = FUZZ_dataProducer_create(data, size);
    size_t const partialCapacitySeed = FUZZ_dataProducer_retrieve32(producer);
    size = FUZZ_dataProducer_remainingBytes(producer);

    size_t const partialCapacity = FUZZ_getRange_from_uint32(partialCapacitySeed, 0, size);
    size_t const dstCapacity = LZ4_compressBound(size);
    size_t const largeSize = 64 * 1024 - 1;
    size_t const smallSize = 1024;
    char* const dstPlusLargePrefix = (char*)malloc(dstCapacity + largeSize);
    FUZZ_ASSERT(dstPlusLargePrefix);
    char* const dstPlusSmallPrefix = dstPlusLargePrefix + largeSize - smallSize;
    char* const largeDict = (char*)malloc(largeSize);
    FUZZ_ASSERT(largeDict);
    char* const smallDict = largeDict + largeSize - smallSize;
    char* const dst = dstPlusLargePrefix + largeSize;
    char* const rt = (char*)malloc(size);
    FUZZ_ASSERT(rt);

    /* Compression must succeed and round trip correctly. */
    int const dstSize = LZ4_compress_default((const char*)data, dst,
                                             size, dstCapacity);
    FUZZ_ASSERT(dstSize > 0);

    int const rtSize = LZ4_decompress_safe(dst, rt, dstSize, size);
    FUZZ_ASSERT_MSG(rtSize == size, "Incorrect size");
    FUZZ_ASSERT_MSG(!memcmp(data, rt, size), "Corruption!");

    /* Partial decompression must succeed. */
    {
        char* const partial = (char*)malloc(partialCapacity);
        FUZZ_ASSERT(partial);
        int const partialSize = LZ4_decompress_safe_partial(
                dst, partial, dstSize, partialCapacity, partialCapacity);
        FUZZ_ASSERT(partialSize >= 0);
        FUZZ_ASSERT_MSG(partialSize == partialCapacity, "Incorrect size");
        FUZZ_ASSERT_MSG(!memcmp(data, partial, partialSize), "Corruption!");
        free(partial);
    }
    /* Partial decompression using dict with no dict. */
    {
        char* const partial = (char*)malloc(partialCapacity);
        FUZZ_ASSERT(partial);
        int const partialSize = LZ4_decompress_safe_partial_usingDict(
                dst, partial, dstSize, partialCapacity, partialCapacity, NULL, 0);
        FUZZ_ASSERT(partialSize >= 0);
        FUZZ_ASSERT_MSG(partialSize == partialCapacity, "Incorrect size");
        FUZZ_ASSERT_MSG(!memcmp(data, partial, partialSize), "Corruption!");
        free(partial);
    }
    /* Partial decompression using dict with small prefix as dict */
    {
        char* const partial = (char*)malloc(partialCapacity);
        FUZZ_ASSERT(partial);
        int const partialSize = LZ4_decompress_safe_partial_usingDict(
                dst, partial, dstSize, partialCapacity, partialCapacity, dstPlusSmallPrefix, smallSize);
        FUZZ_ASSERT(partialSize >= 0);
        FUZZ_ASSERT_MSG(partialSize == partialCapacity, "Incorrect size");
        FUZZ_ASSERT_MSG(!memcmp(data, partial, partialSize), "Corruption!");
        free(partial);
    }
    /* Partial decompression using dict with large prefix as dict */
    {
        char* const partial = (char*)malloc(partialCapacity);
        FUZZ_ASSERT(partial);
        int const partialSize = LZ4_decompress_safe_partial_usingDict(
                dst, partial, dstSize, partialCapacity, partialCapacity, dstPlusLargePrefix, largeSize);
        FUZZ_ASSERT(partialSize >= 0);
        FUZZ_ASSERT_MSG(partialSize == partialCapacity, "Incorrect size");
        FUZZ_ASSERT_MSG(!memcmp(data, partial, partialSize), "Corruption!");
        free(partial);
    }
    /* Partial decompression using dict with small external dict */
    {
        char* const partial = (char*)malloc(partialCapacity);
        FUZZ_ASSERT(partial);
        int const partialSize = LZ4_decompress_safe_partial_usingDict(
                dst, partial, dstSize, partialCapacity, partialCapacity, smallDict, smallSize);
        FUZZ_ASSERT(partialSize >= 0);
        FUZZ_ASSERT_MSG(partialSize == partialCapacity, "Incorrect size");
        FUZZ_ASSERT_MSG(!memcmp(data, partial, partialSize), "Corruption!");
        free(partial);
    }
    /* Partial decompression using dict with large external dict */
    {
        char* const partial = (char*)malloc(partialCapacity);
        FUZZ_ASSERT(partial);
        int const partialSize = LZ4_decompress_safe_partial_usingDict(
                dst, partial, dstSize, partialCapacity, partialCapacity, largeDict, largeSize);
        FUZZ_ASSERT(partialSize >= 0);
        FUZZ_ASSERT_MSG(partialSize == partialCapacity, "Incorrect size");
        FUZZ_ASSERT_MSG(!memcmp(data, partial, partialSize), "Corruption!");
        free(partial);
    }

    free(dstPlusLargePrefix);
    free(largeDict);
    free(rt);
    FUZZ_dataProducer_free(producer);

    return 0;
}
