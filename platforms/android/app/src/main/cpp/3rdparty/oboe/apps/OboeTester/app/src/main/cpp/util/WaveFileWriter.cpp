/*
 * Copyright 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "WaveFileWriter.h"

void WaveFileWriter::WaveFileWriter::write(float value) {
    if (!mHeaderWritten) {
        writeHeader();
    }
    if (mBitsPerSample == 24) {
        writePCM24(value);
    } else {
        writePCM16(value);
    }
}

void WaveFileWriter::write(float *buffer, int32_t startSample, int32_t numSamples) {
    for (int32_t i = 0; i < numSamples; i++) {
        write(buffer[startSample + i]);
    }
}

void WaveFileWriter::writeIntLittle(int32_t n) {
    writeByte(n);
    writeByte(n >> 8);
    writeByte(n >> 16);
    writeByte(n >> 24);
}

void WaveFileWriter::writeShortLittle(int16_t n) {
    writeByte(n);
    writeByte(n >> 8);
}

void WaveFileWriter::writeFormatChunk() {
    int32_t bytesPerSample = (mBitsPerSample + 7) / 8;

    writeByte('f');
    writeByte('m');
    writeByte('t');
    writeByte(' ');
    writeIntLittle(16); // chunk size
    writeShortLittle(WAVE_FORMAT_PCM);
    writeShortLittle((int16_t) mSamplesPerFrame);
    writeIntLittle(mFrameRate);
    // bytes/second
    writeIntLittle(mFrameRate * mSamplesPerFrame * bytesPerSample);
    // block align
    writeShortLittle((int16_t) (mSamplesPerFrame * bytesPerSample));
    writeShortLittle((int16_t) mBitsPerSample);
}

int32_t WaveFileWriter::getDataSizeInBytes() {
    if (mFrameCount <= 0) return INT32_MAX;
    int64_t dataSize = ((int64_t)mFrameCount) * mSamplesPerFrame * mBitsPerSample / 8;
    return (int32_t)std::min(dataSize, (int64_t)INT32_MAX);
}

void WaveFileWriter::writeDataChunkHeader() {
    writeByte('d');
    writeByte('a');
    writeByte('t');
    writeByte('a');
    writeIntLittle(getDataSizeInBytes());
}

void WaveFileWriter::writeHeader() {
    writeRiffHeader();
    writeFormatChunk();
    writeDataChunkHeader();
    mHeaderWritten = true;
}

// Write lower 8 bits. Upper bits ignored.
void WaveFileWriter::writeByte(uint8_t b) {
    mOutputStream->write(b);
    mBytesWritten += 1;
}

void WaveFileWriter::writePCM24(float value) {
    // Offset before casting so that we can avoid using floor().
    // Also round by adding 0.5 so that very small signals go to zero.
    float temp = (PCM24_MAX * value) + 0.5 - PCM24_MIN;
    int32_t sample = ((int) temp) + PCM24_MIN;
    // clip to 24-bit range
    if (sample > PCM24_MAX) {
        sample = PCM24_MAX;
    } else if (sample < PCM24_MIN) {
        sample = PCM24_MIN;
    }
    // encode as little-endian
    writeByte(sample); // little end
    writeByte(sample >> 8); // middle
    writeByte(sample >> 16); // big end
}

void WaveFileWriter::writePCM16(float value) {
    // Offset before casting so that we can avoid using floor().
    // Also round by adding 0.5 so that very small signals go to zero.
    float temp = (INT16_MAX * value) + 0.5 - INT16_MIN;
    int32_t sample = ((int) temp) + INT16_MIN;
    if (sample > INT16_MAX) {
        sample = INT16_MAX;
    } else if (sample < INT16_MIN) {
        sample = INT16_MIN;
    }
    writeByte(sample); // little end
    writeByte(sample >> 8); // big end
}

void WaveFileWriter::writeRiffHeader() {
    writeByte('R');
    writeByte('I');
    writeByte('F');
    writeByte('F');
    // Maximum size is not strictly correct but is commonly used
    // when we do not know the final size.
    const int kExtraHeaderBytes = 36;
    int32_t dataSize = getDataSizeInBytes();
    writeIntLittle((dataSize > (INT32_MAX - kExtraHeaderBytes))
            ? INT32_MAX
            : dataSize + kExtraHeaderBytes);
    writeByte('W');
    writeByte('A');
    writeByte('V');
    writeByte('E');
}
