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

#include <aaudio/AAudioExtensions.h>
#include <oboe/Oboe.h>

#include <android/api-level.h>
#ifndef __ANDROID_API_S__
#define __ANDROID_API_S__ 31
#endif

#ifndef __ANDROID_API_S_V2__
#define __ANDROID_API_S_V2__ 32
#endif

using namespace oboe;

class CallbackSizeMonitor : public AudioStreamCallback {
public:
    DataCallbackResult onAudioReady(AudioStream *oboeStream, void *audioData, int32_t numFrames) override {
        framesPerCallback = numFrames;
        callbackCount++;
        return DataCallbackResult::Continue;
    }

    // This is exposed publicly so that the number of frames per callback can be tested.
    std::atomic<int32_t> framesPerCallback{0};
    std::atomic<int32_t> callbackCount{0};
};

class StreamOpen : public ::testing::Test {

protected:

    bool openStream() {
        EXPECT_EQ(mStream, nullptr);
        Result r = mBuilder.openStream(mStream);
        EXPECT_EQ(r, Result::OK) << "Failed to open stream " << convertToText(r);
        EXPECT_EQ(0, openCount) << "Should start with a fresh object every time.";
        openCount++;
        return (r == Result::OK);
    }

    bool closeStream() {
        if (mStream){
          Result r = mStream->close();
          EXPECT_EQ(r, Result::OK) << "Failed to close stream. " << convertToText(r);
          usleep(500 * 1000); // give previous stream time to settle
          return (r == Result::OK);
        } else {
          return true;
        }
    }

    void checkSampleRateConversionAdvancing(Direction direction) {
        CallbackSizeMonitor callback;

        mBuilder.setDirection(direction);
        if (mBuilder.isAAudioRecommended()) {
            mBuilder.setAudioApi(AudioApi::AAudio);
        }
        mBuilder.setCallback(&callback);
        mBuilder.setPerformanceMode(PerformanceMode::LowLatency);
        mBuilder.setSampleRate(44100);
        mBuilder.setSampleRateConversionQuality(SampleRateConversionQuality::Medium);

        ASSERT_TRUE(openStream());

        ASSERT_EQ(mStream->requestStart(), Result::OK);
        int timeout = 20;
        while (callback.framesPerCallback == 0 && timeout > 0) {
            usleep(50 * 1000);
            timeout--;
        }

        // Catch Issue #1166
        mStream->getTimestamp(CLOCK_MONOTONIC); // should not crash
        mStream->getTimestamp(CLOCK_MONOTONIC, nullptr, nullptr); // should not crash

        ASSERT_GT(callback.callbackCount, 0);
        ASSERT_GT(callback.framesPerCallback, 0);
        ASSERT_EQ(mStream->requestStop(), Result::OK);

        ASSERT_TRUE(closeStream());
    }

    AudioStreamBuilder mBuilder;
    std::shared_ptr<AudioStream> mStream;
    int32_t openCount = 0;

};

class StreamOpenOutput : public StreamOpen {};
class StreamOpenInput : public StreamOpen {};

TEST_F(StreamOpenOutput, ForOpenSLESDefaultSampleRateIsUsed){

    DefaultStreamValues::SampleRate = 44100;
    DefaultStreamValues::FramesPerBurst = 192;
    mBuilder.setAudioApi(AudioApi::OpenSLES);
    ASSERT_TRUE(openStream());
    ASSERT_EQ(mStream->getSampleRate(), 44100);
    ASSERT_TRUE(closeStream());
}

TEST_F(StreamOpenOutput, ForOpenSLESDefaultFramesPerBurstIsUsed){

    DefaultStreamValues::SampleRate = 48000;
    DefaultStreamValues::FramesPerBurst = 128; // used for low latency
    mBuilder.setAudioApi(AudioApi::OpenSLES);
    mBuilder.setPerformanceMode(PerformanceMode::LowLatency);
    ASSERT_TRUE(openStream());
    // Some devices like emulators may not support Low Latency
    if (mStream->getPerformanceMode() == PerformanceMode::LowLatency) {
        ASSERT_EQ(mStream->getFramesPerBurst(), 128);
    }
    ASSERT_TRUE(closeStream());
}

TEST_F(StreamOpenOutput, ForOpenSLESDefaultChannelCountIsUsed){

    DefaultStreamValues::ChannelCount = 1;
    mBuilder.setAudioApi(AudioApi::OpenSLES);
    ASSERT_TRUE(openStream());
    ASSERT_EQ(mStream->getChannelCount(), 1);
    ASSERT_TRUE(closeStream());
}

