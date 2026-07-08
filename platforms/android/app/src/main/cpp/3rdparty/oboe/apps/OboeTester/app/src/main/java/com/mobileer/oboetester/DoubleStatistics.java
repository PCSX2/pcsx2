/*
 * Copyright 2022 The Android Open Source Project
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

package com.mobileer.oboetester;

import java.util.ArrayList;

class DoubleStatistics {
    ArrayList<Double> mValues = new ArrayList<Double>();
    private double mMin = Double.MAX_VALUE;
    private double mMax = Double.MIN_VALUE;
    private double mSum = 0.0;

    // Number of measurements.
    public int count() {
        return mValues.size();
    }

    public void add(double value) {
        mValues.add(value);
        mMin = Math.min(value, mMin);
        mMax = Math.max(value, mMax);
        mSum += value;
    }

    public double calculateMeanAbsoluteDeviation(double mean) {
        double deviationSum = 0.0;
        for (double value : mValues) {
            deviationSum += Math.abs(value - mean);
        }
        return deviationSum / mValues.size();
    }

    // This will crash if there are no values added.
    public double calculateMean() {
        return mSum / mValues.size();
    }

    public double getMin() {
        return mMin;
    }

    public double getMax() {
        return mMax;
    }

    public double getSum() {
        return mSum;
    }

    // This will crash if there are no values added.
    public double getLast() {
        return mValues.get(mValues.size() - 1);
    }
}
