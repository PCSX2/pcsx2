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
#include "WavRIFFChunkHeader.h"
#include "stream/InputStream.h"

namespace parselib {

const RiffID WavRIFFChunkHeader::RIFFID_RIFF = makeRiffID('R', 'I', 'F', 'F');
const RiffID WavRIFFChunkHeader::RIFFID_WAVE = makeRiffID('W', 'A', 'V', 'E');

WavRIFFChunkHeader::WavRIFFChunkHeader() : WavChunkHeader(RIFFID_RIFF) {
    mFormatId = RIFFID_WAVE;
}

WavRIFFChunkHeader::WavRIFFChunkHeader(RiffID tag) : WavChunkHeader(tag) {
    mFormatId = RIFFID_WAVE;
}

void WavRIFFChunkHeader::read(InputStream *stream) {
    WavChunkHeader::read(stream);
    stream->read(&mFormatId, sizeof(mFormatId));
}

} // namespace parselib
