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

#ifndef FFMPEG_FFMPEGEXTRACTOR_H
#define FFMPEG_FFMPEGEXTRACTOR_H


extern "C" {
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
}

#include <cstdint>
#include <android/asset_manager.h>
#include <GameConstants.h>

class FFMpegExtractor {
public:
    static int64_t decode(AAsset *asset, uint8_t *targetData, AudioProperties targetProperties);

private:
    static bool createAVIOContext(AAsset *asset, uint8_t *buffer, uint32_t bufferSize,
                                  AVIOContext **avioContext);

    static bool createAVFormatContext(AVIOContext *avioContext, AVFormatContext **avFormatContext);

    static bool openAVFormatContext(AVFormatContext *avFormatContext);

    static int32_t cleanup(AVIOContext *avioContext, AVFormatContext *avFormatContext);

    static bool getStreamInfo(AVFormatContext *avFormatContext);

    static AVStream *getBestAudioStream(AVFormatContext *avFormatContext);

    static AVCodec *findCodec(AVCodecID id);

    static void printCodecParameters(AVCodecParameters *params);
};


#endif //FFMPEG_FFMPEGEXTRACTOR_H
