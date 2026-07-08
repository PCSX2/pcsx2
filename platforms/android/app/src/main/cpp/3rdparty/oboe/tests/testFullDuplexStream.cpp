/*
 * Copyright 2023 The Android Open Source Project
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

#include <thread>

#include <gtest/gtest.h>

#include <oboe/Oboe.h>

using namespace oboe;

static constexpr int kTimeToSleepMicros = 5 * 1000 * 1000; // 5 s

using TestFullDuplexStreamParams = std::tuple<AudioApi, PerformanceMode, AudioApi, PerformanceMode>;

class TestFullDuplexStream : public ::testing::Test,
                         public ::testing::WithParamInterface<TestFullDuplexStreamParams>,
                         public FullDuplexStream {
public:
    DataCallbackResult onBothStreamsReady(
            const void *inputData,
            int numInputFrames,
            void *outputData,
            int numOutputFrames) override {
        mCallbackCount++;
        if (numInputFrames == numOutputFrames) {
            mGoodCallbackCount++;
        }
        return DataCallbackResult::Continue;
    }

protected:

    void openStream(AudioApi inputAudioApi, PerformanceMode inputPerfMode,
            AudioApi outputAudioApi, PerformanceMode outputPerfMode) {
        mOutputBuilder.setDirection(Direction::Output);
        if (mOutputBuilder.isAAudioRecommended()) {
            mOutputBuilder.setAudioApi(outputAudioApi);
        }
        mOutputBuilder.setPerformanceMode(outputPerfMode);
        mOutputBuilder.setChannelCount(1);
        mOutputBuilder.setFormat(AudioFormat::Float);
        mOutputBuilder.setDataCallback(this);

        Result r = mOutputBuilder.openStream(mOutputStream);
        ASSERT_EQ(r, Result::OK) << "Failed to open output stream " << convertToText(r);

        mInputBuilder.setDirection(Direction::Input);
        if (mInputBuilder.isAAudioRecommended()) {
            mInputBuilder.setAudioApi(inputAudioApi);
        }
        mInputBuilder.setPerformanceMode(inputPerfMode);
        mInputBuilder.setChannelCount(1);
        mInputBuilder.setFormat(AudioFormat::Float);
        mInputBuilder.setBufferCapacityInFrames(mOutputStream->getBufferCapacityInFrames() * 2);
        mInputBuilder.setSampleRate(mOutputStream->getSampleRate());

        r = mInputBuilder.openStream(mInputStream);
        ASSERT_EQ(r, Result::OK) << "Failed to open input stream " << convertToText(r);

        setSharedInputStream(mInputStream);
        setSharedOutputStream(mOutputStream);
    }

    void startStream() {
        Result r = start();
        ASSERT_EQ(r, Result::OK) << "Failed to start streams " << convertToText(r);
    }

    void stopStream() {
        Result r = stop();
        ASSERT_EQ(r, Result::OK) << "Failed to stop streams " << convertToText(r);
    }

    void closeStream() {
        Result r = mOutputStream->close();
        ASSERT_EQ(r, Result::OK) << "Failed to close output stream " << convertToText(r);
        r = mInputStream->close();
        ASSERT_EQ(r, Result::OK) << "Failed to close input stream " << convertToText(r);
    }

    void checkXRuns() {
        // Expect few xRuns with the use of full duplex stream
        EXPECT_LT(mInputStream->getXRunCount().value(), 10);
        EXPECT_LT(mOutputStream->getXRunCount().value(), 10);
    }

    void checkInputAndOutputBufferSizesMatch() {
        // Expect the large majority of callbacks to have the same sized input and output
        EXPECT_GE(mGoodCallbackCount, mCallbackCount * 4 / 5);
    }

    AudioStreamBuilder mInputBuilder;
    AudioStreamBuilder mOutputBuilder;
    std::shared_ptr<AudioStream> mInputStream;
    std::shared_ptr<AudioStream> mOutputStream;
    std::atomic<int32_t> mCallbackCount{0};
    std::atomic<int32_t> mGoodCallbackCount{0};
};

TEST_P(TestFullDuplexStream, VerifyFullDuplexStream) {
    const AudioApi inputAudioApi = std::get<0>(GetParam());
    const PerformanceMode inputPerformanceMode = std::get<1>(GetParam());
    const AudioApi outputAudioApi = std::get<2>(GetParam());
    const PerformanceMode outputPerformanceMode = std::get<3>(GetParam());

    openStream(inputAudioApi, inputPerformanceMode, outputAudioApi, outputPerformanceMode);
    startStream();
    usleep(kTimeToSleepMicros);
    checkXRuns();
    checkInputAndOutputBufferSizesMatch();
    stopStream();
    closeStream();
}

INSTANTIATE_TEST_SUITE_P(
        TestFullDuplexStreamTest,
        TestFullDuplexStream,
        ::testing::Values(
                TestFullDuplexStreamParams({AudioApi::AAudio, PerformanceMode::LowLatency,
                        AudioApi::AAudio, PerformanceMode::LowLatency}),
                TestFullDuplexStreamParams({AudioApi::AAudio, PerformanceMode::LowLatency,
                        AudioApi::AAudio, PerformanceMode::None}),
                TestFullDuplexStreamParams({AudioApi::AAudio, PerformanceMode::LowLatency,
                        AudioApi::AAudio, PerformanceMode::PowerSaving}),
                TestFullDuplexStreamParams({AudioApi::AAudio, PerformanceMode::LowLatency,
                        AudioApi::OpenSLES, PerformanceMode::LowLatency}),
                TestFullDuplexStreamParams({AudioApi::AAudio, PerformanceMode::LowLatency,
                        AudioApi::OpenSLES, PerformanceMode::None}),
                TestFullDuplexStreamParams({AudioApi::AAudio, PerformanceMode::LowLatency,
                        AudioApi::OpenSLES, PerformanceMode::PowerSaving}),
                TestFullDuplexStreamParams({AudioApi::AAudio, PerformanceMode::None,
                        AudioApi::AAudio, PerformanceMode::LowLatency}),
                TestFullDuplexStreamParams({AudioApi::AAudio, PerformanceMode::None,
                        AudioApi::AAudio, PerformanceMode::None}),
                TestFullDuplexStreamParams({AudioApi::AAudio, PerformanceMode::None,
                        AudioApi::AAudio, PerformanceMode::PowerSaving}),
                TestFullDuplexStreamParams({AudioApi::AAudio, PerformanceMode::None,
                        AudioApi::OpenSLES, PerformanceMode::LowLatency}),
                TestFullDuplexStreamParams({AudioApi::AAudio, PerformanceMode::None,
                        AudioApi::OpenSLES, PerformanceMode::None}),
                TestFullDuplexStreamParams({AudioApi::AAudio, PerformanceMode::None,
                        AudioApi::OpenSLES, PerformanceMode::PowerSaving}),
                TestFullDuplexStreamParams({AudioApi::AAudio, PerformanceMode::PowerSaving,
                        AudioApi::AAudio, PerformanceMode::LowLatency}),
                TestFullDuplexStreamParams({AudioApi::AAudio, PerformanceMode::PowerSaving,
                        AudioApi::AAudio, PerformanceMode::None}),
                TestFullDuplexStreamParams({AudioApi::AAudio, PerformanceMode::PowerSaving,
                        AudioApi::AAudio, PerformanceMode::PowerSaving}),
                TestFullDuplexStreamParams({AudioApi::AAudio, PerformanceMode::PowerSaving,
                        AudioApi::OpenSLES, PerformanceMode::LowLatency}),
                TestFullDuplexStreamParams({AudioApi::AAudio, PerformanceMode::PowerSaving,
                        AudioApi::OpenSLES, PerformanceMode::None}),
                TestFullDuplexStreamParams({AudioApi::AAudio, PerformanceMode::PowerSaving,
                        AudioApi::OpenSLES, PerformanceMode::PowerSaving}),
                TestFullDuplexStreamParams({AudioApi::OpenSLES, PerformanceMode::LowLatency,
                        AudioApi::AAudio, PerformanceMode::LowLatency}),
                TestFullDuplexStreamParams({AudioApi::OpenSLES, PerformanceMode::LowLatency,
                        AudioApi::AAudio, PerformanceMode::None}),
                TestFullDuplexStreamParams({AudioApi::OpenSLES, PerformanceMode::LowLatency,
                        AudioApi::AAudio, PerformanceMode::PowerSaving}),
                TestFullDuplexStreamParams({AudioApi::OpenSLES, PerformanceMode::LowLatency,
                        AudioApi::OpenSLES, PerformanceMode::LowLatency}),
                TestFullDuplexStreamParams({AudioApi::OpenSLES, PerformanceMode::LowLatency,
                        AudioApi::OpenSLES, PerformanceMode::None}),
                TestFullDuplexStreamParams({AudioApi::OpenSLES, PerformanceMode::LowLatency,
                        AudioApi::OpenSLES, PerformanceMode::PowerSaving}),
                TestFullDuplexStreamParams({AudioApi::OpenSLES, PerformanceMode::None,
                        AudioApi::AAudio, PerformanceMode::LowLatency}),
                TestFullDuplexStreamParams({AudioApi::OpenSLES, PerformanceMode::None,
                        AudioApi::AAudio, PerformanceMode::None}),
                TestFullDuplexStreamParams({AudioApi::OpenSLES, PerformanceMode::None,
                        AudioApi::AAudio, PerformanceMode::PowerSaving}),
                TestFullDuplexStreamParams({AudioApi::OpenSLES, PerformanceMode::None,
                        AudioApi::OpenSLES, PerformanceMode::LowLatency}),
                TestFullDuplexStreamParams({AudioApi::OpenSLES, PerformanceMode::None,
                        AudioApi::OpenSLES, PerformanceMode::None}),
                TestFullDuplexStreamParams({AudioApi::OpenSLES, PerformanceMode::None,
                        AudioApi::OpenSLES, PerformanceMode::PowerSaving}),
                TestFullDuplexStreamParams({AudioApi::OpenSLES, PerformanceMode::PowerSaving,
                        AudioApi::AAudio, PerformanceMode::LowLatency}),
                TestFullDuplexStreamParams({AudioApi::OpenSLES, PerformanceMode::PowerSaving,
                        AudioApi::AAudio, PerformanceMode::None}),
                TestFullDuplexStreamParams({AudioApi::OpenSLES, PerformanceMode::PowerSaving,
                        AudioApi::AAudio, PerformanceMode::PowerSaving}),
                TestFullDuplexStreamParams({AudioApi::OpenSLES, PerformanceMode::PowerSaving,
                        AudioApi::OpenSLES, PerformanceMode::LowLatency}),
                TestFullDuplexStreamParams({AudioApi::OpenSLES, PerformanceMode::PowerSaving,
                        AudioApi::OpenSLES, PerformanceMode::None}),
                TestFullDuplexStreamParams({AudioApi::OpenSLES, PerformanceMode::PowerSaving,
                        AudioApi::OpenSLES, PerformanceMode::PowerSaving})
        )
);