TEST_F(StreamOpenOutput, OutputForOpenSLESPerformanceModeShouldBeNone){
    // We will not get a LowLatency stream if we request 16000 Hz.
    mBuilder.setSampleRate(16000);
    mBuilder.setSampleRateConversionQuality(SampleRateConversionQuality::None);
    mBuilder.setPerformanceMode(PerformanceMode::LowLatency);
    mBuilder.setDirection(Direction::Output);
    mBuilder.setAudioApi(AudioApi::OpenSLES);
	  ASSERT_TRUE(openStream());
    ASSERT_EQ((int)mStream->getPerformanceMode(), (int)PerformanceMode::None);
    ASSERT_TRUE(closeStream());
}

TEST_F(StreamOpenInput, InputForOpenSLESPerformanceModeShouldBeNone){
    // We will not get a LowLatency stream if we request 16000 Hz.
    mBuilder.setSampleRate(16000);
    mBuilder.setSampleRateConversionQuality(SampleRateConversionQuality::None);
    mBuilder.setPerformanceMode(PerformanceMode::LowLatency);
    mBuilder.setDirection(Direction::Input);
    mBuilder.setAudioApi(AudioApi::OpenSLES);
    ASSERT_TRUE(openStream());
    ASSERT_EQ((int)mStream->getPerformanceMode(), (int)PerformanceMode::None);
    ASSERT_TRUE(closeStream());
}

TEST_F(StreamOpenOutput, ForOpenSlesIllegalFormatRejectedOutput) {
    mBuilder.setAudioApi(AudioApi::OpenSLES);
    mBuilder.setPerformanceMode(PerformanceMode::LowLatency);
    mBuilder.setFormat(static_cast<AudioFormat>(666));
    Result r = mBuilder.openStream(mStream);
    EXPECT_NE(r, Result::OK) << "Should not open stream " << convertToText(r);
    if (mStream != nullptr) {
        mStream->close(); // just in case it accidentally opened
    }
}

TEST_F(StreamOpenInput, ForOpenSlesIllegalFormatRejectedInput) {
    mBuilder.setAudioApi(AudioApi::OpenSLES);
    mBuilder.setPerformanceMode(PerformanceMode::LowLatency);
    mBuilder.setDirection(Direction::Input);
    mBuilder.setFormat(static_cast<AudioFormat>(666));
    Result r = mBuilder.openStream(mStream);
    EXPECT_NE(r, Result::OK) << "Should not open stream " << convertToText(r);
    if (mStream != nullptr) {
        mStream->close(); // just in case it accidentally opened
    }
}

// Make sure the callback is called with the requested FramesPerCallback
TEST_F(StreamOpenOutput, OpenSLESFramesPerCallback) {
    const int kRequestedFramesPerCallback = 417;
    CallbackSizeMonitor callback;

    DefaultStreamValues::SampleRate = 48000;
    DefaultStreamValues::ChannelCount = 2;
    DefaultStreamValues::FramesPerBurst = 192;
    mBuilder.setAudioApi(AudioApi::OpenSLES);
    mBuilder.setFramesPerCallback(kRequestedFramesPerCallback);
    mBuilder.setCallback(&callback);
    ASSERT_TRUE(openStream());
    ASSERT_EQ(mStream->requestStart(), Result::OK);
    int timeout = 20;
    while (callback.framesPerCallback == 0 && timeout > 0) {
        usleep(50 * 1000);
        timeout--;
    }
    ASSERT_EQ(kRequestedFramesPerCallback, callback.framesPerCallback);
    ASSERT_EQ(kRequestedFramesPerCallback, mStream->getFramesPerCallback());
    ASSERT_EQ(mStream->requestStop(), Result::OK);
    ASSERT_TRUE(closeStream());
}

