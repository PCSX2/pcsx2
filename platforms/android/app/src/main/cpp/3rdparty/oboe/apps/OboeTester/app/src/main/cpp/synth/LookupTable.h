/*
 * Copyright (C) 2016 The Android Open Source Project
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
 *
 * This code was translated from the JSyn Java code.
 * JSyn is Copyright 2009 Phil Burk, Mobileer Inc
 * JSyn is licensed under the Apache License, Version 2.0
 */

#ifndef SYNTHMARK_LOOKUP_TABLE_H
#define SYNTHMARK_LOOKUP_TABLE_H

#include <cstdint>
#include "SynthTools.h"

namespace marksynth {

class LookupTable {
public:
    LookupTable(int32_t numEntries)
        : mNumEntries(numEntries)
        {}

    virtual ~LookupTable() {
        delete[] mTable;
    }

    void fillTable() {
        // Add 2 guard points for interpolation and roundoff error.
        int tableSize = mNumEntries + 2;
        mTable = new float[tableSize];
        // Fill the table with calculated values
        float scale = 1.0f / mNumEntries;
        for (int i = 0; i < tableSize; i++) {
            float value = calculate(i * scale);
            mTable[i] = value;
        }
    }

    /**
     * @param input normalized between 0.0 and 1.0
     */
    float lookup(float input) {
        float fractionalTableIndex = input * mNumEntries;
        int32_t index = (int) floor(fractionalTableIndex);
        float fraction = fractionalTableIndex - index;
        float baseValue = mTable[index];
        float value = baseValue
                + (fraction * (mTable[index + 1] - baseValue));
        return value;
    }

    virtual float calculate(float input)  = 0;

private:
    int32_t mNumEntries;
    synth_float_t *mTable;
};

};
#endif // SYNTHMARK_LOOKUP_TABLE_H
