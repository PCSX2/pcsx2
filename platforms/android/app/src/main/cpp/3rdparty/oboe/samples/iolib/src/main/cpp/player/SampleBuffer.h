/*
 * Copyright (C) 2020 The Android Open Source Project
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

#ifndef _PLAYER_SAMPLEBUFFER_
#define _PLAYER_SAMPLEBUFFER_

#include <wav/WavStreamReader.h>

namespace iolib {

/*
 * Defines the relevant properties of the audio data being sourced.
 */
struct AudioProperties {
    int32_t channelCount;
    int32_t sampleRate;
};

class SampleBuffer {
public:
    SampleBuffer() : mNumSamples(0) {};
    ~SampleBuffer() { unloadSampleData(); }

    // Data load/unload
    void loadSampleData(parselib::WavStreamReader* reader);
    void unloadSampleData();

    void resampleData(int sampleRate);

    virtual AudioProperties getProperties() const { return mAudioProperties; }

    float* getSampleData() { return mSampleData; }
    int32_t getNumSamples() { return mNumSamples; }

protected:
    AudioProperties mAudioProperties;

    float*  mSampleData;
    int32_t mNumSamples;
};

}

#endif //_PLAYER_SAMPLEBUFFER_