// Make sure the LowLatency callback has the requested FramesPerCallback.
TEST_F(StreamOpen, AAudioFramesPerCallbackLowLatency) {
    const int kRequestedFramesPerCallback = 192;
    CallbackSizeMonitor callback;

    mBuilder.setAudioApi(AudioApi::AAudio);
    mBuilder.setFramesPerCallback(kRequestedFramesPerCallback);
    mBuilder.setCallback(&callback);
    mBuilder.setPerformanceMode(PerformanceMode::LowLatency);
    ASSERT_TRUE(openStream());
    ASSERT_EQ(kRequestedFramesPerCallback, mStream->getFramesPerCallback());
    ASSERT_EQ(mStream->requestStart(), Result::OK);
    int timeout = 20;
    while (callback.framesPerCallback == 0 && timeout > 0) {
        usleep(50 * 1000);
        timeout--;
    }
    ASSERT_EQ(kRequestedFramesPerCallback, callback.framesPerCallback);
    ASSERT_EQ(mStream->requestStop(), Result::OK);
    ASSERT_TRUE(closeStream());
}

// Make sure the regular callback has the requested FramesPerCallback.
TEST_F(StreamOpen, AAudioFramesPerCallbackNone) {
    const int kRequestedFramesPerCallback = 1024;
    CallbackSizeMonitor callback;

    mBuilder.setAudioApi(AudioApi::AAudio);
    mBuilder.setFramesPerCallback(kRequestedFramesPerCallback);
    mBuilder.setCallback(&callback);
    mBuilder.setPerformanceMode(PerformanceMode::None);
    ASSERT_TRUE(openStream());
    ASSERT_EQ(kRequestedFramesPerCallback, mStream->getFramesPerCallback());
    ASSERT_EQ(mStream->requestStart(), Result::OK);
    int timeout = 20;
    while (callback.framesPerCallback == 0 && timeout > 0) {
        usleep(50 * 1000);
        timeout--;
    }
    ASSERT_EQ(kRequestedFramesPerCallback, callback.framesPerCallback);
    ASSERT_EQ(mStream->requestStop(), Result::OK);
    ASSERT_TRUE(closeStream());
}

TEST_F(StreamOpenInput, RecordingFormatUnspecifiedReturnsI16BeforeMarshmallow){

    if (getSdkVersion() < __ANDROID_API_M__){
        mBuilder.setDirection(Direction::Input);
        mBuilder.setFormat(AudioFormat::Unspecified);
        ASSERT_TRUE(openStream());
        ASSERT_EQ(mStream->getFormat(), AudioFormat::I16);
        ASSERT_TRUE(closeStream());
    }
}

TEST_F(StreamOpenInput, RecordingFormatUnspecifiedReturnsFloatOnMarshmallowAndLater){

    if (getSdkVersion() >= __ANDROID_API_M__){
        mBuilder.setDirection(Direction::Input);
        mBuilder.setFormat(AudioFormat::Unspecified);
        ASSERT_TRUE(openStream());
        ASSERT_EQ(mStream->getFormat(), AudioFormat::Float);
        ASSERT_TRUE(closeStream());
    }
}

TEST_F(StreamOpenInput, RecordingFormatFloatReturnsErrorBeforeMarshmallow){

    if (getSdkVersion() < __ANDROID_API_M__){
        mBuilder.setDirection(Direction::Input);
        mBuilder.setFormat(AudioFormat::Float);
        Result r = mBuilder.openStream(mStream);
        ASSERT_EQ(r, Result::ErrorInvalidFormat) << convertToText(r);
        ASSERT_TRUE(closeStream());
    }
}

TEST_F(StreamOpenInput, RecordingFormatFloatReturnsFloatOnMarshmallowAndLater){

    if (getSdkVersion() >= __ANDROID_API_M__){
        mBuilder.setDirection(Direction::Input);
        mBuilder.setFormat(AudioFormat::Float);
        ASSERT_TRUE(openStream());
        ASSERT_EQ(mStream->getFormat(), AudioFormat::Float);
        ASSERT_TRUE(closeStream());
    }
}

TEST_F(StreamOpenInput, RecordingFormatI16ReturnsI16){

    mBuilder.setDirection(Direction::Input);
    mBuilder.setFormat(AudioFormat::I16);
    ASSERT_TRUE(openStream());
    ASSERT_EQ(mStream->getFormat(), AudioFormat::I16);
    ASSERT_TRUE(closeStream());
}

TEST_F(StreamOpenOutput, PlaybackFormatUnspecifiedReturnsI16BeforeLollipop){

    if (getSdkVersion() < __ANDROID_API_L__){
        mBuilder.setDirection(Direction::Output);
        mBuilder.setFormat(AudioFormat::Unspecified);
        ASSERT_TRUE(openStream());
        ASSERT_EQ(mStream->getFormat(), AudioFormat::I16);
        ASSERT_TRUE(closeStream());
    }
}

