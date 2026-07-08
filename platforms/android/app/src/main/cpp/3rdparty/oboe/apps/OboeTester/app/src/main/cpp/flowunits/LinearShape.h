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


#ifndef OBOETESTER_LINEAR_SHAPE_H
#define OBOETESTER_LINEAR_SHAPE_H

#include "flowgraph/FlowGraphNode.h"

/**
 * Convert an input between -1.0 and +1.0 to a linear region between min and max.
 */
class LinearShape : public oboe::flowgraph::FlowGraphFilter {
public:
    LinearShape();

    int32_t onProcess(int numFrames) override;

    float getMinimum() const {
        return mMinimum;
    }

    void setMinimum(float minimum) {
        mMinimum = minimum;
    }

    float getMaximum() const {
        return mMaximum;
    }

    void setMaximum(float maximum) {
        mMaximum = maximum;
    }

private:
    float mMinimum = 0.0;
    float mMaximum = 1.0;
};

#endif //OBOETESTER_LINEAR_SHAPE_H
