/*
 * Copyright (C) 2019 The Android Open Source Project
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

#ifndef _PLAYER_AUDIOSOURCE_H_
#define _PLAYER_AUDIOSOURCE_H_

#include <cstdint>

namespace iolib {

/*
 * Defines an interface for audio data sources for the SimpleMultiPlayer class.
 */
class DataSource {
public:
    virtual ~DataSource() {};

    virtual void mixAudio(float* outBuff, int numChannels, int numFrames) = 0;
};

}

#endif //_PLAYER_AUDIOSOURCE_H_
