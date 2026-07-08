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

#ifndef NATIVEOBOE_MULTICHANNEL_RECORDING_H
#define NATIVEOBOE_MULTICHANNEL_RECORDING_H

#include <algorithm>
#include <memory.h>
#include <unistd.h>
#include <sys/types.h>

/**
 * Store multi-channel audio data in float format.
 * The most recent data will be saved.
 * Old data may be overwritten.
 *
 * Note that this is not thread safe. Do not read and write from separate threads.
 */
class MultiChannelRecording {
public:
    MultiChannelRecording(int32_t channelCount, int32_t maxFrames)
            : mChannelCount(channelCount)
            , mMaxFrames(maxFrames) {
        mData = new float[channelCount * maxFrames];
    }

    ~MultiChannelRecording() {
        delete[] mData;
    }

    void rewind() {
        mReadCursorFrames = mWriteCursorFrames - getSizeInFrames();
    }

    void clear() {
        mReadCursorFrames = 0;
        mWriteCursorFrames = 0;
    }

    int32_t getChannelCount() {
        return mChannelCount;
    }

    int32_t getSizeInFrames() {
        return (int32_t) std::min(mWriteCursorFrames, static_cast<int64_t>(mMaxFrames));
    }

    int32_t getReadIndex() {
        return mReadCursorFrames % mMaxFrames;
    }
    int32_t getWriteIndex() {
        return mWriteCursorFrames % mMaxFrames;
    }

    /**
     * Write numFrames from the short buffer into the recording.
     * Overwrite old data if necessary.
     * Convert shorts to floats.
     *
     * @param buffer
     * @param numFrames
     * @return number of frames actually written.
     */
    int32_t write(int16_t *buffer, int32_t numFrames) {
        int32_t framesLeft = numFrames;
        while (framesLeft > 0) {
            int32_t indexFrame = getWriteIndex();
            // contiguous writes
            int32_t framesToEndOfBuffer = mMaxFrames - indexFrame;
            int32_t framesNow = std::min(framesLeft, framesToEndOfBuffer);
            int32_t numSamples = framesNow * mChannelCount;
            int32_t sampleIndex = indexFrame * mChannelCount;

            for (int i = 0; i < numSamples; i++) {
                mData[sampleIndex++] = *buffer++ * (1.0f / 32768);
            }

            mWriteCursorFrames += framesNow;
            framesLeft -= framesNow;
        }
        return numFrames - framesLeft;
    }

    /**
     * Write all numFrames from the float buffer into the recording.
     * Overwrite old data if full.
     * @param buffer
     * @param numFrames
     * @return number of frames actually written.
     */
    int32_t write(float *buffer, int32_t numFrames) {
        int32_t framesLeft = numFrames;
        while (framesLeft > 0) {
            int32_t indexFrame = getWriteIndex();
            // contiguous writes
            int32_t framesToEnd = mMaxFrames - indexFrame;
            int32_t framesNow = std::min(framesLeft, framesToEnd);
            int32_t numSamples = framesNow * mChannelCount;
            int32_t sampleIndex = indexFrame * mChannelCount;

            memcpy(&mData[sampleIndex],
                   buffer,
                   (numSamples * sizeof(float)));
            buffer += numSamples;
            mWriteCursorFrames += framesNow;
            framesLeft -= framesNow;
        }
        return numFrames;
    }

    /**
     * Read numFrames from the recording into the buffer, if there is enough data.
     * Start at the cursor position, aligned up to the next frame.
     * @param buffer
     * @param numFrames
     * @return number of frames actually read.
     */
    int32_t read(float *buffer, int32_t numFrames) {
        int32_t framesRead = 0;
        int32_t framesLeft = std::min(numFrames,
                std::min(mMaxFrames, (int32_t)(mWriteCursorFrames - mReadCursorFrames)));
        while (framesLeft > 0) {
            int32_t indexFrame = getReadIndex();
            // contiguous reads
            int32_t framesToEnd = mMaxFrames - indexFrame;
            int32_t framesNow = std::min(framesLeft, framesToEnd);
            int32_t numSamples = framesNow * mChannelCount;
            int32_t sampleIndex = indexFrame * mChannelCount;

            memcpy(buffer,
                   &mData[sampleIndex],
                   (numSamples * sizeof(float)));

            mReadCursorFrames += framesNow;
            framesLeft -= framesNow;
            framesRead += framesNow;
        }
        return framesRead;
    }

private:
    float          *mData = nullptr;
    int64_t         mReadCursorFrames = 0;
    int64_t         mWriteCursorFrames = 0; // monotonically increasing
    const int32_t   mChannelCount;
    const int32_t   mMaxFrames;
};

#endif //NATIVEOBOE_MULTICHANNEL_RECORDING_H
