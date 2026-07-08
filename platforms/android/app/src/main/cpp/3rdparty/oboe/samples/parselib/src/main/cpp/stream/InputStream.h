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
#ifndef _IO_STREAM_INPUTSTREAM_H_
#define _IO_STREAM_INPUTSTREAM_H_

#include <cstdint>

namespace parselib {

/**
 * An interface declaration for a stream of bytes. Concrete implements for File and Memory Buffers
 */
class InputStream {
public:
    InputStream() {}
    virtual ~InputStream() {}

    /**
     * Retrieve the specified number of bytes and advance the read position.
     * Returns: The number of bytes actually retrieved. May be less than requested
     * if attempt to read beyond the end of the stream.
     */
    virtual int32_t read(void *buff, int32_t numBytes) = 0;

    /**
     * Retrieve the specified number of bytes. DOES NOT advance the read position.
     * Returns: The number of bytes actually retrieved. May be less than requested
     * if attempt to read beyond the end of the stream.
     */
    virtual int32_t peek(void *buff, int32_t numBytes) = 0;

    /**
     * Moves the read position forward the (positive) number of bytes specified.
     */
    virtual void advance(int32_t numBytes) = 0;

    /**
     * Returns the read position of the stream
     */
    virtual int32_t getPos() = 0;

    /**
     * Sets the read position of the stream to the 0 or positive position.
     */
    virtual void setPos(int32_t pos) = 0;
};

} // namespace parselib

#endif // _IO_STREAM_INPUTSTREAM_H_
