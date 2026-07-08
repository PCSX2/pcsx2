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
#ifndef _IO_WAV_AUDIOFORMAT_H_
#define _IO_WAV_AUDIOFORMAT_H_

namespace parselib {

/**
 * Definitions for Audio Encodings in WAV files.
 */
class AudioEncoding {
public:
    static const int INVALID = -1;
    static const int PCM_16 = 0;
    static const int PCM_8 = 1;
    static const int PCM_IEEEFLOAT = 2;
    static const int PCM_24 = 3;
    static const int PCM_32 = 4;
};

} // namespace parselib

#endif // _IO_WAV_AUDIOFORMAT_H_
