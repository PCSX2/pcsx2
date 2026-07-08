/*
 * Copyright (C) 2013 The Android Open Source Project
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
 * Circular buffer for continuously capturing audio then reading the previous N samples.
 * Can hold from zero to max frames.
 */
public class CircularCaptureBuffer {

    private float[] mData;
    private int mCursor;
    private int mNumValidSamples;

    public CircularCaptureBuffer(int maxSamples) {
        mData = new float[maxSamples];
    }

    public int write(float[] buffer) {
        return write(buffer, 0, buffer.length);
    }

    public int write(float[] buffer, int offset, int numSamples) {
        if (numSamples > mData.length) {
            throw new IllegalArgumentException("Tried to write more than maxSamples.");
        }
        if ((mCursor + numSamples) > mData.length) {
            // Wraps so write in two parts.
            int numWrite1 = mData.length - mCursor;
            System.arraycopy(buffer, offset, mData, mCursor, numWrite1);
            offset += numWrite1;
            int numWrite2 = numSamples - numWrite1;
            System.arraycopy(buffer, offset, mData, 0, numWrite2);
            mCursor = numWrite2;
        } else {
            System.arraycopy(buffer, offset, mData, mCursor, numSamples);
            mCursor += numSamples;
            if (mCursor == mData.length) {
                mCursor = 0;
            }
        }
        mNumValidSamples += numSamples;
        if (mNumValidSamples > mData.length) {
            mNumValidSamples = mData.length;
        }
        return numSamples;
    }

    public int readMostRecent(float[] buffer) {
        return readMostRecent(buffer, 0, buffer.length);
    }

    /**
     * Read the most recently written samples.
     * @param buffer
     * @param offset
     * @param numSamples
     * @return number of samples read
     */
    public int readMostRecent(float[] buffer, int offset, int numSamples) {

        if (numSamples > mNumValidSamples) {
            numSamples = mNumValidSamples;
        }
        int cursor = mCursor; // read once in case it gets updated by another thread
        // Read in two parts.
        if ((cursor - numSamples) < 0) {
            int numRead1 = numSamples - cursor;
            System.arraycopy(mData, mData.length - numRead1, buffer, offset, numRead1);
            offset += numRead1;
            int numRead2 = cursor;
            System.arraycopy(mData, 0, buffer, offset, numRead2);
        } else {
            System.arraycopy(mData, cursor - numSamples, buffer, offset, numSamples);
        }

        return numSamples;
    }

    public void erase() {
        mNumValidSamples = 0;
        mCursor = 0;
    }

    public int getSize() {
        return mData.length;
    }

}
