/*
 * Copyright 2025 The Android Open Source Project
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

import android.content.Context;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.widget.LinearLayout;
import android.widget.SeekBar;
import android.widget.TextView;

public class PartialDataCallbackSizeView extends LinearLayout {
    private OboeAudioStream mStream;

    private SeekBar mSeekBar;
    private TextView mValueView;
    private int mProgress;

    public PartialDataCallbackSizeView(Context context) {
        super(context);
        initializeViews(context);
    }

    public PartialDataCallbackSizeView(Context context, AttributeSet attrs) {
        super(context, attrs);
        initializeViews(context);
    }

    public PartialDataCallbackSizeView(Context context,
                          AttributeSet attrs,
                          int defStyle) {
        super(context, attrs, defStyle);
        initializeViews(context);
    }

    public void onStreamOpened(OboeAudioStream stream) {
        mStream = stream;
    }


    private void initializeViews(Context context) {
        LayoutInflater inflater = (LayoutInflater) context
                .getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        inflater.inflate(R.layout.partial_data_callback_size_view, this);

        mSeekBar = findViewById(R.id.partialCallbackSizeSeekBar);
        mValueView = findViewById(R.id.partialCallbackSizeValue);

        mSeekBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                mProgress = progress;
                mValueView.setText(progress + "%");
                if (mStream != null) {
                    mStream.setPartialCallbackPercentage(progress);
                }
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {
            }

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
            }
        });
        mProgress = mSeekBar.getProgress();
    }

    public int getProgress() {
        return mProgress;
    }
}
