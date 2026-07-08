/*
 * Copyright 2019 The Android Open Source Project
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
#ifndef _IO_WAV_WAVSTREAMREADER_H_
#define _IO_WAV_WAVSTREAMREADER_H_

#include <map>

#include "AudioEncoding.h"
#include "WavRIFFChunkHeader.h"
#include "WavFmtChunkHeader.h"

/*
 * WAV format documentation can be found:
 * http://soundfile.sapp.org/doc/WaveFormat/
 * https://web.archive.org/web/20090417165828/http://www.kk.iij4u.or.jp/~kondo/wave/mpidata.txt
 */
namespace parselib {

class InputStream;

class WavStreamReader {
public:
    WavStreamReader(InputStream *stream);

    int getSampleRate() { return mFmtChunk->mSampleRate; }

    int getNumSampleFrames() {
        return mDataChunk->mChunkSize / (mFmtChunk->mSampleSize / 8) / mFmtChunk->mNumChannels;
    }

    int getNumChannels() { return mFmtChunk != 0 ? mFmtChunk->mNumChannels : 0; }

    int getSampleEncoding();

    int getBitsPerSample() { return mFmtChunk->mSampleSize; }

    void parse();

    // Data access
    void positionToAudio();

    static constexpr int ERR_INVALID_FORMAT    = -1;
    static constexpr int ERR_INVALID_STATE    = -2;

    int getDataFloat(float *buff, int numFrames);

    // int getData16(short *buff, int numFramees);

protected:
    InputStream *mStream;

    std::shared_ptr<WavRIFFChunkHeader> mWavChunk;
    std::shared_ptr<WavFmtChunkHeader> mFmtChunk;
    std::shared_ptr<WavChunkHeader> mDataChunk;

    long mAudioDataStartPos;

    std::map<RiffID, std::shared_ptr<WavChunkHeader>> mChunkMap;

private:
    /*
     * Individual Format Readers/Converters
     */
    int getDataFloat_PCM8(float *buff, int numFrames);

    int getDataFloat_PCM16(float *buff, int numFrames);

    int getDataFloat_PCM24(float *buff, int numFrames);

    int getDataFloat_Float32(float *buff, int numFrames);
    int getDataFloat_PCM32(float *buff, int numFrames);
};

} // namespace parselib

#endif // _IO_WAV_WAVSTREAMREADER_H_
