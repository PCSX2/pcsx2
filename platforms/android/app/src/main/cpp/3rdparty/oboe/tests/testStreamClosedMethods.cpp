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

class MyCallback : public AudioStreamDataCallback {
public:
    DataCallbackResult onAudioReady(AudioStream *oboeStream, void *audioData, int32_t numFrames) override {
        return DataCallbackResult::Continue;
    }
};

class StreamClosedReturnValues : public ::testing::Test {

protected:

    bool openStream() {
        Result r = mBuilder.openStream(mStream);
        EXPECT_EQ(r, Result::OK) << "Failed to open stream " << convertToText(r);
        return (r == Result::OK);
    }

    bool releaseStream() {
        Result r = mStream->release();
        if (getSdkVersion() > __ANDROID_API_R__ && mBuilder.getAudioApi() != AudioApi::OpenSLES) {
            EXPECT_EQ(r, Result::OK) << "Failed to release stream. " << convertToText(r);
            return (r == Result::OK);
        } else {
            EXPECT_EQ(r, Result::ErrorUnimplemented) << "Did not get  ErrorUnimplemented" << convertToText(r);
            return (r == Result::ErrorUnimplemented);
        }
    }

    bool closeStream() {
        Result r = mStream->close();
        EXPECT_EQ(r, Result::OK) << "Failed to close stream. " << convertToText(r);
        return (r == Result::OK);
    }

    bool openAndCloseStream() {
        if (!openStream() || !closeStream())
            return false;
        StreamState s = mStream->getState();
        EXPECT_EQ(s, StreamState::Closed) << "Stream state " << convertToText(mStream->getState());
        return (s == StreamState::Closed);
    }

    static int64_t getNanoseconds() {
        struct timespec time;
        int result = clock_gettime(CLOCK_MONOTONIC, &time);
        if (result < 0) {
            return result;
        }
        return (time.tv_sec * (int64_t)1e9) + time.tv_nsec;
    }

	// ASSERT_* requires a void return type.
    void measureCloseTime(int32_t delayMillis) {
        ASSERT_TRUE(openStream());
        mStream->setDelayBeforeCloseMillis(delayMillis);
        ASSERT_EQ(delayMillis, mStream->getDelayBeforeCloseMillis());
        // Measure time it takes to close.
        int64_t startTimeMillis = getNanoseconds() / 1e6;
        ASSERT_TRUE(closeStream());
        int64_t stopTimeMillis = getNanoseconds() / 1e6;
        int32_t elapsedTimeMillis = (int32_t)(stopTimeMillis - startTimeMillis);
        ASSERT_GE(elapsedTimeMillis, delayMillis);
    }

    void testDelayBeforeClose() {
        const int32_t delayMillis = 500;
        measureCloseTime(delayMillis);
    }

    AudioStreamBuilder mBuilder;
    std::shared_ptr<AudioStream> mStream;

};

TEST_F(StreamClosedReturnValues, GetChannelCountReturnsLastKnownValue){

    mBuilder.setChannelCount(2);
    ASSERT_TRUE(openAndCloseStream());
    ASSERT_EQ(mStream->getChannelCount(), 2);
}

TEST_F(StreamClosedReturnValues, GetDirectionReturnsLastKnownValue){

    // Note that when testing on the emulator setting the direction to Input will result in ErrorInternal when
    // opening the stream
    mBuilder.setDirection(Direction::Input);
    ASSERT_TRUE(openAndCloseStream());
    ASSERT_EQ(mStream->getDirection(), Direction::Input);
}

TEST_F(StreamClosedReturnValues, GetSampleRateReturnsLastKnownValue){

    mBuilder.setSampleRate(8000);
    ASSERT_TRUE(openAndCloseStream());
    ASSERT_EQ(mStream->getSampleRate(), 8000);
}

TEST_F(StreamClosedReturnValues, GetFramesPerCallbackReturnsLastKnownValue) {

    mBuilder.setFramesPerCallback(192);
    ASSERT_TRUE(openAndCloseStream());
    ASSERT_EQ(mStream->getFramesPerCallback(), 192);
}

