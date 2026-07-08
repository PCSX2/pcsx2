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
#include <android/log.h>

#include "stream/InputStream.h"

#include "WavFmtChunkHeader.h"

static const char *TAG = "WavFmtChunkHeader";

namespace parselib {

const RiffID WavFmtChunkHeader::RIFFID_FMT = makeRiffID('f', 'm', 't', ' ');

WavFmtChunkHeader::WavFmtChunkHeader() : WavChunkHeader(RIFFID_FMT) {
    mEncodingId = ENCODING_PCM;
    mNumChannels = 0;
    mSampleRate = 0;
    mAveBytesPerSecond = 0;
    mBlockAlign = 0;
    mSampleSize = 0;
    mExtraBytes = 0;
}

WavFmtChunkHeader::WavFmtChunkHeader(RiffID tag) : WavChunkHeader(tag) {
    mEncodingId = ENCODING_PCM;
    mNumChannels = 0;
    mSampleRate = 0;
    mAveBytesPerSecond = 0;
    mBlockAlign = 0;
    mSampleSize = 0;
    mExtraBytes = 0;
}

void WavFmtChunkHeader::normalize() {
    if (mEncodingId == ENCODING_PCM || mEncodingId == ENCODING_IEEE_FLOAT) {
        mBlockAlign = (short) (mNumChannels * (mSampleSize / 8));
        mAveBytesPerSecond = mSampleRate * mBlockAlign;
        mExtraBytes = 0;
    } else {
        //hmmm....
    }
}

void WavFmtChunkHeader::read(InputStream *stream) {
    WavChunkHeader::read(stream);
    stream->read(&mEncodingId, sizeof(mEncodingId));
    stream->read(&mNumChannels, sizeof(mNumChannels));
    stream->read(&mSampleRate, sizeof(mSampleRate));
    stream->read(&mAveBytesPerSecond, sizeof(mAveBytesPerSecond));
    stream->read(&mBlockAlign, sizeof(mBlockAlign));
    stream->read(&mSampleSize, sizeof(mSampleSize));

    if (mEncodingId != ENCODING_PCM && mEncodingId != ENCODING_IEEE_FLOAT) {
        // only read this if NOT PCM
        stream->read(&mExtraBytes, sizeof(mExtraBytes));
    } else {
        mExtraBytes = (short) (mChunkSize - 16);
    }
}

} // namespace parselib
