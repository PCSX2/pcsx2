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


#ifndef OBOETESTER_EXPONENTIAL_SHAPE_H
#define OBOETESTER_EXPONENTIAL_SHAPE_H

#include "flowgraph/FlowGraphNode.h"

/**
 * Generate a exponential sweep between min and max.
 *
 * The waveform is not band-limited so it will have aliasing artifacts at higher frequencies.
 */
class ExponentialShape : public oboe::flowgraph::FlowGraphFilter {
public:
    ExponentialShape();

    int32_t onProcess(int32_t numFrames) override;

    float getMinimum() const {
        return mMinimum;
    }

    /**
     * The minimum and maximum should not span zero.
     * They should both be positive or both negative.
     *
     * @param minimum
     */
    void setMinimum(float minimum) {
        mMinimum = minimum;
        mRatio = mMaximum / mMinimum;
    }

    float getMaximum() const {
        return mMaximum;
    }

    /**
     * The minimum and maximum should not span zero.
     * They should both be positive or both negative.
     *
     * @param maximum
     */
    void setMaximum(float maximum) {
        mMaximum = maximum;
        mRatio = mMaximum / mMinimum;
    }

private:
float mMinimum = 0.0;
float mMaximum = 1.0;
float mRatio = 1.0;
};

#endif //OBOETESTER_EXPONENTIAL_SHAPE_H