TEST_F(StreamClosedReturnValues, GetFormatReturnsLastKnownValue) {

    mBuilder.setFormat(AudioFormat::I16);
    ASSERT_TRUE(openAndCloseStream());
    ASSERT_EQ(mStream->getFormat(), AudioFormat::I16);
}

TEST_F(StreamClosedReturnValues, GetBufferSizeInFramesReturnsLastKnownValue) {

    ASSERT_TRUE(openStream());
    int32_t bufferSize = mStream->getBufferSizeInFrames();
    ASSERT_TRUE(closeStream());
    ASSERT_EQ(mStream->getBufferSizeInFrames(), bufferSize);
}

TEST_F(StreamClosedReturnValues, GetBufferCapacityInFramesReturnsLastKnownValue) {

    ASSERT_TRUE(openStream());
    int32_t bufferCapacity = mStream->getBufferCapacityInFrames();
    ASSERT_TRUE(closeStream());
    ASSERT_EQ(mStream->getBufferCapacityInFrames(), bufferCapacity);
}

TEST_F(StreamClosedReturnValues, GetSharingModeReturnsLastKnownValue) {

    ASSERT_TRUE(openStream());
    SharingMode s = mStream->getSharingMode();
    ASSERT_TRUE(closeStream());
    ASSERT_EQ(mStream->getSharingMode(), s);
}

TEST_F(StreamClosedReturnValues, GetPerformanceModeReturnsLastKnownValue) {

    ASSERT_TRUE(openStream());
    PerformanceMode p = mStream->getPerformanceMode();
    ASSERT_TRUE(closeStream());
    ASSERT_EQ(mStream->getPerformanceMode(), p);
}

TEST_F(StreamClosedReturnValues, GetDeviceIdReturnsLastKnownValue) {

    ASSERT_TRUE(openStream());
    int32_t d = mStream->getDeviceId();
    ASSERT_TRUE(closeStream());
    ASSERT_EQ(mStream->getDeviceId(), d);
}

TEST_F(StreamClosedReturnValues, GetDataCallbackReturnsLastKnownValue) {

    AudioStreamDataCallback *callback = new MyCallback();
    mBuilder.setDataCallback(callback);
    ASSERT_TRUE(openAndCloseStream());

    AudioStreamDataCallback *callback2 = mStream->getDataCallback();
    ASSERT_EQ(callback, callback2);
}

TEST_F(StreamClosedReturnValues, GetUsageReturnsLastKnownValue){
    ASSERT_TRUE(openStream());
    Usage u = mStream->getUsage();
    ASSERT_TRUE(closeStream());
    ASSERT_EQ(mStream->getUsage(), u);
}

TEST_F(StreamClosedReturnValues, GetContentTypeReturnsLastKnownValue){
    ASSERT_TRUE(openStream());
    ContentType c = mStream->getContentType();
    ASSERT_TRUE(closeStream());
    ASSERT_EQ(mStream->getContentType(), c);
}

TEST_F(StreamClosedReturnValues, GetInputPresetReturnsLastKnownValue){
    ASSERT_TRUE(openStream());
    auto i = mStream->getInputPreset();
    ASSERT_TRUE(closeStream());
    ASSERT_EQ(mStream->getInputPreset(), i);
}

TEST_F(StreamClosedReturnValues, GetSessionIdReturnsLastKnownValue){
    ASSERT_TRUE(openStream());
    auto s = mStream->getSessionId();
    ASSERT_TRUE(closeStream());
    ASSERT_EQ(mStream->getSessionId(), s);
}

TEST_F(StreamClosedReturnValues, StreamStateIsClosed){
    ASSERT_TRUE(openAndCloseStream());
    ASSERT_EQ(mStream->getState(), StreamState::Closed);
}

TEST_F(StreamClosedReturnValues, GetXRunCountReturnsLastKnownValue){

    ASSERT_TRUE(openStream());
    if (mStream->isXRunCountSupported()){
        auto i = mStream->getXRunCount();
        ASSERT_EQ(mStream->getXRunCount(), i);
    }
    ASSERT_TRUE(closeStream());
}