TEST_F(StreamOpenOutput, PlaybackFormatUnspecifiedReturnsFloatOnLollipopAndLater){

    if (getSdkVersion() >= __ANDROID_API_L__){
        mBuilder.setDirection(Direction::Output);
        mBuilder.setFormat(AudioFormat::Unspecified);
        ASSERT_TRUE(openStream());
        ASSERT_EQ(mStream->getFormat(), AudioFormat::Float);
        ASSERT_TRUE(closeStream());
    }
}

TEST_F(StreamOpenOutput, PlaybackFormatFloatReturnsErrorBeforeLollipop){

    if (getSdkVersion() < __ANDROID_API_L__){
        mBuilder.setDirection(Direction::Output);
        mBuilder.setFormat(AudioFormat::Float);
        Result r = mBuilder.openStream(mStream);
        ASSERT_EQ(r, Result::ErrorInvalidFormat);
        ASSERT_TRUE(closeStream());
    }
}

TEST_F(StreamOpenOutput, PlaybackFormatFloatReturnsFloatWithFormatConversionAllowed){
    mBuilder.setDirection(Direction::Output);
    mBuilder.setFormat(AudioFormat::Float);
    mBuilder.setFormatConversionAllowed(true);
    ASSERT_TRUE(openStream());
    ASSERT_EQ(mStream->getFormat(), AudioFormat::Float);
    ASSERT_TRUE(closeStream());
}

TEST_F(StreamOpenOutput, PlaybackFormatFloatReturnsFloatOnLollipopAndLater){

    if (getSdkVersion() >= __ANDROID_API_L__){
        mBuilder.setDirection(Direction::Output);
        mBuilder.setFormat(AudioFormat::Float);
        ASSERT_TRUE(openStream());
        ASSERT_EQ(mStream->getFormat(), AudioFormat::Float);
        ASSERT_TRUE(closeStream());
    }
}

TEST_F(StreamOpenOutput, PlaybackFormatI16ReturnsI16) {
    mBuilder.setDirection(Direction::Output);
    mBuilder.setFormat(AudioFormat::I16);
    ASSERT_TRUE(openStream());
    ASSERT_EQ(mStream->getFormat(), AudioFormat::I16);
    ASSERT_TRUE(closeStream());
}

TEST_F(StreamOpenOutput, OpenCloseLowLatencyStream){
    mBuilder.setDirection(Direction::Output);
    mBuilder.setPerformanceMode(PerformanceMode::LowLatency);
    float *buf = new float[100];
    ASSERT_TRUE(openStream());
    delete[] buf;
    ASSERT_TRUE(closeStream());
}

TEST_F(StreamOpenOutput, LowLatencyStreamHasSmallBufferSize){

    if (mBuilder.isAAudioRecommended()) {
        mBuilder.setDirection(Direction::Output);
        mBuilder.setPerformanceMode(PerformanceMode::LowLatency);
        ASSERT_TRUE(openStream());
        int32_t bufferSize = mStream->getBufferSizeInFrames();
        int32_t burst = mStream->getFramesPerBurst();
        ASSERT_TRUE(closeStream());
        ASSERT_LE(bufferSize, burst * 3);
    }
}

// Make sure the parameters get copied from the child stream.
TEST_F(StreamOpenOutput, AAudioOutputSampleRate44100FilterConfiguration) {
    if (mBuilder.isAAudioRecommended()) {
        mBuilder.setDirection(Direction::Output);
        mBuilder.setPerformanceMode(PerformanceMode::LowLatency);
        mBuilder.setSharingMode(SharingMode::Exclusive);
        // Try to force the use of a FilterAudioStream by requesting conversion.
        mBuilder.setSampleRate(44100);
        mBuilder.setSampleRateConversionQuality(SampleRateConversionQuality::Medium);
        ASSERT_TRUE(openStream());
        if (getSdkVersion() >= __ANDROID_API_U__) {
            ASSERT_LT(0, mStream->getHardwareSampleRate());
            ASSERT_LT(0, mStream->getHardwareChannelCount());
            ASSERT_LT(0, (int)mStream->getHardwareFormat());
        }
        // If MMAP is not supported then we cannot get an EXCLUSIVE mode stream.
        if (!AAudioExtensions::getInstance().isMMapSupported()) {
            ASSERT_NE(SharingMode::Exclusive, mStream->getSharingMode()); // IMPOSSIBLE
        }
        ASSERT_TRUE(closeStream());
    }
}

