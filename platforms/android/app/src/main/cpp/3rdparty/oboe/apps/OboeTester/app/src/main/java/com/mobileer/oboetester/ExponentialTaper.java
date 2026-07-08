/*
 * Copyright 2017 The Android Open Source Project
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

/**
 * Maps integer range info to a double value along an exponential scale.
 *
 * <pre>
 *
 *   x = ival / mResolution
 *   f(x) = a*(root**bx)
 *   f(0.0) = dmin
 *   f(1.0) = dmax
 *
 *   f(0.0) = a * 1.0 => a = dmin
 *   f(1.0) = dmin * root**b = dmax
 *   b = log(dmax / dmin) / log(root)
 *
 * </pre>
 */

public class ExponentialTaper {
    private double offset = 0.0;
    private double a = 1.0;
    private double b = 2.0;
    private static final double ROOT = 10.0; // because we are using log10

    public ExponentialTaper(double dmin, double dmax) {
        this(dmin, dmax, 10000.0);
    }

    public ExponentialTaper(double dmin, double dmax, double maxRatio) {
        a = dmax;
        double curvature;
        if (dmax > dmin * maxRatio) {
            offset = dmax / maxRatio;
            a = offset;
            curvature = (dmax + offset) / offset;
        } else {
            curvature = dmax / dmin;
            a = dmin;
        }
        b = Math.log10(curvature);
    }

    public double linearToExponential(double linear) {
        return a * Math.pow(ROOT,  b * linear) - offset;
    }

    public double exponentialToLinear(double exponential) {
        return Math.log((exponential + offset) / a) / (b * Math.log(ROOT));
    }
}
