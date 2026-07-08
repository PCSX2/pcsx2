/*
 * Copyright 2017 The Android Open Source Project
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

package com.mobileer.oboetester;

import java.io.IOException;

class AudioStreamTester {
    protected AudioStreamBase mCurrentAudioStream;
    StreamConfiguration  requestedConfiguration = new StreamConfiguration();
    StreamConfiguration  actualConfiguration = new StreamConfiguration();

    AudioStreamBase getCurrentAudioStream() {
        return mCurrentAudioStream;
    }

    public void open() throws IOException {
        mCurrentAudioStream.open(requestedConfiguration, actualConfiguration,
                -1);
    }

    public void reset() {
        setWorkload(0);
        requestedConfiguration.reset(); // TODO consider making new ones
        actualConfiguration.reset();
    }

    public void close() {
        mCurrentAudioStream.close();
    }

    public void startPlayback() throws IOException {
        mCurrentAudioStream.startPlayback();
    }

    public void setWorkload(int workload) {
        mCurrentAudioStream.setWorkload(workload);
    }
}
