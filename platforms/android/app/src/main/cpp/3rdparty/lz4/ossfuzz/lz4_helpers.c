#include "fuzz_helpers.h"
#include "lz4_helpers.h"
#include "lz4hc.h"

LZ4F_frameInfo_t FUZZ_randomFrameInfo(uint32_t* seed)
{
    LZ4F_frameInfo_t info = LZ4F_INIT_FRAMEINFO;
    info.blockSizeID = FUZZ_rand32(seed, LZ4F_max64KB - 1, LZ4F_max4MB);
    if (info.blockSizeID < LZ4F_max64KB) {
        info.blockSizeID = LZ4F_default;
    }
    info.blockMode = FUZZ_rand32(seed, LZ4F_blockLinked, LZ4F_blockIndependent);
    info.contentChecksumFlag = FUZZ_rand32(seed, LZ4F_noContentChecksum,
                                           LZ4F_contentChecksumEnabled);
    info.blockChecksumFlag = FUZZ_rand32(seed, LZ4F_noBlockChecksum,
                                         LZ4F_blockChecksumEnabled);
    return info;
}

LZ4F_preferences_t FUZZ_randomPreferences(uint32_t* seed)
{
    LZ4F_preferences_t prefs = LZ4F_INIT_PREFERENCES;
    prefs.frameInfo = FUZZ_randomFrameInfo(seed);
    prefs.compressionLevel = FUZZ_rand32(seed, 0, LZ4HC_CLEVEL_MAX + 3) - 3;
    prefs.autoFlush = FUZZ_rand32(seed, 0, 1);
    prefs.favorDecSpeed = FUZZ_rand32(seed, 0, 1);
    return prefs;
}

size_t FUZZ_decompressFrame(void* dst, const size_t dstCapacity,
                            const void* src, const size_t srcSize)
{
    LZ4F_decompressOptions_t opts;
    memset(&opts, 0, sizeof(opts));
    opts.stableDst = 1;
    LZ4F_dctx* dctx;
    LZ4F_createDecompressionContext(&dctx, LZ4F_VERSION);
    FUZZ_ASSERT(dctx);

    size_t dstSize = dstCapacity;
    size_t srcConsumed = srcSize;
    size_t const rc =
            LZ4F_decompress(dctx, dst, &dstSize, src, &srcConsumed, &opts);
    FUZZ_ASSERT(!LZ4F_isError(rc));
    FUZZ_ASSERT(rc == 0);
    FUZZ_ASSERT(srcConsumed == srcSize);

    LZ4F_freeDecompressionContext(dctx);

    return dstSize;
}
