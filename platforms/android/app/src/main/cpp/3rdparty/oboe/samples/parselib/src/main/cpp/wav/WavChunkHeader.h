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
#ifndef _IO_WAV_WAVCHUNKHEADER_H_
#define _IO_WAV_WAVCHUNKHEADER_H_

#include "WavTypes.h"

namespace parselib {

class InputStream;

/**
 * Superclass for all RIFF chunks. Handles the chunk ID and chunk size.
 * Concrete subclasses include chunks for 'RIFF' and 'fmt ' chunks.
 */
class WavChunkHeader {
public:
    static const RiffID RIFFID_DATA;

    RiffID mChunkId;
    RiffInt32 mChunkSize;

    WavChunkHeader() : mChunkId(0), mChunkSize(0) {}

    WavChunkHeader(RiffID chunkId) : mChunkId(chunkId), mChunkSize(0) {}

    /**
     * Reads the contents of the chunk. In this class, just the ID and size fields.
     * When implemented in a concrete subclass, that implementation MUST call this (super) method
     * as the first step. It may then read the fields specific to that chunk type.
     */
    virtual void read(InputStream *stream);
};

} // namespace parselib

#endif // _IO_WAV_WAVCHUNKHEADER_H_
