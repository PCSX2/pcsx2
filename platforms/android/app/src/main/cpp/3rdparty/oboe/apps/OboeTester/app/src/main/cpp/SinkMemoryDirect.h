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

#pragma once

#include <memory>

#include "flowgraph/FlowGraphNode.h"

/**
 * AudioSink that provides data from a cached memory.
 * Data conversion is not allowed when using this sink.
 */
class SinkMemoryDirect : public oboe::flowgraph::FlowGraphSink {
public:
    explicit SinkMemoryDirect(int channelCount, int bytesPerFrame);

    void setupMemoryBuffer(std::unique_ptr<uint8_t[]>& buffer, int length);

    void reset() final;

    int32_t read(void* data, int32_t numFrames) final;

private:
    std::unique_ptr<uint8_t[]> mBuffer = nullptr;
    int mBufferLength = 0;
    int mCurPosition = 0;

    const int mBytesPerFrame;
};
