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

#include <sys/types.h>

#include <cstring>

#include <media/NdkMediaExtractor.h>
#include <utils/logging.h>
#include <cinttypes>

#include "NDKExtractor.h"

int32_t NDKExtractor::decode(AAsset *asset, uint8_t *targetData, AudioProperties targetProperties) {

    LOGD("Using NDK decoder");

    // open asset as file descriptor
    off_t start, length;
    int fd = AAsset_openFileDescriptor(asset, &start, &length);

    // Extract the audio frames
    AMediaExtractor *extractor = AMediaExtractor_new();
    media_status_t amresult = AMediaExtractor_setDataSourceFd(extractor, fd,
                                                              static_cast<off64_t>(start),
                                                              static_cast<off64_t>(length));
    if (amresult != AMEDIA_OK){
        LOGE("Error setting extractor data source, err %d", amresult);
        return 0;
    }

    // Specify our desired output format by creating it from our source
    AMediaFormat *format = AMediaExtractor_getTrackFormat(extractor, 0);

    int32_t sampleRate;
    if (AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_SAMPLE_RATE, &sampleRate)){
        LOGD("Source sample rate %d", sampleRate);
        if (sampleRate != targetProperties.sampleRate){
            LOGE("Input (%d) and output (%d) sample rates do not match. "
                 "NDK decoder does not support resampling.",
                 sampleRate,
                 targetProperties.sampleRate);
            return 0;
        }
    } else {
        LOGE("Failed to get sample rate");
        return 0;
    };

    int32_t channelCount;
    if (AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_CHANNEL_COUNT, &channelCount)){
        LOGD("Got channel count %d", channelCount);
        if (channelCount != targetProperties.channelCount){
            LOGE("NDK decoder does not support different "
                 "input (%d) and output (%d) channel counts",
                 channelCount,
                 targetProperties.channelCount);
        }
    } else {
        LOGE("Failed to get channel count");
        return 0;
    }

    const char *formatStr = AMediaFormat_toString(format);
    LOGD("Output format %s", formatStr);

    const char *mimeType;
    if (AMediaFormat_getString(format, AMEDIAFORMAT_KEY_MIME, &mimeType)) {
        LOGD("Got mime type %s", mimeType);
    } else {
        LOGE("Failed to get mime type");
        return 0;
    }

    // Obtain the correct decoder
    AMediaCodec *codec = nullptr;
    AMediaExtractor_selectTrack(extractor, 0);
    codec = AMediaCodec_createDecoderByType(mimeType);
    AMediaCodec_configure(codec, format, nullptr, nullptr, 0);
    AMediaCodec_start(codec);

    // DECODE

    bool isExtracting = true;
    bool isDecoding = true;
    int32_t bytesWritten = 0;

    while(isExtracting || isDecoding){

        if (isExtracting){

            // Obtain the index of the next available input buffer
            ssize_t inputIndex = AMediaCodec_dequeueInputBuffer(codec, 2000);
            //LOGV("Got input buffer %d", inputIndex);

            // The input index acts as a status if its negative
            if (inputIndex < 0){
                if (inputIndex == AMEDIACODEC_INFO_TRY_AGAIN_LATER){
                    // LOGV("Codec.dequeueInputBuffer try again later");
                } else {
                    LOGE("Codec.dequeueInputBuffer unknown error status");
                }
            } else {

                // Obtain the actual buffer and read the encoded data into it
                size_t inputSize;
                uint8_t *inputBuffer = AMediaCodec_getInputBuffer(codec, inputIndex, &inputSize);
                //LOGV("Sample size is: %d", inputSize);

                ssize_t sampleSize = AMediaExtractor_readSampleData(extractor, inputBuffer, inputSize);
                auto presentationTimeUs = AMediaExtractor_getSampleTime(extractor);

                if (sampleSize > 0){

                    // Enqueue the encoded data
                    AMediaCodec_queueInputBuffer(codec, inputIndex, 0, sampleSize,
                                                 presentationTimeUs,
                                                 0);
                    AMediaExtractor_advance(extractor);

                } else {
                    LOGD("End of extractor data stream");
                    isExtracting = false;

                    // We need to tell the codec that we've reached the end of the stream
                    AMediaCodec_queueInputBuffer(codec, inputIndex, 0, 0,
                                                 presentationTimeUs,
                                                 AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM);
                }
            }
        }

        if (isDecoding){
            // Dequeue the decoded data
            AMediaCodecBufferInfo info;
            ssize_t outputIndex = AMediaCodec_dequeueOutputBuffer(codec, &info, 0);

            if (outputIndex >= 0){

                // Check whether this is set earlier
                if (info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM){
                    LOGD("Reached end of decoding stream");
                    isDecoding = false;
                } 
                
                // Valid index, acquire buffer
                size_t outputSize;
                uint8_t *outputBuffer = AMediaCodec_getOutputBuffer(codec, outputIndex, &outputSize);

                /*LOGV("Got output buffer index %d, buffer size: %d, info size: %d writing to pcm index %d",
                     outputIndex,
                     outputSize,
                     info.size,
                     m_writeIndex);*/

                // copy the data out of the buffer
                memcpy(targetData + bytesWritten, outputBuffer, info.size);
                bytesWritten+=info.size;
                AMediaCodec_releaseOutputBuffer(codec, outputIndex, false);
            } else {

                // The outputIndex doubles as a status return if its value is < 0
                switch(outputIndex){
                    case AMEDIACODEC_INFO_TRY_AGAIN_LATER:
                        LOGD("dequeueOutputBuffer: try again later");
                        break;
                    case AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED:
                        LOGD("dequeueOutputBuffer: output buffers changed");
                        break;
                    case AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED:
                        LOGD("dequeueOutputBuffer: output outputFormat changed");
                        format = AMediaCodec_getOutputFormat(codec);
                        LOGD("outputFormat changed to: %s", AMediaFormat_toString(format));
                        break;
                }
            }
        }
    }

    // Clean up
    AMediaFormat_delete(format);
    AMediaCodec_delete(codec);
    AMediaExtractor_delete(extractor);

    return bytesWritten;
}
