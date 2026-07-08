#include "fuzz_data_producer.h"

struct FUZZ_dataProducer_s{
  const uint8_t *data;
  size_t size;
};

FUZZ_dataProducer_t* FUZZ_dataProducer_create(const uint8_t* data, size_t size) {
  FUZZ_dataProducer_t* const producer = malloc(sizeof(FUZZ_dataProducer_t));

  FUZZ_ASSERT(producer != NULL);

  producer->data = data;
  producer->size = size;
  return producer;
}

void FUZZ_dataProducer_free(FUZZ_dataProducer_t *producer) { free(producer); }

uint32_t FUZZ_dataProducer_retrieve32(FUZZ_dataProducer_t *producer) {
    const uint8_t* data = producer->data;
    const size_t size = producer->size;
    if (size == 0) {
        return 0;
    } else if (size < 4) {
        producer->size -= 1;
        return (uint32_t)data[size - 1];
    } else {
        producer->size -= 4;
        return *(data + size - 4);
    }
}

uint32_t FUZZ_getRange_from_uint32(uint32_t seed, uint32_t min, uint32_t max)
{
    uint32_t range = max - min;
    if (range == 0xffffffff) {
      return seed;
    }
    return min + seed % (range + 1);
}

uint32_t FUZZ_dataProducer_range32(FUZZ_dataProducer_t* producer,
    uint32_t min, uint32_t max)
{
    size_t const seed = FUZZ_dataProducer_retrieve32(producer);
    return FUZZ_getRange_from_uint32(seed, min, max);
}

LZ4F_frameInfo_t FUZZ_dataProducer_frameInfo(FUZZ_dataProducer_t* producer)
{
    LZ4F_frameInfo_t info = LZ4F_INIT_FRAMEINFO;
    info.blockSizeID = FUZZ_dataProducer_range32(producer, LZ4F_max64KB - 1, LZ4F_max4MB);
    if (info.blockSizeID < LZ4F_max64KB) {
        info.blockSizeID = LZ4F_default;
    }
    info.blockMode = FUZZ_dataProducer_range32(producer, LZ4F_blockLinked, LZ4F_blockIndependent);
    info.contentChecksumFlag = FUZZ_dataProducer_range32(producer, LZ4F_noContentChecksum,
                                           LZ4F_contentChecksumEnabled);
    info.blockChecksumFlag = FUZZ_dataProducer_range32(producer, LZ4F_noBlockChecksum,
                                         LZ4F_blockChecksumEnabled);
    return info;
}

LZ4F_preferences_t FUZZ_dataProducer_preferences(FUZZ_dataProducer_t* producer)
{
    LZ4F_preferences_t prefs = LZ4F_INIT_PREFERENCES;
    prefs.frameInfo = FUZZ_dataProducer_frameInfo(producer);
    prefs.compressionLevel = FUZZ_dataProducer_range32(producer, 0, LZ4HC_CLEVEL_MAX + 3) - 3;
    prefs.autoFlush = FUZZ_dataProducer_range32(producer, 0, 1);
    prefs.favorDecSpeed = FUZZ_dataProducer_range32(producer, 0, 1);
    return prefs;
}

size_t FUZZ_dataProducer_remainingBytes(FUZZ_dataProducer_t *producer){
  return producer->size;
}
