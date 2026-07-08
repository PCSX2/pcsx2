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

#include <gtest/gtest.h>
#include <oboe/Oboe.h>
#include "../src/aaudio/AAudioLoader.h"

using namespace oboe;

class AAudioDirect : public ::testing::Test {
public:

    /**
     * @return true if AAudio NOT supported
     */
    bool openAAudio() {
        mAAudioLoader = AAudioLoader::getInstance();
        return (mAAudioLoader->open() != 0);
    }

    void createBuilder() {
        ASSERT_NE(mAAudioLoader, nullptr);
        ASSERT_EQ(0, mAAudioLoader->open());
        ASSERT_NE(mAAudioLoader->createStreamBuilder, nullptr);
        aaudio_result_t result = mAAudioLoader->createStreamBuilder(&mBuilder);
        ASSERT_NE(mBuilder, nullptr);
        ASSERT_EQ(result, 0);
    }

    void openCloseStream() {
        AAudioStream *stream = nullptr;
        aaudio_result_t result = mAAudioLoader->builder_openStream(mBuilder, &stream);
        ASSERT_EQ(result, 0);
        ASSERT_NE(stream, nullptr);

        result = mAAudioLoader->stream_close(stream);
        ASSERT_EQ(result, 0);
    }

    AAudioStreamBuilder *mBuilder = nullptr;
    AAudioLoader *mAAudioLoader = nullptr;
};

TEST_F(AAudioDirect, InstantiateAAudioLoader) {
    AAudioLoader *aaudioLoader = AAudioLoader::getInstance();
    ASSERT_NE(aaudioLoader, nullptr);
}

TEST_F(AAudioDirect, OpenCloseHighLatencyStream) {
    if (openAAudio()) return;
    createBuilder();
    mAAudioLoader->builder_setPerformanceMode(mBuilder, AAUDIO_PERFORMANCE_MODE_NONE);
    openCloseStream();
}

TEST_F(AAudioDirect, OpenCloseLowLatencyStream) {
    if (openAAudio()) return;
    createBuilder();
    mAAudioLoader->builder_setPerformanceMode(mBuilder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
    openCloseStream();
}
