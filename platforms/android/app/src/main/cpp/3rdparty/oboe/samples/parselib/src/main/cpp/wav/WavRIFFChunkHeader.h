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
#ifndef _IO_WAV_WAVRIFFCHUNKHEADER_H_
#define _IO_WAV_WAVRIFFCHUNKHEADER_H_

#include "WavChunkHeader.h"

namespace parselib {

class InputStream;

class WavRIFFChunkHeader : public WavChunkHeader {
public:
    static const RiffID RIFFID_RIFF;

    static const RiffID RIFFID_WAVE;

    RiffID mFormatId;

    WavRIFFChunkHeader();

    WavRIFFChunkHeader(RiffID tag);

    virtual void read(InputStream *stream);
};

} // namespace parselib

#endif // _IO_WAV_WAVRIFFCHUNKHEADER_H_
