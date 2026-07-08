/*
 * Copyright 2018 The Android Open Source Project
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

#ifndef NATIVEOBOE_IMPULSE_GENERATOR_H
#define NATIVEOBOE_IMPULSE_GENERATOR_H

#include <unistd.h>
#include <sys/types.h>

#include "flowgraph/FlowGraphNode.h"
#include "OscillatorBase.h"

/**
 * Generate a raw impulse equal to the amplitude.
 * The output baseline is zero.
 *
 * The waveform is not band-limited so it will have aliasing artifacts at higher frequencies.
 */
class ImpulseOscillator : public OscillatorBase {
public:
    ImpulseOscillator();

    int32_t onProcess(int32_t numFrames) override;
};

#endif //NATIVEOBOE_IMPULSE_GENERATOR_H
