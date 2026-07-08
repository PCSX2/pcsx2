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

/*
 * Test FlowGraph
 */

#include "math.h"
#include "stdio.h"

#include <gtest/gtest.h>
#include <oboe/Oboe.h>

#include "flowgraph/resampler/MultiChannelResampler.h"

using namespace oboe::resampler;

// Measure zero crossings.
static int32_t countZeroCrossingsWithHysteresis(float *input, int32_t numSamples) {
    const float kHysteresisLevel = 0.25f;
    int zeroCrossingCount = 0;
    int state = 0; // can be -1, 0, +1
    for (int i = 0; i < numSamples; i++) {
        if (input[i] >= kHysteresisLevel) {
            if (state < 0) {
                zeroCrossingCount++;
            }
            state = 1;
        } else if (input[i] <= -kHysteresisLevel) {
            if (state > 0) {
                zeroCrossingCount++;
            }
            state = -1;
        }
    }
    return zeroCrossingCount;
}

static constexpr int kChannelCount = 1;

/**
 * Convert a sine wave and then look for glitches.
 * Glitches have a high value in the second derivative.
 */
static void checkResampler(int32_t sourceRate, int32_t sinkRate,
        MultiChannelResampler::Quality quality) {
    const int kNumOutputSamples = 10000;
    const double framesPerCycle = 81.379; // target output period

    int numInputSamples = kNumOutputSamples * sourceRate / sinkRate;

    std::unique_ptr<float[]>  inputBuffer = std::make_unique<float[]>(numInputSamples);
    std::unique_ptr<float[]>  outputBuffer = std::make_unique<float[]>(kNumOutputSamples);

    // Generate a sine wave for input.
    const double kPhaseIncrement = 2.0 * sinkRate / (framesPerCycle * sourceRate);
    double phase = 0.0;
    for (int i = 0; i < numInputSamples; i++) {
        inputBuffer[i] = sin(phase * M_PI);
        phase += kPhaseIncrement;
        while (phase > 1.0) {
            phase -= 2.0;
        }
    }
    int sourceZeroCrossingCount = countZeroCrossingsWithHysteresis(inputBuffer.get(), numInputSamples);

    // Use a MultiChannelResampler to convert from the sourceRate to the sinkRate.
    std::unique_ptr<MultiChannelResampler>  mcResampler;
    mcResampler.reset(MultiChannelResampler::make(kChannelCount,
                                                 sourceRate,
                                                 sinkRate,
                                                 quality));
    int inputFramesLeft = numInputSamples;
    int numRead = 0;
    float *input = inputBuffer.get(); // for iteration
    float *output = outputBuffer.get();
    while (inputFramesLeft > 0) {
        if (mcResampler->isWriteNeeded()) {
            mcResampler->writeNextFrame(input);
            input++;
            inputFramesLeft--;
        } else {
            mcResampler->readNextFrame(output);
            output++;
            numRead++;
        }
    }

    // Flush out remaining frames from the flowgraph
    while (!mcResampler->isWriteNeeded()) {
        mcResampler->readNextFrame(output);
        output++;
        numRead++;
    }

    ASSERT_LE(numRead, kNumOutputSamples);
    // Some frames are lost priming the FIR filter.
    const int kMaxAlgorithmicFrameLoss = 5;
    EXPECT_GT(numRead, kNumOutputSamples - kMaxAlgorithmicFrameLoss);

    int sinkZeroCrossingCount = countZeroCrossingsWithHysteresis(outputBuffer.get(), numRead);
    // The sine wave may be cut off partially. This may cause multiple crossing
    // differences when upsampling.
    const int kMaxZeroCrossingDelta = std::max(sinkRate / sourceRate / 2, 1);
    EXPECT_LE(abs(sourceZeroCrossingCount - sinkZeroCrossingCount), kMaxZeroCrossingDelta);

    // Detect glitches by looking for spikes in the second derivative.
    output = outputBuffer.get();
    float previousValue = output[0];
    float previousSlope = output[1] - output[0];
    for (int i = 0; i < numRead; i++) {
        float slope = output[i] - previousValue;
        float slopeDelta = fabs(slope - previousSlope);
        // Skip a few samples because there are often some steep slope changes at the beginning.
        if (i > 10) {
            EXPECT_LT(slopeDelta, 0.1);
        }
        previousValue = output[i];
        previousSlope = slope;
    }

#if 0
    // Save to disk for inspection.
    FILE *fp = fopen( "/sdcard/Download/src_float_out.raw" , "wb" );
    fwrite(outputBuffer.get(), sizeof(float), numRead, fp );
    fclose(fp);
#endif
}


