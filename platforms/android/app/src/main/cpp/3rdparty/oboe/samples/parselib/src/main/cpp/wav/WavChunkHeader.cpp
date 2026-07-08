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
#include "stream/InputStream.h"

#include "WavChunkHeader.h"

namespace parselib {

const RiffID WavChunkHeader::RIFFID_DATA = makeRiffID('d', 'a', 't', 'a');

void WavChunkHeader::read(InputStream *stream) {
    stream->read(&mChunkId, sizeof(mChunkId));
    stream->read(&mChunkSize, sizeof(mChunkSize));
}

} // namespace parselib
