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

#include <gtest/gtest.h>
#include <oboe/Oboe.h>

#include <tuple>

using namespace oboe;

class FramesProcessedCallback : public AudioStreamDataCallback {
public:
    DataCallbackResult onAudioReady(AudioStream *oboeStream, void *audioData, int32_t numFrames) override {
        return DataCallbackResult::Continue;
    }
};

using StreamFramesProcessedParams = std::tuple<Direction, int32_t, bool>;

class StreamFramesProcessed : public ::testing::Test,
                              public ::testing::WithParamInterface<StreamFramesProcessedParams> {

protected:
    void TearDown() override;

    static constexpr int PROCESS_TIME_SECONDS = 5;

    AudioStreamBuilder mBuilder;
    std::shared_ptr<AudioStream> mStream;
};

void StreamFramesProcessed::TearDown() {
    if (mStream) {
        mStream->close();
    }
}

TEST_P(StreamFramesProcessed, VerifyFramesProcessed) {
    const Direction direction = std::get<0>(GetParam());
    const int32_t sampleRate = std::get<1>(GetParam());
    const bool useOboeSampleRateConversion = std::get<2>(GetParam());

    SampleRateConversionQuality srcQuality = useOboeSampleRateConversion ?
            SampleRateConversionQuality::Medium : SampleRateConversionQuality::None;

    AudioStreamDataCallback *callback = new FramesProcessedCallback();
    mBuilder.setDirection(direction)
            ->setFormat(AudioFormat::I16)
            ->setSampleRate(sampleRate)
            ->setSampleRateConversionQuality(srcQuality)
            ->setPerformanceMode(PerformanceMode::LowLatency)
            ->setSharingMode(SharingMode::Exclusive)
            ->setDataCallback(callback);
    Result r = mBuilder.openStream(mStream);
    ASSERT_EQ(r, Result::OK) << "Failed to open stream." << convertToText(r);

    r = mStream->start();
    ASSERT_EQ(r, Result::OK) << "Failed to start stream." << convertToText(r);
    sleep(PROCESS_TIME_SECONDS);

    // The frames written should be close to sampleRate * PROCESS_TIME_SECONDS
    const int kDeltaFramesWindowInFrames = 30000;
    const int64_t framesWritten = mStream->getFramesWritten();
    const int64_t framesRead = mStream->getFramesRead();
    EXPECT_NEAR(framesWritten, sampleRate * PROCESS_TIME_SECONDS, kDeltaFramesWindowInFrames);
    EXPECT_NEAR(framesRead, sampleRate * PROCESS_TIME_SECONDS, kDeltaFramesWindowInFrames);
}

INSTANTIATE_TEST_CASE_P(
        StreamFramesProcessedTest,
        StreamFramesProcessed,
        ::testing::Values(
                StreamFramesProcessedParams({Direction::Output, 8000, true}),
                StreamFramesProcessedParams({Direction::Output, 44100, true}),
                StreamFramesProcessedParams({Direction::Output, 96000, true}),
                StreamFramesProcessedParams({Direction::Input, 8000, true}),
                StreamFramesProcessedParams({Direction::Input, 44100, true}),
                StreamFramesProcessedParams({Direction::Input, 96000, true}),
                StreamFramesProcessedParams({Direction::Output, 8000, false}),
                StreamFramesProcessedParams({Direction::Output, 44100, false}),
                StreamFramesProcessedParams({Direction::Output, 96000, false}),
                StreamFramesProcessedParams({Direction::Input, 8000, false}),
                StreamFramesProcessedParams({Direction::Input, 44100, false}),
                StreamFramesProcessedParams({Direction::Input, 96000, false})
                )
        );