// See if sample rate conversion by Oboe is calling the callback.
TEST_F(StreamOpenOutput, AAudioOutputSampleRate44100) {
    checkSampleRateConversionAdvancing(Direction::Output);
}

// See if sample rate conversion by Oboe is calling the callback.
TEST_F(StreamOpenInput, AAudioInputSampleRate44100) {
    checkSampleRateConversionAdvancing(Direction::Input);
}

TEST_F(StreamOpenOutput, AAudioOutputSetPackageName){
    if (getSdkVersion() >= __ANDROID_API_S__){
        mBuilder.setAudioApi(AudioApi::AAudio);
        mBuilder.setPackageName("com.google.oboe.tests.unittestrunner");
        ASSERT_TRUE(openStream());
        ASSERT_EQ(mStream->requestStart(), Result::OK);
        ASSERT_TRUE(closeStream());
    }
}

TEST_F(StreamOpenInput, AAudioInputSetPackageName){
    if (getSdkVersion() >= __ANDROID_API_S__){
        mBuilder.setDirection(Direction::Input);
        mBuilder.setAudioApi(AudioApi::AAudio);
        mBuilder.setPackageName("com.google.oboe.tests.unittestrunner");
        ASSERT_TRUE(openStream());
        ASSERT_EQ(mStream->requestStart(), Result::OK);
        ASSERT_TRUE(closeStream());
    }
}

TEST_F(StreamOpenInput, AAudioInputSetPackageNameInvalid){
    if (getSdkVersion() >= __ANDROID_API_S__){
        mBuilder.setDirection(Direction::Input);
        mBuilder.setAudioApi(AudioApi::AAudio);
        mBuilder.setPackageName("com.google.oboe.tests.unittestrunnerinvalid");
        ASSERT_TRUE(openStream());
        ASSERT_NE(mStream->requestStart(), Result::OK);
        ASSERT_TRUE(closeStream());
    }
}

TEST_F(StreamOpenOutput, AAudioOutputSetAttributionTag){
    if (getSdkVersion() >= __ANDROID_API_S__){
        mBuilder.setAudioApi(AudioApi::AAudio);
        mBuilder.setAttributionTag("TestSetOutputAttributionTag");
        ASSERT_TRUE(openStream());
        ASSERT_EQ(mStream->requestStart(), Result::OK);
        ASSERT_TRUE(closeStream());
    }
}

TEST_F(StreamOpenInput, AAudioInputSetAttributionTag){
    if (getSdkVersion() >= __ANDROID_API_S__){
        mBuilder.setDirection(Direction::Input);
        mBuilder.setAudioApi(AudioApi::AAudio);
        mBuilder.setAttributionTag("TestSetInputAttributionTag");
        ASSERT_TRUE(openStream());
        ASSERT_EQ(mStream->requestStart(), Result::OK);
        ASSERT_TRUE(closeStream());
    }
}

TEST_F(StreamOpenInput, AAudioInputSetSpatializationBehavior) {
    mBuilder.setDirection(Direction::Input);
    mBuilder.setSpatializationBehavior(SpatializationBehavior::Auto);
    ASSERT_TRUE(openStream());
    if (getSdkVersion() >= __ANDROID_API_S_V2__){
        ASSERT_EQ(mStream->getSpatializationBehavior(), SpatializationBehavior::Auto);
    } else {
        ASSERT_EQ(mStream->getSpatializationBehavior(), SpatializationBehavior::Never);
    }
    ASSERT_TRUE(closeStream());
}

TEST_F(StreamOpenOutput, AAudioOutputSetSpatializationBehavior) {
    mBuilder.setDirection(Direction::Output);
    mBuilder.setSpatializationBehavior(SpatializationBehavior::Never);
    ASSERT_TRUE(openStream());
    ASSERT_EQ(mStream->getSpatializationBehavior(), SpatializationBehavior::Never);
    ASSERT_TRUE(closeStream());
}

TEST_F(StreamOpenOutput, OpenSLESOutputSetSpatializationBehavior) {
    mBuilder.setDirection(Direction::Output);
    mBuilder.setAudioApi(AudioApi::OpenSLES);
    mBuilder.setSpatializationBehavior(SpatializationBehavior::Auto);
    ASSERT_TRUE(openStream());
    ASSERT_EQ(mStream->getSpatializationBehavior(), SpatializationBehavior::Never);
    ASSERT_TRUE(closeStream());
}

