/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include "SampleBuffer.h"

// Resampler Includes
#include <resampler/MultiChannelResampler.h>

#include "wav/WavStreamReader.h"

using namespace RESAMPLER_OUTER_NAMESPACE::resampler;

namespace iolib {

void SampleBuffer::loadSampleData(parselib::WavStreamReader* reader) {
    mAudioProperties.channelCount = reader->getNumChannels();
    mAudioProperties.sampleRate = reader->getSampleRate();

    reader->positionToAudio();

    mNumSamples = reader->getNumSampleFrames() * reader->getNumChannels();
    mSampleData = new float[mNumSamples];

    reader->getDataFloat(mSampleData, reader->getNumSampleFrames());
}

void SampleBuffer::unloadSampleData() {
    if (mSampleData != nullptr) {
        delete[] mSampleData;
        mSampleData = nullptr;
    }
    mNumSamples = 0;
}

class ResampleBlock {
public:
    int32_t mSampleRate;
    float*  mBuffer;
    int32_t mNumSamples;
};

void resampleData(const ResampleBlock& input, ResampleBlock* output, int numChannels) {
    // Calculate output buffer size
    double temp =
            ((double)input.mNumSamples * (double)output->mSampleRate) / (double)input.mSampleRate;

    // round up
    int32_t numOutFramesAllocated = (int32_t)(temp + 0.5);
    // We iterate thousands of times through the loop. Roundoff error could accumulate
    // so add a few more frames for padding
    numOutFramesAllocated += 8;

    MultiChannelResampler *resampler = MultiChannelResampler::make(
            numChannels, // channel count
            input.mSampleRate, // input sampleRate
            output->mSampleRate, // output sampleRate
            MultiChannelResampler::Quality::Medium); // conversion quality

    float *inputBuffer = input.mBuffer;;     // multi-channel buffer to be consumed
    float *outputBuffer = new float[numOutFramesAllocated];    // multi-channel buffer to be filled
    output->mBuffer = outputBuffer;

    int numOutputSamples = 0;
    int inputSamplesLeft = input.mNumSamples;
    while ((inputSamplesLeft > 0) && (numOutputSamples < numOutFramesAllocated)) {
        if(resampler->isWriteNeeded()) {
            resampler->writeNextFrame(inputBuffer);
            inputBuffer += numChannels;
            inputSamplesLeft -= numChannels;
        } else {
            resampler->readNextFrame(outputBuffer);
            outputBuffer += numChannels;
            numOutputSamples += numChannels;
        }
    }
    output->mNumSamples = numOutputSamples;

    delete resampler;
}

void SampleBuffer::resampleData(int sampleRate) {
    if (mAudioProperties.sampleRate == sampleRate) {
        // nothing to do
        return;
    }

    ResampleBlock inputBlock;
    inputBlock.mBuffer = mSampleData;
    inputBlock.mNumSamples = mNumSamples;
    inputBlock.mSampleRate = mAudioProperties.sampleRate;

    ResampleBlock outputBlock;
    outputBlock.mSampleRate = sampleRate;
    iolib::resampleData(inputBlock, &outputBlock, mAudioProperties.channelCount);

    // delete previous samples
    delete[] mSampleData;

    // install the resampled data
    mSampleData = outputBlock.mBuffer;
    mNumSamples = outputBlock.mNumSamples;
    mAudioProperties.sampleRate = outputBlock.mSampleRate;
}

} // namespace iolib
