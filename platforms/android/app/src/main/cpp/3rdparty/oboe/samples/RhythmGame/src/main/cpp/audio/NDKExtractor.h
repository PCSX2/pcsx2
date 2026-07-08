/*
 * Copyright 2018 The Android Open Source Project
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

#ifndef FFMPEG_NDKMEDIAEXTRACTOR_H
#define FFMPEG_NDKMEDIAEXTRACTOR_H


#include <cstdint>
#include <android/asset_manager.h>
#include <GameConstants.h>


class NDKExtractor {

public:
    static int32_t decode(AAsset *asset, uint8_t *targetData, AudioProperties targetProperties);
};


#endif //FFMPEG_NDKMEDIAEXTRACTOR_H
