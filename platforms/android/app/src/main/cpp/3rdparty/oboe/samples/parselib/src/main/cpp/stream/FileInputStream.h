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
#ifndef _IO_STREAM_FILEINPUTSTREAM_H_
#define _IO_STREAM_FILEINPUTSTREAM_H_

#include "InputStream.h"

namespace parselib {

/**
 * A concrete implementation of InputStream for a file data source
 */
class FileInputStream : public InputStream {
public:
    /** constructor. Caller is presumed to have opened the file with (at least) read permission */
    FileInputStream(int fh) : mFH(fh) {}
    virtual ~FileInputStream() {}

    virtual int32_t read(void *buff, int32_t numBytes);

    virtual int32_t peek(void *buff, int32_t numBytes);

    virtual void advance(int32_t numBytes);

    virtual int32_t getPos();

    virtual void setPos(int32_t pos);

private:
    /** File handle of the data file to read from */
    int mFH;
};

} // namespace parselib

#endif // _IO_STREAM_FILEINPUTSTREAM_H_
