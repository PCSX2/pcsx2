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

#ifndef NATIVEOBOE_SAWPINGGENERATOR_H
#define NATIVEOBOE_SAWPINGGENERATOR_H

#include <atomic>
#include <unistd.h>
#include <sys/types.h>

#include "flowgraph/FlowGraphNode.h"
#include "flowunits/OscillatorBase.h"

class SawPingGenerator : public OscillatorBase {
public:
    SawPingGenerator();

    virtual ~SawPingGenerator();

    int32_t onProcess(int numFrames) override;

    void trigger();

    void reset() override;

private:
    std::atomic<int> mRequestCount; // external thread increments this to request a beep
    std::atomic<int> mAcknowledgeCount; // audio thread sets this to acknowledge
    double mLevel;
};


#endif //NATIVEOBOE_SAWPINGGENERATOR_H
