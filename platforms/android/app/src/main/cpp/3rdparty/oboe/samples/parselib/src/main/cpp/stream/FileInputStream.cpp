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
#include <unistd.h>

#include "FileInputStream.h"

namespace parselib {

int32_t FileInputStream::read(void *buff, int32_t numBytes) {
    return ::read(mFH, buff, numBytes);
}

int32_t FileInputStream::peek(void *buff, int32_t numBytes) {
    int32_t numRead = ::read(mFH, buff, numBytes);
    ::lseek(mFH, -numBytes, SEEK_CUR);
    return numRead;
}

void FileInputStream::advance(int32_t numBytes) {
    if (numBytes > 0) {
        ::lseek(mFH, numBytes, SEEK_CUR);
    }
}

int32_t FileInputStream::getPos() {
    return ::lseek(mFH, 0L, SEEK_CUR);
}

void FileInputStream::setPos(int32_t pos) {
    if (pos > 0) {
        ::lseek(mFH, pos, SEEK_SET);
    }
}

} /* namespace parselib */