TEST_F(StreamClosedReturnValues, GetFramesPerBurstReturnsLastKnownValue){

    ASSERT_TRUE(openStream());
    auto f = mStream->getFramesPerBurst();
    ASSERT_TRUE(closeStream());
    ASSERT_EQ(mStream->getFramesPerBurst(), f);
}

TEST_F(StreamClosedReturnValues, GetBytesPerFrameReturnsLastKnownValue){
    ASSERT_TRUE(openStream());
    auto f = mStream->getBytesPerFrame();
    ASSERT_TRUE(closeStream());
    ASSERT_EQ(mStream->getBytesPerFrame(), f);
}

TEST_F(StreamClosedReturnValues, GetBytesPerSampleReturnsLastKnownValue){
    ASSERT_TRUE(openStream());
    auto f = mStream->getBytesPerSample();
    ASSERT_TRUE(closeStream());
    ASSERT_EQ(mStream->getBytesPerSample(), f);
}

TEST_F(StreamClosedReturnValues, GetFramesWrittenReturnsLastKnownValue){
    mBuilder.setFormat(AudioFormat::I16);
    mBuilder.setChannelCount(1);
    ASSERT_TRUE(openStream());
    ASSERT_EQ(mStream->setBufferSizeInFrames(mStream->getBufferCapacityInFrames()), Result::OK);
    mStream->start();

    int16_t buffer[4] = { 1, 2, 3, 4 };
    Result r = mStream->write(&buffer, 4, 0);
    if (r != Result::OK) {
        FAIL() << "Could not write to audio stream";
    }

    auto f = mStream->getFramesWritten();
    ASSERT_EQ(f, 4);

    ASSERT_TRUE(closeStream());
    ASSERT_EQ(mStream->getFramesWritten(), f);
}

TEST_F(StreamClosedReturnValues, GetFramesReadReturnsLastKnownValue) {

    mBuilder.setDirection(Direction::Input);
    mBuilder.setFormat(AudioFormat::I16);
    mBuilder.setChannelCount(1);

    ASSERT_TRUE(openStream());
    mStream->start();

    int16_t buffer[192];
    auto r = mStream->read(&buffer, 192, 1000 * kNanosPerMillisecond);
    ASSERT_EQ(r.value(), 192);

    auto f = mStream->getFramesRead();
    ASSERT_EQ(f, 192);

    ASSERT_TRUE(closeStream());
    ASSERT_EQ(mStream->getFramesRead(), f);
}

TEST_F(StreamClosedReturnValues, GetTimestampReturnsErrorClosedIfSupported){

    ASSERT_TRUE(openStream());

    int64_t framePosition;
    int64_t presentationTime;

    auto r = mStream->getTimestamp(CLOCK_MONOTONIC, &framePosition, &presentationTime);
    bool isTimestampSupported = (r == Result::OK);

    ASSERT_TRUE(closeStream());

    if (isTimestampSupported){
        ASSERT_EQ(mStream->getTimestamp(CLOCK_MONOTONIC, &framePosition, &presentationTime), Result::ErrorClosed);
    }
}

TEST_F(StreamClosedReturnValues, GetAudioApiReturnsLastKnownValue){
    ASSERT_TRUE(openStream());
    AudioApi a = mStream->getAudioApi();
    ASSERT_TRUE(closeStream());
    ASSERT_EQ(mStream->getAudioApi(), a);
}

TEST_F(StreamClosedReturnValues, GetUsesAAudioReturnsLastKnownValue){
    ASSERT_TRUE(openStream());
    bool a = mStream->usesAAudio();
    ASSERT_TRUE(closeStream());
    ASSERT_EQ(mStream->usesAAudio(), a);
}

TEST_F(StreamClosedReturnValues, StreamStateControlsReturnClosed){

    ASSERT_TRUE(openAndCloseStream());
    Result r = mStream->close();
    EXPECT_EQ(r, Result::ErrorClosed) << convertToText(r);
    r = mStream->start();
    EXPECT_EQ(r, Result::ErrorClosed) << convertToText(r);
    EXPECT_EQ(mStream->pause(), Result::ErrorClosed);
    EXPECT_EQ(mStream->flush(), Result::ErrorClosed);
    EXPECT_EQ(mStream->stop(), Result::ErrorClosed);
    EXPECT_EQ(mStream->requestStart(), Result::ErrorClosed);
    EXPECT_EQ(mStream->requestPause(), Result::ErrorClosed);
    EXPECT_EQ(mStream->requestFlush(), Result::ErrorClosed);
    EXPECT_EQ(mStream->requestStop(), Result::ErrorClosed);
}

