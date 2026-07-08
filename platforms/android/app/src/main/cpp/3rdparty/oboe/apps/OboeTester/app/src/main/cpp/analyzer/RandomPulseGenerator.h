/*
 * Copyright 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANALYZER_RANDOM_PULSE_GENERATOR_H
#define ANALYZER_RANDOM_PULSE_GENERATOR_H

#include <stdlib.h>
#include "RoundedManchesterEncoder.h"

/**
 * Encode random ones and zeros using Manchester Code per IEEE 802.3.
 */
class RandomPulseGenerator : public RoundedManchesterEncoder {
public:
    RandomPulseGenerator(int samplesPerPulse)
    : RoundedManchesterEncoder(samplesPerPulse) {
    }

    virtual ~RandomPulseGenerator() = default;

    /**
     * This will be called when the next byte is needed.
     * @return random byte
     */
    uint8_t onNextByte() override {
        return static_cast<uint8_t>(rand());
    }
};

#endif //ANALYZER_RANDOM_PULSE_GENERATOR_H
