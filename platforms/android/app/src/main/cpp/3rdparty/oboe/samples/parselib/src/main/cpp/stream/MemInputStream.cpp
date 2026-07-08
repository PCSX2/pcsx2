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

#include "MemInputStream.h"

namespace parselib {

int32_t MemInputStream::read(void *buff, int32_t numBytes) {
    int32_t numAvail = mBufferLen - mPos;
    numBytes = std::min(numBytes, numAvail);

    peek(buff, numBytes);
    mPos += numBytes;
    return numBytes;
}

int32_t MemInputStream::peek(void *buff, int32_t numBytes) {
    int32_t numAvail = mBufferLen - mPos;
    numBytes = std::min(numBytes, numAvail);
    memcpy(buff, mBuffer + mPos, numBytes);
    return numBytes;
}

void MemInputStream::advance(int32_t numBytes) {
    if (numBytes > 0) {
        int32_t numAvail = mBufferLen - mPos;
        mPos += std::min(numAvail, numBytes);
    }
}

int32_t MemInputStream::getPos() {
    return mPos;
}

void MemInputStream::setPos(int32_t pos) {
    if (pos > 0) {
        if (pos < mBufferLen) {
            mPos = pos;
        } else {
            mPos = mBufferLen - 1;
        }
    }
}

} // namespace parselib
