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
#include <algorithm>
#include <string.h>

#include <android/log.h>

#include "stream/InputStream.h"

#include "AudioEncoding.h"
#include "WavRIFFChunkHeader.h"
#include "WavFmtChunkHeader.h"
#include "WavChunkHeader.h"
#include "WavStreamReader.h"

static const char *TAG = "WavStreamReader";

static constexpr int kConversionBufferFrames = 16;

namespace parselib {

WavStreamReader::WavStreamReader(InputStream *stream) {
    mStream = stream;

    mWavChunk = nullptr;
    mFmtChunk = nullptr;
    mDataChunk = nullptr;

    mAudioDataStartPos = -1;
}

int WavStreamReader::getSampleEncoding() {
    if (mFmtChunk->mEncodingId == WavFmtChunkHeader::ENCODING_PCM) {
        switch (mFmtChunk->mSampleSize) {
            case 8:
                return AudioEncoding::PCM_8;

            case 16:
                return AudioEncoding::PCM_16;

            case 24:
                return AudioEncoding::PCM_24;

            case 32:
                return AudioEncoding::PCM_32;

            default:
                return AudioEncoding::INVALID;
        }
    } else if (mFmtChunk->mEncodingId == WavFmtChunkHeader::ENCODING_IEEE_FLOAT) {
        return AudioEncoding::PCM_IEEEFLOAT;
    }

    return AudioEncoding::INVALID;
}

void WavStreamReader::parse() {
    RiffID tag;

    while (true) {
        int numRead = mStream->peek(&tag, sizeof(tag));
        if (numRead <= 0) {
            break; // done
        }

//        char *tagStr = (char *) &tag;
//        __android_log_print(ANDROID_LOG_INFO, TAG, "[%c%c%c%c]",
//                            tagStr[0], tagStr[1], tagStr[2], tagStr[3]);

        std::shared_ptr<WavChunkHeader> chunk = nullptr;
        if (tag == WavRIFFChunkHeader::RIFFID_RIFF) {
            chunk = mWavChunk = std::make_shared<WavRIFFChunkHeader>(WavRIFFChunkHeader(tag));
            mWavChunk->read(mStream);
        } else if (tag == WavFmtChunkHeader::RIFFID_FMT) {
            chunk = mFmtChunk = std::make_shared<WavFmtChunkHeader>(WavFmtChunkHeader(tag));
            mFmtChunk->read(mStream);
        } else if (tag == WavChunkHeader::RIFFID_DATA) {
            chunk = mDataChunk = std::make_shared<WavChunkHeader>(WavChunkHeader(tag));
            mDataChunk->read(mStream);
            // We are now positioned at the start of the audio data.
            mAudioDataStartPos = mStream->getPos();
            mStream->advance(mDataChunk->mChunkSize);
        } else {
            chunk = std::make_shared<WavChunkHeader>(WavChunkHeader(tag));
            chunk->read(mStream);
            mStream->advance(chunk->mChunkSize); // skip the body
        }

        mChunkMap[tag] = chunk;
    }

    if (mDataChunk != 0) {
        mStream->setPos(mAudioDataStartPos);
    }
}

// Data access
void WavStreamReader::positionToAudio() {
    if (mDataChunk != 0) {
        mStream->setPos(mAudioDataStartPos);
    }
}

/**
 * Read and convert samples in PCM8 format to float
 */
int WavStreamReader::getDataFloat_PCM8(float *buff, int numFrames) {
    int numChannels = mFmtChunk->mNumChannels;

    int buffOffset = 0;
    int totalFramesRead = 0;

    static constexpr int kSampleSize = sizeof(u_int8_t);
    static constexpr float kSampleFullScale = (float)0x80;
    static constexpr float kInverseScale = 1.0f / kSampleFullScale;

    u_int8_t readBuff[kConversionBufferFrames * numChannels];
    int framesLeft = numFrames;
    while (framesLeft > 0) {
        int framesThisRead = std::min(framesLeft, kConversionBufferFrames);
        //__android_log_print(ANDROID_LOG_INFO, TAG, "read(%d)", framesThisRead);
        int numFramesRead =
                mStream->read(readBuff, framesThisRead *  kSampleSize * numChannels) /
                (kSampleSize * numChannels);
        totalFramesRead += numFramesRead;

        // Convert & Scale
        for (int offset = 0; offset < numFramesRead * numChannels; offset++) {
            // PCM8 is unsigned, so we need to make it signed before scaling/converting
            buff[buffOffset++] = ((float) readBuff[offset] - kSampleFullScale)
                    * kInverseScale;
        }

        if (numFramesRead < framesThisRead) {
            break; // none left
        }

        framesLeft -= framesThisRead;
    }

    return totalFramesRead;
}

/**
 * Read and convert samples in PCM16 format to float
 */
int WavStreamReader::getDataFloat_PCM16(float *buff, int numFrames) {
    int numChannels = mFmtChunk->mNumChannels;

    int buffOffset = 0;
    int totalFramesRead = 0;

    static constexpr int kSampleSize = sizeof(int16_t);
    static constexpr float kSampleFullScale = (float) 0x8000;
    static constexpr float kInverseScale = 1.0f / kSampleFullScale;

    int16_t readBuff[kConversionBufferFrames * numChannels];
    int framesLeft = numFrames;
    while (framesLeft > 0) {
        int framesThisRead = std::min(framesLeft, kConversionBufferFrames);
        //__android_log_print(ANDROID_LOG_INFO, TAG, "read(%d)", framesThisRead);
        int numFramesRead =
                mStream->read(readBuff, framesThisRead * kSampleSize * numChannels) /
                (kSampleSize * numChannels);
        totalFramesRead += numFramesRead;

        // Convert & Scale
        for (int offset = 0; offset < numFramesRead * numChannels; offset++) {
            buff[buffOffset++] = (float) readBuff[offset] * kInverseScale;
        }

        if (numFramesRead < framesThisRead) {
            break; // none left
        }

        framesLeft -= framesThisRead;
    }

    return totalFramesRead;
}

/**
 * Read and convert samples in PCM24 format to float
 */
int WavStreamReader::getDataFloat_PCM24(float *buff, int numFrames) {
    int numChannels = mFmtChunk->mNumChannels;
    int numSamples = numFrames * numChannels;

    static constexpr float kSampleFullScale = (float) 0x80000000;
    static constexpr float kInverseScale = 1.0f / kSampleFullScale;

    uint8_t buffer[3];
    for(int sampleIndex = 0; sampleIndex < numSamples; sampleIndex++) {
        if (mStream->read(buffer, 3) < 3) {
            break; // no more data
        }
        int32_t sample = (buffer[0] << 8) | (buffer[1] << 16) | (buffer[2] << 24);
        buff[sampleIndex] = (float)sample * kInverseScale;
    }

    return numFrames;
}

/**
 * Read and convert samples in Float32 format to float
 */
int WavStreamReader::getDataFloat_Float32(float *buff, int numFrames) {
    // Turns out that WAV Float32 is just Android floats
    int numChannels = mFmtChunk->mNumChannels;

    return mStream->read(buff, numFrames * sizeof(float) * numChannels) /
           (sizeof(float) * numChannels);
}

/**
 * Read and convert samples in PCM32 format to float
 */
int WavStreamReader::getDataFloat_PCM32(float *buff, int numFrames) {
    int numChannels = mFmtChunk->mNumChannels;

    int buffOffset = 0;
    int totalFramesRead = 0;

    static constexpr int kSampleSize = sizeof(int32_t);
    static constexpr float kSampleFullScale = (float) 0x80000000;
    static constexpr float kInverseScale = 1.0f / kSampleFullScale;

    int32_t readBuff[kConversionBufferFrames * numChannels];
    int framesLeft = numFrames;
    while (framesLeft > 0) {
        int framesThisRead = std::min(framesLeft, kConversionBufferFrames);
        //__android_log_print(ANDROID_LOG_INFO, TAG, "read(%d)", framesThisRead);
        int numFramesRead =
                mStream->read(readBuff, framesThisRead *  kSampleSize* numChannels) /
                    (kSampleSize * numChannels);
        totalFramesRead += numFramesRead;

        // convert & Scale
        for (int offset = 0; offset < numFramesRead * numChannels; offset++) {
            buff[buffOffset++] = (float) readBuff[offset] * kInverseScale;
        }

        if (numFramesRead < framesThisRead) {
            break; // none left
        }

        framesLeft -= framesThisRead;
    }

    return totalFramesRead;
}

int WavStreamReader::getDataFloat(float *buff, int numFrames) {
    // __android_log_print(ANDROID_LOG_INFO, TAG, "getData(%d)", numFrames);

    if (mDataChunk == nullptr || mFmtChunk == nullptr) {
        return ERR_INVALID_STATE;
    }

    int numFramesRead = 0;
    switch (mFmtChunk->mSampleSize) {
        case 8:
            numFramesRead = getDataFloat_PCM8(buff, numFrames);
            break;

        case 16:
            numFramesRead = getDataFloat_PCM16(buff, numFrames);
            break;

        case 24:
            if (mFmtChunk->mEncodingId == WavFmtChunkHeader::ENCODING_PCM) {
                numFramesRead = getDataFloat_PCM24(buff, numFrames);
            } else {
                __android_log_print(ANDROID_LOG_INFO, TAG, "invalid encoding:%d mSampleSize:%d",
                                    mFmtChunk->mEncodingId, mFmtChunk->mSampleSize);
            }
            break;

        case 32:
            if (mFmtChunk->mEncodingId == WavFmtChunkHeader::ENCODING_PCM) {
                numFramesRead = getDataFloat_PCM32(buff, numFrames);
            } else if (mFmtChunk->mEncodingId == WavFmtChunkHeader::ENCODING_IEEE_FLOAT) {
                numFramesRead = getDataFloat_Float32(buff, numFrames);
            } else {
                __android_log_print(ANDROID_LOG_INFO, TAG, "invalid encoding:%d mSampleSize:%d",
                                    mFmtChunk->mEncodingId, mFmtChunk->mSampleSize);
            }
            break;

        default:
            __android_log_print(ANDROID_LOG_INFO, TAG, "invalid encoding:%d mSampleSize:%d",
                    mFmtChunk->mEncodingId, mFmtChunk->mSampleSize);
            return ERR_INVALID_FORMAT;
    }

    // Zero out any unread frames
    if (numFramesRead < numFrames) {
        int numChannels = getNumChannels();
        memset(buff + (numFramesRead * numChannels), 0,
                (numFrames - numFramesRead) * sizeof(buff[0]) * numChannels);
    }

    return numFramesRead;
}

} // namespace parselib
