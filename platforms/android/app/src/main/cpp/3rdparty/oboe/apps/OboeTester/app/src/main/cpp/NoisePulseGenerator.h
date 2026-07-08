/*
 * Copyright 2025 The Android Open Source Project
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

#ifndef NATIVEOBOE_NOISEPULSEGENERATOR_H
#define NATIVEOBOE_NOISEPULSEGENERATOR_H

#include <atomic>
#include <unistd.h>
#include <sys/types.h>

#include "flowgraph/FlowGraphNode.h"
#include "flowunits/OscillatorBase.h"

class NoisePulseGenerator : public oboe::flowgraph::FlowGraphNode {
public:
    NoisePulseGenerator();

    virtual ~NoisePulseGenerator();

    int32_t onProcess(int numFrames) override;

    void trigger();

    void reset() override;

    /**
     * Control the linear amplitude.
     * Silence is 0.0.
     * A typical full amplitude would be 1.0.
     */
    oboe::flowgraph::FlowGraphPortFloatInput  amplitude;

    oboe::flowgraph::FlowGraphPortFloatOutput output;

private:
    std::atomic<int> mRequestCount; // external thread increments this to request a beep
    std::atomic<int> mAcknowledgeCount; // audio thread sets this to acknowledge
    double mLevel;
};


#endif //NATIVEOBOE_NOISEPULSEGENERATOR_H