TEST_F(StreamClosedReturnValues, WaitForStateChangeReturnsClosed){

    ASSERT_TRUE(openAndCloseStream());
    StreamState next;
    Result r = mStream->waitForStateChange(StreamState::Open, &next, 0);
    EXPECT_TRUE(r == Result::OK || r == Result::ErrorClosed) << convertToText(r);
}

TEST_F(StreamClosedReturnValues, SetBufferSizeInFramesReturnsClosed){

    ASSERT_TRUE(openAndCloseStream());
    auto r = mStream->setBufferSizeInFrames(192);
    ASSERT_EQ(r.error(), Result::ErrorClosed);
}

TEST_F(StreamClosedReturnValues, CalculateLatencyInMillisReturnsClosedIfSupported){

    ASSERT_TRUE(openAndCloseStream());

    if (mStream->getAudioApi() == AudioApi::AAudio){
        auto r = mStream->calculateLatencyMillis();
        ASSERT_EQ(r.error(), Result::ErrorInvalidState);
    }
}

TEST_F(StreamClosedReturnValues, ReadReturnsClosed){

    ASSERT_TRUE(openAndCloseStream());

    int buffer[8]{0};
    auto r = mStream->read(buffer, 1, 0);
    ASSERT_EQ(r.error(), Result::ErrorClosed);
}

TEST_F(StreamClosedReturnValues, WriteReturnsClosed){

    ASSERT_TRUE(openAndCloseStream());

    int buffer[8]{0};
    auto r = mStream->write(buffer, 1, 0);
    ASSERT_EQ(r.error(), Result::ErrorClosed);
}

TEST_F(StreamClosedReturnValues, DelayBeforeCloseInput){
    if (AudioStreamBuilder::isAAudioRecommended()) {
        mBuilder.setDirection(Direction::Input);
        testDelayBeforeClose();
    }
}

TEST_F(StreamClosedReturnValues, DelayBeforeCloseOutput){
    if (AudioStreamBuilder::isAAudioRecommended()) {
        mBuilder.setDirection(Direction::Output);
        testDelayBeforeClose();
    }
}

TEST_F(StreamClosedReturnValues, DelayBeforeCloseInputOpenSL){
    mBuilder.setAudioApi(AudioApi::OpenSLES);
    mBuilder.setDirection(Direction::Input);
    testDelayBeforeClose();
}

TEST_F(StreamClosedReturnValues, DelayBeforeCloseOutputOpenSL){
    mBuilder.setAudioApi(AudioApi::OpenSLES);
    mBuilder.setDirection(Direction::Output);
    testDelayBeforeClose();
}

TEST_F(StreamClosedReturnValues, TestReleaseInput){
    mBuilder.setDirection(Direction::Input);
    ASSERT_TRUE(openStream());
    ASSERT_TRUE(releaseStream());
    ASSERT_TRUE(closeStream());
}

TEST_F(StreamClosedReturnValues, TestReleaseInputOpenSLES){
    mBuilder.setAudioApi(AudioApi::OpenSLES);
    mBuilder.setDirection(Direction::Input);
    ASSERT_TRUE(openStream());
    ASSERT_TRUE(releaseStream());
    ASSERT_TRUE(closeStream());
}

TEST_F(StreamClosedReturnValues, TestReleaseOutput){
    mBuilder.setDirection(Direction::Output);
    ASSERT_TRUE(openStream());
    ASSERT_TRUE(releaseStream());
    ASSERT_TRUE(closeStream());
}

TEST_F(StreamClosedReturnValues, TestReleaseOutputOpenSLES){
    mBuilder.setAudioApi(AudioApi::OpenSLES);
    mBuilder.setDirection(Direction::Output);
    ASSERT_TRUE(openStream());
    ASSERT_TRUE(releaseStream());
    ASSERT_TRUE(closeStream());
}
