#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "fuzz_helpers.h"
#include "lz4frame.h"
#include "lz4hc.h"

/* Struct used for maintaining the state of the data */
typedef struct FUZZ_dataProducer_s FUZZ_dataProducer_t;

/* Returns a data producer state struct. Use for producer initialization. */
FUZZ_dataProducer_t *FUZZ_dataProducer_create(const uint8_t *data, size_t size);

/* Frees the data producer */
void FUZZ_dataProducer_free(FUZZ_dataProducer_t *producer);

/* Returns 32 bits from the end of data */
uint32_t FUZZ_dataProducer_retrieve32(FUZZ_dataProducer_t *producer);

/* Returns value between [min, max] */
uint32_t FUZZ_getRange_from_uint32(uint32_t seed, uint32_t min, uint32_t max);

/* Combination of above two functions for non adaptive use cases. ie where size is not involved */
uint32_t FUZZ_dataProducer_range32(FUZZ_dataProducer_t *producer, uint32_t min,
                                  uint32_t max);

/* Returns lz4 preferences */
LZ4F_preferences_t FUZZ_dataProducer_preferences(FUZZ_dataProducer_t* producer);

/* Returns lz4 frame info */
LZ4F_frameInfo_t FUZZ_dataProducer_frameInfo(FUZZ_dataProducer_t* producer);

/* Returns the size of the remaining bytes of data in the producer */
size_t FUZZ_dataProducer_remainingBytes(FUZZ_dataProducer_t *producer);
