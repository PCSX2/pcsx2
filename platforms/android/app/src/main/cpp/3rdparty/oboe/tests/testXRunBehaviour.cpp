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


class MyCallback : public AudioStreamCallback {
public:
    DataCallbackResult onAudioReady(AudioStream *oboeStream, void *audioData, int32_t numFrames) override {
        return DataCallbackResult::Continue;
    }
};

class XRunBehaviour : public ::testing::Test {

protected:

    bool openStream() {
        Result r = mBuilder.openStream(mStream);
        EXPECT_EQ(r, Result::OK) << "Failed to open stream " << convertToText(r);
        return (r == Result::OK);
    }

    bool closeStream() {
        Result r = mStream->close();
        EXPECT_EQ(r, Result::OK) << "Failed to close stream. " << convertToText(r);
        return (r == Result::OK);
    }

    AudioStreamBuilder mBuilder;
    std::shared_ptr<AudioStream> mStream;

};

// TODO figure out this behaviour - On OpenSLES xRuns are supported within AudioStreamBuffered,
//  however, these aren't the same as the actual stream underruns
TEST_F(XRunBehaviour, SupportedWhenStreamIsUsingAAudio){

    ASSERT_TRUE(openStream());
    if (mStream->getAudioApi() == AudioApi::AAudio){
        ASSERT_TRUE(mStream->isXRunCountSupported());
    }
    ASSERT_TRUE(closeStream());
}

TEST_F(XRunBehaviour, NotSupportedOnOpenSLESWhenStreamIsUsingCallback){

    MyCallback callback;
    mBuilder.setCallback(&callback);
    ASSERT_TRUE(openStream());
    if (mStream->getAudioApi() == AudioApi::OpenSLES){
        ASSERT_FALSE(mStream->isXRunCountSupported());
    }
    ASSERT_TRUE(closeStream());
}

TEST_F(XRunBehaviour, SupportedOnOpenSLESWhenStreamIsUsingBlockingIO){

    ASSERT_TRUE(openStream());
    if (mStream->getAudioApi() == AudioApi::OpenSLES){
        ASSERT_TRUE(mStream->isXRunCountSupported());
    }
    ASSERT_TRUE(closeStream());
}
