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

#ifndef OBOETESTER_TEST_ERROR_CALLBACK_H
#define OBOETESTER_TEST_ERROR_CALLBACK_H

#include "common/OboeDebug.h"
#include "oboe/Oboe.h"
#include <thread>

/**
 * This code is an experiment to see if we can cause a crash from the ErrorCallback.
 */
class TestErrorCallback {
public:

    oboe::Result open();
    oboe::Result start();
    oboe::Result stop();
    oboe::Result close();

    int test();

    int32_t getCallbackMagic() {
        return mCallbackMagic.load();
    }

protected:

    std::atomic<int32_t> mCallbackMagic{0};

private:

    void cleanup() {
        mDataCallback.reset();
        mErrorCallback.reset();
        mStream.reset();
    }

    class MyDataCallback : public oboe::AudioStreamDataCallback {    public:

        oboe::DataCallbackResult onAudioReady(
                oboe::AudioStream *audioStream,
                void *audioData,
                int32_t numFrames) override;

    };

    class MyErrorCallback : public oboe::AudioStreamErrorCallback {
    public:

        MyErrorCallback(TestErrorCallback *parent): mParent(parent) {}

        virtual ~MyErrorCallback() {
            // If the delete occurs before onErrorAfterClose() then this bad magic
            // value will be seen by the Java test code, causing a failure.
            // It is also possible that this code will just cause OboeTester to crash!
            mMagic = 0xdeadbeef;
            LOGE("%s() called", __func__);
        }

        void onErrorBeforeClose(oboe::AudioStream *oboeStream, oboe::Result error) override {
            LOGE("%s() - error = %s, parent = %p",
                 __func__, oboe::convertToText(error), &mParent);
            // Trigger a crash by "deleting" this callback object while in use!
            // Do not try this at home. We are just trying to reproduce the crash
            // reported in #1603.
            std::thread t([this]() {
                    this->mParent->cleanup(); // Possibly delete stream and callback objects.
                    LOGE("onErrorBeforeClose called cleanup!");
                });
            t.detach();
            // There is a race condition between the deleting thread and this thread.
            // We do not want to add synchronization because the object is getting deleted
            // and cannot be relied on.
            // So we sleep here to give the deleting thread a chance to win the race.
            usleep(10 * 1000);
        }

        void onErrorAfterClose(oboe::AudioStream *oboeStream, oboe::Result error) override {
            // The callback was probably deleted by now.
            LOGE("%s() - error = %s, mMagic = 0x%08X",
                 __func__, oboe::convertToText(error), mMagic.load());
            mParent->mCallbackMagic = mMagic.load();
        }

    private:
        TestErrorCallback *mParent;
        // This must match the value in TestErrorCallbackActivity.java
        static constexpr int32_t kMagicGood = 0x600DCAFE;
        std::atomic<int32_t> mMagic{kMagicGood};
    };

    std::shared_ptr<oboe::AudioStream> mStream;
    std::shared_ptr<MyDataCallback> mDataCallback;
    std::shared_ptr<MyErrorCallback> mErrorCallback;

    static constexpr int kChannelCount = 2;
};

#endif //OBOETESTER_TEST_ERROR_CALLBACK_H
