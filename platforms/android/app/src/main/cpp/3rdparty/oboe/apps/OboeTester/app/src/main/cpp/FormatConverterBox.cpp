/*
 * Copyright 2021 The Android Open Source Project
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

#include "FormatConverterBox.h"

FormatConverterBox::FormatConverterBox(int32_t maxSamples,
                                       oboe::AudioFormat inputFormat,
                                       oboe::AudioFormat outputFormat) {
    mInputFormat = inputFormat;
    mOutputFormat = outputFormat;

    mMaxSamples = maxSamples;
    mInputBuffer = std::make_unique<uint8_t[]>(maxSamples * sizeof(int32_t));
    mOutputBuffer = std::make_unique<uint8_t[]>(maxSamples * sizeof(int32_t));

    mSource.reset();
    switch (mInputFormat) {
        case oboe::AudioFormat::I16:
        case oboe::AudioFormat::IEC61937:
            mSource = std::make_unique<oboe::flowgraph::SourceI16>(1);
            break;
        case oboe::AudioFormat::I24:
            mSource = std::make_unique<oboe::flowgraph::SourceI24>(1);
            break;
        case oboe::AudioFormat::I32:
            mSource = std::make_unique<oboe::flowgraph::SourceI32>(1);
            break;
        case oboe::AudioFormat::Float:
        case oboe::AudioFormat::Invalid:
        case oboe::AudioFormat::Unspecified:
            mSource = std::make_unique<oboe::flowgraph::SourceFloat>(1);
            break;
        case oboe::AudioFormat::MP3:
        case oboe::AudioFormat::AAC_LC:
        case oboe::AudioFormat::AAC_HE_V1:
        case oboe::AudioFormat::AAC_HE_V2:
        case oboe::AudioFormat::AAC_ELD:
        case oboe::AudioFormat::AAC_XHE:
        case oboe::AudioFormat::OPUS:
            break;
    }

    mSink.reset();
    switch (mOutputFormat) {
        case oboe::AudioFormat::I16:
        case oboe::AudioFormat::IEC61937:
            mSink = std::make_unique<oboe::flowgraph::SinkI16>(1);
            break;
        case oboe::AudioFormat::I24:
            mSink = std::make_unique<oboe::flowgraph::SinkI24>(1);
            break;
        case oboe::AudioFormat::I32:
            mSink = std::make_unique<oboe::flowgraph::SinkI32>(1);
            break;
        case oboe::AudioFormat::Float:
        case oboe::AudioFormat::Invalid:
        case oboe::AudioFormat::Unspecified:
            mSink = std::make_unique<oboe::flowgraph::SinkFloat>(1);
            break;
        case oboe::AudioFormat::MP3:
        case oboe::AudioFormat::AAC_LC:
        case oboe::AudioFormat::AAC_HE_V1:
        case oboe::AudioFormat::AAC_HE_V2:
        case oboe::AudioFormat::AAC_ELD:
        case oboe::AudioFormat::AAC_XHE:
        case oboe::AudioFormat::OPUS:
            break;
    }

    if (mSource && mSink) {
        mSource->output.connect(&mSink->input);
        mSink->pullReset();
    }
}

int32_t FormatConverterBox::convertInternalBuffers(int32_t numSamples) {
    assert(numSamples <= mMaxSamples);
    return convert(getOutputBuffer(), numSamples, getInputBuffer());
}

int32_t FormatConverterBox::convertToInternalOutput(int32_t numSamples, const void *inputBuffer) {
    assert(numSamples <= mMaxSamples);
    return convert(getOutputBuffer(), numSamples, inputBuffer);
}

int32_t FormatConverterBox::convertFromInternalInput(void *outputBuffer, int32_t numSamples) {
    assert(numSamples <= mMaxSamples);
    return convert(outputBuffer, numSamples, getInputBuffer());
}

int32_t FormatConverterBox::convert(void *outputBuffer, int32_t numSamples, const void *inputBuffer) {
    mSource->setData(inputBuffer, numSamples);
    return mSink->read(outputBuffer, numSamples);
}
