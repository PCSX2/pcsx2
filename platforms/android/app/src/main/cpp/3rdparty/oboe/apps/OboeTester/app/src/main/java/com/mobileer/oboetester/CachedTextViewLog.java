/*
 * Copyright (C) 2020 The Android Open Source Project
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

import android.app.Activity;
import android.widget.TextView;

/** Wrap a TextView with a buffer and only update it periodically.
 */
public class CachedTextViewLog {
    private final Activity mActivity;
    private TextView mTextView;
    private int mUpdatePeriodMillis = 500;
    private long mLastUpdateTime;
    private StringBuffer mBuffer = new StringBuffer();

    public CachedTextViewLog(Activity activity, TextView textView) {
        mActivity = activity;
        mTextView = textView;
    }

    public synchronized void append(String text) {
        mBuffer.append(text);
        long now = System.currentTimeMillis();
        if ((now - mLastUpdateTime) > mUpdatePeriodMillis) {
            flush_l();
        }
    }

    public synchronized void flush() {
        flush_l();
    }

    // This must be called from a synchronized method.
    private void flush_l() {
        final String textToAdd = mBuffer.toString();
        mBuffer.setLength(0);
        mLastUpdateTime = System.currentTimeMillis();
        mActivity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mTextView.append(textToAdd);
            }
        });
    }

    public synchronized void clear() {
        mBuffer.setLength(0);
        mLastUpdateTime = System.currentTimeMillis();
        mActivity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mTextView.setText("");
            }
        });
    }

}