TEST_F(StreamOpenInput, AAudioInputSetSpatializationBehaviorUnspecified) {
    mBuilder.setDirection(Direction::Input);
    mBuilder.setSpatializationBehavior(SpatializationBehavior::Unspecified);
    ASSERT_TRUE(openStream());
    ASSERT_EQ(mStream->getSpatializationBehavior(), SpatializationBehavior::Never);
    ASSERT_TRUE(closeStream());
}

TEST_F(StreamOpenOutput, AAudioOutputSetSpatializationBehaviorUnspecified) {
    mBuilder.setDirection(Direction::Output);
    mBuilder.setSpatializationBehavior(SpatializationBehavior::Unspecified);
    ASSERT_TRUE(openStream());
    ASSERT_EQ(mStream->getSpatializationBehavior(), SpatializationBehavior::Never);
    ASSERT_TRUE(closeStream());
}

TEST_F(StreamOpenInput, AAudioInputSetIsContentSpatialized) {
    mBuilder.setDirection(Direction::Input);
    mBuilder.setIsContentSpatialized(true);
    ASSERT_TRUE(openStream());
    ASSERT_EQ(mStream->isContentSpatialized(), true);
    ASSERT_TRUE(closeStream());
}

TEST_F(StreamOpenOutput, AAudioOutputSetIsContentSpatialized) {
    mBuilder.setDirection(Direction::Output);
    mBuilder.setIsContentSpatialized(true);
    ASSERT_TRUE(openStream());
    ASSERT_EQ(mStream->isContentSpatialized(), true);
    ASSERT_TRUE(closeStream());
}

TEST_F(StreamOpenOutput, OpenSLESOutputSetIsContentSpatialized) {
    mBuilder.setDirection(Direction::Output);
    mBuilder.setAudioApi(AudioApi::OpenSLES);
    mBuilder.setIsContentSpatialized(true);
    ASSERT_TRUE(openStream());
    ASSERT_EQ(mStream->isContentSpatialized(), true);
    ASSERT_TRUE(closeStream());
}

TEST_F(StreamOpenOutput, AAudioOutputSetIsContentSpatializedFalse) {
    mBuilder.setDirection(Direction::Output);
    mBuilder.setIsContentSpatialized(false);
    ASSERT_TRUE(openStream());
    ASSERT_EQ(mStream->isContentSpatialized(), false);
    ASSERT_TRUE(closeStream());
}

TEST_F(StreamOpenOutput, AAudioOutputSetIsContentSpatializedUnspecified) {
    mBuilder.setDirection(Direction::Output);
    ASSERT_TRUE(openStream());
    ASSERT_EQ(mStream->isContentSpatialized(), false);
    ASSERT_TRUE(closeStream());
}

TEST_F(StreamOpenInput, AAudioInputSetIsContentSpatializedUnspecified) {
    mBuilder.setDirection(Direction::Input);
    ASSERT_TRUE(openStream());
    ASSERT_EQ(mStream->isContentSpatialized(), false);
    ASSERT_TRUE(closeStream());
}

TEST_F(StreamOpenOutput, OutputForOpenSLESPerformanceModeNoneGetBufferSizeInFrames){
    mBuilder.setPerformanceMode(PerformanceMode::None);
    mBuilder.setAudioApi(AudioApi::OpenSLES);
    ASSERT_TRUE(openStream());
    EXPECT_GT(mStream->getBufferSizeInFrames(), 0);
    ASSERT_TRUE(closeStream());
}

TEST_F(StreamOpenOutput, OboeExtensions){
    if (OboeExtensions::isMMapSupported()) {
        ASSERT_EQ(OboeExtensions::setMMapEnabled(true), 0);
        ASSERT_TRUE(OboeExtensions::isMMapEnabled());

        ASSERT_EQ(OboeExtensions::setMMapEnabled(false), 0);
        ASSERT_FALSE(OboeExtensions::isMMapEnabled());
        ASSERT_TRUE(openStream());
        EXPECT_FALSE(OboeExtensions::isMMapUsed(mStream.get()));
        ASSERT_TRUE(closeStream());

        ASSERT_EQ(OboeExtensions::setMMapEnabled(true), 0);
        ASSERT_TRUE(OboeExtensions::isMMapEnabled());
    }
}

