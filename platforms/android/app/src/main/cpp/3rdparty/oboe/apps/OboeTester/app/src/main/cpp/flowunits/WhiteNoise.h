/*
 * Copyright 2022 The Android Open Source Project
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

#ifndef FLOWGRAPH_WHITE_NOISE_H
#define FLOWGRAPH_WHITE_NOISE_H

#include <unistd.h>

#include "flowgraph/FlowGraphNode.h"

#include "../analyzer/PseudoRandom.h"

/**
 * White noise with equal energy in all frequencies up to the Nyquist.
 * This is a based on random numbers with a uniform distribution.
 */
class WhiteNoise : public oboe::flowgraph::FlowGraphNode {
public:

    WhiteNoise()
            : oboe::flowgraph::FlowGraphNode()
            , amplitude(*this, 1)
            , output(*this, 1)
            {
    }

    virtual ~WhiteNoise() = default;

    int32_t onProcess(int32_t numFrames) override;

    /**
     * Control the amplitude amplitude of the noise.
     * Silence is 0.0.
     * A typical full amplitude would be 1.0.
     */
    oboe::flowgraph::FlowGraphPortFloatInput  amplitude;

    oboe::flowgraph::FlowGraphPortFloatOutput output;

private:
    PseudoRandom mPseudoRandom;
};

#endif //FLOWGRAPH_WHITE_NOISE_H
