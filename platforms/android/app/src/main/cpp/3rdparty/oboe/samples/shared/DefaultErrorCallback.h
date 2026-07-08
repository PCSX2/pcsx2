/*
 * Copyright 2020 The Android Open Source Project
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

#ifndef SAMPLES_DEFAULT_ERROR_CALLBACK_H
#define SAMPLES_DEFAULT_ERROR_CALLBACK_H

#include <vector>
#include <oboe/AudioStreamCallback.h>
#include <logging_macros.h>

#include "IRestartable.h"

/**
 * This is a callback object which will be called when a stream error occurs.
 *
 * It is constructed using an `IRestartable` which allows it to automatically restart the parent
 * object if the stream is disconnected (for example, when headphones are attached).
 *
 * @param IRestartable - the object which should be restarted when the stream is disconnected
 */
class DefaultErrorCallback : public oboe::AudioStreamErrorCallback {
public:

    DefaultErrorCallback(IRestartable &parent): mParent(parent) {}
    virtual ~DefaultErrorCallback() = default;

    virtual void onErrorAfterClose(oboe::AudioStream *oboeStream, oboe::Result error) override {
        // Restart the stream if the error is a disconnect, otherwise do nothing and log the error
        // reason.
        if (error == oboe::Result::ErrorDisconnected) {
            LOGI("Restarting AudioStream");
            mParent.restart();
        }
        LOGE("Error was %s", oboe::convertToText(error));
    }

private:
    IRestartable &mParent;

};


#endif //SAMPLES_DEFAULT_ERROR_CALLBACK_H