TEST_F(StreamOpenInput, AAudioInputSetPrivacySensitiveModeUnspecifiedUnprocessed){
    if (getSdkVersion() >= __ANDROID_API_R__){
        mBuilder.setDirection(Direction::Input);
        mBuilder.setAudioApi(AudioApi::AAudio);
        mBuilder.setInputPreset(InputPreset::Unprocessed);
        ASSERT_TRUE(openStream());
        ASSERT_EQ(mStream->getPrivacySensitiveMode(), PrivacySensitiveMode::Disabled);
        ASSERT_TRUE(closeStream());
    }
}

TEST_F(StreamOpenInput, AAudioInputSetPrivacySensitiveModeUnspecifiedVoiceCommunication){
    if (getSdkVersion() >= __ANDROID_API_R__){
        mBuilder.setDirection(Direction::Input);
        mBuilder.setAudioApi(AudioApi::AAudio);
        mBuilder.setInputPreset(InputPreset::VoiceCommunication);
        ASSERT_TRUE(openStream());
        ASSERT_EQ(mStream->getPrivacySensitiveMode(), PrivacySensitiveMode::Enabled);
        ASSERT_TRUE(closeStream());
    }
}

TEST_F(StreamOpenInput, AAudioInputSetPrivacySensitiveModeVoiceDisabled){
    if (getSdkVersion() >= __ANDROID_API_R__){
        mBuilder.setDirection(Direction::Input);
        mBuilder.setAudioApi(AudioApi::AAudio);
        mBuilder.setInputPreset(InputPreset::VoiceCommunication);
        mBuilder.setPrivacySensitiveMode(PrivacySensitiveMode::Disabled);
        ASSERT_TRUE(openStream());
        ASSERT_EQ(mStream->getPrivacySensitiveMode(), PrivacySensitiveMode::Disabled);
        ASSERT_TRUE(closeStream());
    }
}

TEST_F(StreamOpenInput, AAudioInputSetPrivacySensitiveModeUnprocessedEnabled){
    if (getSdkVersion() >= __ANDROID_API_R__){
        mBuilder.setDirection(Direction::Input);
        mBuilder.setAudioApi(AudioApi::AAudio);
        mBuilder.setInputPreset(InputPreset::Unprocessed);
        mBuilder.setPrivacySensitiveMode(PrivacySensitiveMode::Enabled);
        ASSERT_TRUE(openStream());
        ASSERT_EQ(mStream->getPrivacySensitiveMode(), PrivacySensitiveMode::Enabled);
        ASSERT_TRUE(closeStream());
    }
}

TEST_F(StreamOpenOutput, AAudioOutputSetPrivacySensitiveModeGetsUnspecified){
    if (getSdkVersion() >= __ANDROID_API_R__){
        mBuilder.setDirection(Direction::Output);
        mBuilder.setAudioApi(AudioApi::AAudio);
        mBuilder.setPrivacySensitiveMode(PrivacySensitiveMode::Enabled);
        ASSERT_TRUE(openStream());
        ASSERT_EQ(mStream->getPrivacySensitiveMode(), PrivacySensitiveMode::Unspecified);
        ASSERT_TRUE(closeStream());
    }
}

TEST_F(StreamOpenInput, OpenSLESInputSetPrivacySensitiveModeDoesNotCrash){
    mBuilder.setDirection(Direction::Input);
    mBuilder.setAudioApi(AudioApi::OpenSLES);
    mBuilder.setInputPreset(InputPreset::Unprocessed);
    mBuilder.setPrivacySensitiveMode(PrivacySensitiveMode::Enabled);
    ASSERT_TRUE(openStream());
    ASSERT_EQ(mStream->getPrivacySensitiveMode(), PrivacySensitiveMode::Unspecified);
    ASSERT_TRUE(closeStream());
}

TEST_F(StreamOpenInput, OldAndroidVersionInputSetPrivacySensitiveModeDoesNotCrash){
    if (getSdkVersion() < __ANDROID_API_R__) {
        mBuilder.setDirection(Direction::Input);
        mBuilder.setInputPreset(InputPreset::Unprocessed);
        mBuilder.setPrivacySensitiveMode(PrivacySensitiveMode::Enabled);
        ASSERT_TRUE(openStream());
        ASSERT_EQ(mStream->getPrivacySensitiveMode(), PrivacySensitiveMode::Unspecified);
        ASSERT_TRUE(closeStream());
    }
}