TEST(test_resampler, resampler_scan_all) {
    const int rates[] = {8000, 11025, 22050, 32000, 44100, 48000, 64000, 88200, 96000};
    const MultiChannelResampler::Quality qualities[] =
    {
        MultiChannelResampler::Quality::Fastest,
        MultiChannelResampler::Quality::Low,
        MultiChannelResampler::Quality::Medium,
        MultiChannelResampler::Quality::High,
        MultiChannelResampler::Quality::Best
    };
    for (int srcRate : rates) {
        for (int destRate : rates) {
            for (auto quality : qualities) {
                if (srcRate != destRate) {
                    checkResampler(srcRate, destRate, quality);
                }
            }
        }
    }
}

TEST(test_resampler, resampler_8000_11025_best) {
    checkResampler(8000, 11025, MultiChannelResampler::Quality::Best);
}
TEST(test_resampler, resampler_8000_48000_best) {
    checkResampler(8000, 48000, MultiChannelResampler::Quality::Best);
}

TEST(test_resampler, resampler_8000_44100_best) {
    checkResampler(8000, 44100, MultiChannelResampler::Quality::Best);
}

TEST(test_resampler, resampler_11025_24000_best) {
    checkResampler(11025, 24000, MultiChannelResampler::Quality::Best);
}

TEST(test_resampler, resampler_11025_48000_fastest) {
    checkResampler(11025, 48000, MultiChannelResampler::Quality::Fastest);
}
TEST(test_resampler, resampler_11025_48000_low) {
    checkResampler(11025, 48000, MultiChannelResampler::Quality::Low);
}
TEST(test_resampler, resampler_11025_48000_medium) {
    checkResampler(11025, 48000, MultiChannelResampler::Quality::Medium);
}
TEST(test_resampler, resampler_11025_48000_high) {
    checkResampler(11025, 48000, MultiChannelResampler::Quality::High);
}

TEST(test_resampler, resampler_11025_48000_best) {
    checkResampler(11025, 48000, MultiChannelResampler::Quality::Best);
}

TEST(test_resampler, resampler_11025_44100_best) {
    checkResampler(11025, 44100, MultiChannelResampler::Quality::Best);
}

TEST(test_resampler, resampler_11025_88200_best) {
   checkResampler(11025, 88200, MultiChannelResampler::Quality::Best);
}

TEST(test_resampler, resampler_16000_48000_best) {
    checkResampler(16000, 48000, MultiChannelResampler::Quality::Best);
}

TEST(test_resampler, resampler_44100_48000_low) {
    checkResampler(44100, 48000, MultiChannelResampler::Quality::Low);
}
TEST(test_resampler, resampler_44100_48000_best) {
    checkResampler(44100, 48000, MultiChannelResampler::Quality::Best);
}

// Look for glitches when downsampling.
TEST(test_resampler, resampler_48000_11025_best) {
    checkResampler(48000, 11025, MultiChannelResampler::Quality::Best);
}
TEST(test_resampler, resampler_48000_44100_best) {
    checkResampler(48000, 44100, MultiChannelResampler::Quality::Best);
}
TEST(test_resampler, resampler_44100_11025_best) {
    checkResampler(44100, 11025, MultiChannelResampler::Quality::Best);
}
