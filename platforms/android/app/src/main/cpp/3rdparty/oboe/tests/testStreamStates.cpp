/*
 * Copyright 2018 The Android Open Source Project
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

using namespace oboe;

// Sleep between close and open to avoid a race condition inside Android Audio.
// On a Pixel 2 emulator on a fast Linux host, the minimum value is around 16 msec.
constexpr int kOboeOpenCloseSleepMSec = 100;

class StreamStates : public ::testing::Test {

protected:

    void SetUp(){
        mBuilder.setPerformanceMode(PerformanceMode::None);
        mBuilder.setDirection(Direction::Output);
    }

    bool openStream(Direction direction) {
        usleep(100 * 1000);
        mBuilder.setDirection(direction);
        Result r = mBuilder.openStream(mStream);
        EXPECT_EQ(r, Result::OK) << "Failed to open stream " << convertToText(r);
        if (r != Result::OK)
            return false;

        Direction d = mStream->getDirection();
        EXPECT_EQ(d, direction) << convertToText(mStream->getDirection());
        return (d == direction);
    }

    bool openStream() {
        return openStream(Direction::Output);
    }

    bool openInputStream() {
        return openStream(Direction::Input);
    }

    bool closeStream() {
        Result r = mStream->close();
        EXPECT_EQ(r, Result::OK) << "Failed to close stream. " << convertToText(r);
        return (r == Result::OK);
    }

    void checkStreamStateIsStartedAfterStartingTwice(Direction direction) {
        ASSERT_TRUE(openStream(direction));

        StreamState next = StreamState::Unknown;
        auto r = mStream->requestStart();
        EXPECT_EQ(r, Result::OK) << "requestStart returned: " << convertToText(r);
        r = mStream->waitForStateChange(StreamState::Starting, &next, kTimeoutInNanos);
        EXPECT_EQ(r, Result::OK);
        EXPECT_EQ(next, StreamState::Started);

        next = StreamState::Unknown;
        r = mStream->requestStart();
        // TODO On P, AAudio is returning ErrorInvalidState for Output and OK for Input
        // EXPECT_EQ(r, Result::OK) << "requestStart returned: " << convertToText(r);
        r = mStream->waitForStateChange(StreamState::Starting, &next, kTimeoutInNanos);
        EXPECT_EQ(r, Result::OK);
        ASSERT_EQ(next, StreamState::Started);

        ASSERT_TRUE(closeStream());
    }

    void checkStreamStateIsStoppedAfterStoppingTwice(Direction direction) {
        ASSERT_TRUE(openStream(direction));

        StreamState next = StreamState::Unknown;
        auto r = mStream->requestStart();
        EXPECT_EQ(r, Result::OK);

        r = mStream->requestStop();
        EXPECT_EQ(r, Result::OK);
        r = mStream->waitForStateChange(StreamState::Stopping, &next, kTimeoutInNanos);
        EXPECT_EQ(r, Result::OK);
        EXPECT_EQ(next, StreamState::Stopped);

        r = mStream->requestStop();
        EXPECT_EQ(r, Result::OK);
        next = StreamState::Unknown;
        r = mStream->waitForStateChange(StreamState::Stopping, &next, kTimeoutInNanos);
        EXPECT_EQ(r, Result::OK);
        ASSERT_EQ(next, StreamState::Stopped);

        ASSERT_TRUE(closeStream());
    }

    // TODO: This seems to fail intermittently on Pixel OC_MR1 !
    void checkStreamLeftRunningShouldNotInterfereWithNextOpen(Direction direction) {
        ASSERT_TRUE(openStream(direction));

        auto r = mStream->requestStart();
        EXPECT_EQ(r, Result::OK);
        // It should be safe to close without stopping.
        // The underlying API should stop the stream.
        ASSERT_TRUE(closeStream());

        usleep(kOboeOpenCloseSleepMSec * 1000); // avoid race condition in emulator

        ASSERT_TRUE(openInputStream());
        r = mStream->requestStart();
        ASSERT_EQ(r, Result::OK) << "requestStart returned: " << convertToText(r);

        r = mStream->requestStop();
        EXPECT_EQ(r, Result::OK) << "requestStop returned: " << convertToText(r);
        ASSERT_TRUE(closeStream());
    }

    AudioStreamBuilder mBuilder;
    std::shared_ptr<AudioStream> mStream;
    static constexpr int kTimeoutInNanos = 500 * kNanosPerMillisecond;

};

TEST_F(StreamStates, OutputStreamStateIsOpenAfterOpening){
    ASSERT_TRUE(openStream());
    StreamState next = StreamState::Unknown;
    Result r = mStream->waitForStateChange(StreamState::Uninitialized, &next, kTimeoutInNanos);
    EXPECT_EQ(r, Result::OK) << convertToText(r);
    ASSERT_EQ(next, StreamState::Open) << convertToText(next);
    ASSERT_TRUE(closeStream());
}

TEST_F(StreamStates, OutputStreamStateIsStartedAfterStarting){

    ASSERT_TRUE(openStream());

    StreamState next = StreamState::Unknown;
    auto r = mStream->requestStart();
    EXPECT_EQ(r, Result::OK);

    r = mStream->waitForStateChange(StreamState::Starting, &next, kTimeoutInNanos);
    EXPECT_EQ(r, Result::OK);
    ASSERT_EQ(next, StreamState::Started);

    ASSERT_TRUE(closeStream());
}

TEST_F(StreamStates, OutputStreamStateIsPausedAfterPausing){

    ASSERT_TRUE(openStream());

    StreamState next = StreamState::Unknown;
    auto r = mStream->requestStart();
    EXPECT_EQ(r, Result::OK);

    r = mStream->requestPause();
    EXPECT_EQ(r, Result::OK);

    r = mStream->waitForStateChange(StreamState::Pausing, &next, kTimeoutInNanos);
    EXPECT_EQ(r, Result::OK);

    ASSERT_EQ(next, StreamState::Paused);

    ASSERT_TRUE(closeStream());
}

TEST_F(StreamStates, OutputStreamStateIsStoppedAfterStopping){

    ASSERT_TRUE(openStream());

    StreamState next = StreamState::Unknown;
    auto r = mStream->requestStart();
    EXPECT_EQ(r, Result::OK);

    r = mStream->requestStop();
    r = mStream->waitForStateChange(StreamState::Stopping, &next, kTimeoutInNanos);
    EXPECT_EQ(r, Result::OK);

    ASSERT_EQ(next, StreamState::Stopped);

    ASSERT_TRUE(closeStream());
}

TEST_F(StreamStates, InputStreamStateIsOpenAfterOpening){
    ASSERT_TRUE(openInputStream());
    StreamState next = StreamState::Unknown;
    Result r = mStream->waitForStateChange(StreamState::Uninitialized, &next, kTimeoutInNanos);
    EXPECT_EQ(r, Result::OK) << convertToText(r);
    ASSERT_EQ(next, StreamState::Open) << convertToText(next);
    ASSERT_TRUE(closeStream());
}

TEST_F(StreamStates, InputStreamStateIsStartedAfterStarting){

    ASSERT_TRUE(openInputStream());

    StreamState next = StreamState::Unknown;
    auto r = mStream->requestStart();
    EXPECT_EQ(r, Result::OK);

    r = mStream->waitForStateChange(StreamState::Starting, &next, kTimeoutInNanos);
    EXPECT_EQ(r, Result::OK);

    ASSERT_EQ(next, StreamState::Started);

    ASSERT_TRUE(closeStream());
}

TEST_F(StreamStates, OutputStreamStateIsStartedAfterStartingTwice){
    checkStreamStateIsStartedAfterStartingTwice(Direction::Output);
}

TEST_F(StreamStates, InputStreamStateIsStartedAfterStartingTwice){
    checkStreamStateIsStartedAfterStartingTwice(Direction::Input);
}

TEST_F(StreamStates, OutputStreamStateIsStoppedAfterStoppingTwice){
    checkStreamStateIsStoppedAfterStoppingTwice(Direction::Output);
}

TEST_F(StreamStates, InputStreamStateIsStoppedAfterStoppingTwice){
    checkStreamStateIsStoppedAfterStoppingTwice(Direction::Input);
}

TEST_F(StreamStates, OutputStreamStateIsPausedAfterPausingTwice){
    ASSERT_TRUE(openStream());

    StreamState next = StreamState::Unknown;
    auto r = mStream->requestStart();
    EXPECT_EQ(r, Result::OK);

    r = mStream->requestPause();
    EXPECT_EQ(r, Result::OK);
    r = mStream->waitForStateChange(StreamState::Pausing, &next, kTimeoutInNanos);
    EXPECT_EQ(r, Result::OK);
    EXPECT_EQ(next, StreamState::Paused);

    // requestPause() while already paused could leave us in Pausing in AAudio O_MR1.
    r = mStream->requestPause();
    EXPECT_EQ(r, Result::OK);
    next = StreamState::Unknown;
    r = mStream->waitForStateChange(StreamState::Pausing, &next, kTimeoutInNanos);
    EXPECT_EQ(r, Result::OK);
    ASSERT_EQ(next, StreamState::Paused);

    ASSERT_TRUE(closeStream());
}

TEST_F(StreamStates, InputStreamDoesNotSupportPause){

    ASSERT_TRUE(openInputStream());
    auto r = mStream->requestStart();
    EXPECT_EQ(r, Result::OK);
    r = mStream->requestPause();

    ASSERT_EQ(r, Result::ErrorUnimplemented) << convertToText(r);
    mStream->requestStop();
    ASSERT_TRUE(closeStream());
}

TEST_F(StreamStates, OutputStreamLeftRunningShouldNotInterfereWithNextOpen) {
    checkStreamLeftRunningShouldNotInterfereWithNextOpen(Direction::Output);
}

TEST_F(StreamStates, InputStreamLeftRunningShouldNotInterfereWithNextOpen) {
    checkStreamLeftRunningShouldNotInterfereWithNextOpen(Direction::Input);
}

TEST_F(StreamStates, OutputLowLatencyStreamLeftRunningShouldNotInterfereWithNextOpen) {
    mBuilder.setPerformanceMode(PerformanceMode::LowLatency);
    checkStreamLeftRunningShouldNotInterfereWithNextOpen(Direction::Output);
}

TEST_F(StreamStates, InputLowLatencyStreamLeftRunningShouldNotInterfereWithNextOpen) {
    mBuilder.setPerformanceMode(PerformanceMode::LowLatency);
    checkStreamLeftRunningShouldNotInterfereWithNextOpen(Direction::Input);
}

TEST_F(StreamStates, InputStreamStateIsStoppedAfterStopping){

    ASSERT_TRUE(openInputStream());

    StreamState next = StreamState::Unknown;
    auto r = mStream->requestStart();
    EXPECT_EQ(r, Result::OK) << "requestStart returned: " << convertToText(r);

    r = mStream->requestStop();
    EXPECT_EQ(r, Result::OK) << "requestStop returned: " << convertToText(r);

    r = mStream->waitForStateChange(StreamState::Stopping, &next, kTimeoutInNanos);
    EXPECT_EQ(r, Result::OK) << "waitForStateChange returned: " << convertToText(r);

    ASSERT_EQ(next, StreamState::Stopped);

    ASSERT_TRUE(closeStream());
}