TEST_F(StreamOpenOutput, AAudioOutputSetAllowedCapturePolicyUnspecifiedGetsAll){
    if (getSdkVersion() >= __ANDROID_API_Q__){
        mBuilder.setDirection(Direction::Output);
        mBuilder.setAudioApi(AudioApi::AAudio);
        mBuilder.setAllowedCapturePolicy(AllowedCapturePolicy::Unspecified);
        ASSERT_TRUE(openStream());
        ASSERT_EQ(mStream->getAllowedCapturePolicy(), AllowedCapturePolicy::All);
        ASSERT_TRUE(closeStream());
    }
}

TEST_F(StreamOpenOutput, AAudioOutputSetAllowedCapturePolicyAll){
    if (getSdkVersion() >= __ANDROID_API_Q__){
        mBuilder.setDirection(Direction::Output);
        mBuilder.setAudioApi(AudioApi::AAudio);
        mBuilder.setAllowedCapturePolicy(AllowedCapturePolicy::All);
        ASSERT_TRUE(openStream());
        ASSERT_EQ(mStream->getAllowedCapturePolicy(), AllowedCapturePolicy::All);
        ASSERT_TRUE(closeStream());
    }
}

TEST_F(StreamOpenOutput, AAudioOutputSetAllowedCapturePolicySystem){
    if (getSdkVersion() >= __ANDROID_API_Q__){
        mBuilder.setDirection(Direction::Output);
        mBuilder.setAudioApi(AudioApi::AAudio);
        mBuilder.setAllowedCapturePolicy(AllowedCapturePolicy::System);
        ASSERT_TRUE(openStream());
        ASSERT_EQ(mStream->getAllowedCapturePolicy(), AllowedCapturePolicy::System);
        ASSERT_TRUE(closeStream());
    }
}

TEST_F(StreamOpenOutput, AAudioOutputSetAllowedCapturePolicyNone){
    if (getSdkVersion() >= __ANDROID_API_Q__){
        mBuilder.setDirection(Direction::Output);
        mBuilder.setAudioApi(AudioApi::AAudio);
        mBuilder.setAllowedCapturePolicy(AllowedCapturePolicy::None);
        ASSERT_TRUE(openStream());
        ASSERT_EQ(mStream->getAllowedCapturePolicy(), AllowedCapturePolicy::None);
        ASSERT_TRUE(closeStream());
    }
}

TEST_F(StreamOpenOutput, AAudioOutputDoNotSetAllowedCapturePolicy){
    mBuilder.setDirection(Direction::Output);
    mBuilder.setAudioApi(AudioApi::AAudio);
    ASSERT_TRUE(openStream());
    if (getSdkVersion() >= __ANDROID_API_Q__){
        ASSERT_EQ(mStream->getAllowedCapturePolicy(), AllowedCapturePolicy::All);
    } else {
        ASSERT_EQ(mStream->getAllowedCapturePolicy(), AllowedCapturePolicy::Unspecified);
    }
    ASSERT_TRUE(closeStream());
}

TEST_F(StreamOpenOutput, OpenSLESOutputSetAllowedCapturePolicyAllGetsUnspecified){
    mBuilder.setDirection(Direction::Output);
    mBuilder.setAudioApi(AudioApi::OpenSLES);
    mBuilder.setAllowedCapturePolicy(AllowedCapturePolicy::All);
    ASSERT_TRUE(openStream());
    ASSERT_EQ(mStream->getAllowedCapturePolicy(), AllowedCapturePolicy::Unspecified);
    ASSERT_TRUE(closeStream());
}

TEST_F(StreamOpenOutput, AAudioBeforeQOutputSetAllowedCapturePolicyAllGetsUnspecified){
    if (getSdkVersion() < __ANDROID_API_Q__){
        mBuilder.setDirection(Direction::Output);
        mBuilder.setAudioApi(AudioApi::AAudio);
        mBuilder.setAllowedCapturePolicy(AllowedCapturePolicy::All);
        ASSERT_TRUE(openStream());
        ASSERT_EQ(mStream->getAllowedCapturePolicy(), AllowedCapturePolicy::Unspecified);
        ASSERT_TRUE(closeStream());
    }
}

TEST_F(StreamOpenInput, AAudioInputSetAllowedCapturePolicyAllGetsUnspecified){
    mBuilder.setDirection(Direction::Input);
    mBuilder.setAllowedCapturePolicy(AllowedCapturePolicy::All);
    ASSERT_TRUE(openStream());
    ASSERT_EQ(mStream->getAllowedCapturePolicy(), AllowedCapturePolicy::Unspecified);
    ASSERT_TRUE(closeStream());
}
