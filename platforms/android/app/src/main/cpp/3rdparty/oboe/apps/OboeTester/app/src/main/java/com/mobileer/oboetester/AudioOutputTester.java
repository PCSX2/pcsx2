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

import android.util.Log;

public class AudioOutputTester extends AudioStreamTester {

    protected OboeAudioOutputStream mOboeAudioOutputStream;

    private static AudioOutputTester mInstance;

    public static synchronized AudioOutputTester getInstance() {
        if (mInstance == null) {
            mInstance = new AudioOutputTester();
        }
        return mInstance;
    }

    private AudioOutputTester() {
        super();
        Log.i(TapToToneActivity.TAG, "create OboeAudioOutputStream ---------");
        mOboeAudioOutputStream = new OboeAudioOutputStream();
        mCurrentAudioStream = mOboeAudioOutputStream;
        requestedConfiguration.setDirection(StreamConfiguration.DIRECTION_OUTPUT);
    }

    public void trigger() {
        mOboeAudioOutputStream.trigger();
    }

    public void setChannelEnabled(int channelIndex, boolean enabled)  {
        mOboeAudioOutputStream.setChannelEnabled(channelIndex, enabled);
    }

    public void setSignalType(int type) {
        mOboeAudioOutputStream.setSignalType(type);
    }

    public void setAmplitude(float amplitude) {
        mOboeAudioOutputStream.setAmplitude(amplitude);
    }

    public int getLastErrorCallbackResult() {return mOboeAudioOutputStream.getLastErrorCallbackResult();};

    public long getFramesRead() {return mOboeAudioOutputStream.getFramesRead();};
}
