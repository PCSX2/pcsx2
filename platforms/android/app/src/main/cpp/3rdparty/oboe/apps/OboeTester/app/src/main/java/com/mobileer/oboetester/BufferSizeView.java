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

package com.mobileer.oboetester;

import android.content.Context;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.RadioButton;
import android.widget.RadioGroup;
import android.widget.SeekBar;
import android.widget.TextView;

public class BufferSizeView extends LinearLayout {
    private OboeAudioStream mStream;

    private static final int FADER_THRESHOLD_MAX = 1000; // must match layout
    private static final int USE_FADER = -1;
    private static final int DEFAULT_NUM_BURSTS = 2;
    private TextView mTextLabel;
    private SeekBar mFader;
    private ExponentialTaper mTaper;
    private RadioGroup mBufferSizeGroup;
    private RadioButton mBufferSizeRadio1;
    private RadioButton mBufferSizeRadio2;
    private RadioButton mBufferSizeRadio3;
    private int mCachedCapacity;
    private int mFramesPerBurst;
    private int mNumBursts;

    private SeekBar.OnSeekBarChangeListener mFaderListener = new SeekBar.OnSeekBarChangeListener() {
        @Override
        public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
            mNumBursts = USE_FADER;
            updateRadioButtons();
            setBufferSizeByPosition(progress);
        }

        @Override
        public void onStartTrackingTouch(SeekBar seekBar) {
        }

        @Override
        public void onStopTrackingTouch(SeekBar seekBar) {
        }
    };

    public BufferSizeView(Context context) {
        super(context);
        initializeViews(context);
    }

    public BufferSizeView(Context context, AttributeSet attrs) {
        super(context, attrs);
        initializeViews(context);
    }

    public BufferSizeView(Context context,
                          AttributeSet attrs,
                          int defStyle) {
        super(context, attrs, defStyle);
        initializeViews(context);
    }

    void setFaderNormalizedProgress(double fraction) {
        mFader.setProgress((int) (fraction * FADER_THRESHOLD_MAX));
    }

    /**
     * Inflates the views in the layout.
     *
     * @param context the current context for the view.
     */
    private void initializeViews(Context context) {
        LayoutInflater inflater = (LayoutInflater) context
                .getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        inflater.inflate(R.layout.buffer_size_view, this);

        mTextLabel = (TextView) findViewById(R.id.textThreshold);
        mFader = (SeekBar) findViewById(R.id.faderBufferSize);
        mFader.setOnSeekBarChangeListener(mFaderListener);
        mTaper = new ExponentialTaper(0.0, 1.0, 10.0);
        mFader.setProgress(0);

        mBufferSizeGroup = (RadioGroup) findViewById(R.id.bufferSizeGroup);

        mBufferSizeRadio1 = (RadioButton) findViewById(R.id.bufferSize1);
        mBufferSizeRadio1.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                onSizeRadioButtonClicked(view, 1);
            }
        });
        mBufferSizeRadio2 = (RadioButton) findViewById(R.id.bufferSize2);
        mBufferSizeRadio2.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                onSizeRadioButtonClicked(view, 2);
            }
        });
        mBufferSizeRadio3 = (RadioButton) findViewById(R.id.bufferSize3);
        mBufferSizeRadio3.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                onSizeRadioButtonClicked(view, 3);
            }
        });

        mNumBursts = DEFAULT_NUM_BURSTS;
        updateRadioButtons();
        updateBufferSize();
    }

    public void updateRadioButtons() {
        if (mNumBursts == USE_FADER && mBufferSizeGroup != null) {
            // Clear all the radio buttons using the group.
            // If you clear a checked button directly then it stops working.
            mBufferSizeGroup.clearCheck();
        } else if (mBufferSizeRadio3 != null) {
            mBufferSizeRadio1.setChecked(mNumBursts == 1);
            mBufferSizeRadio2.setChecked(mNumBursts == 2);
            mBufferSizeRadio3.setChecked(mNumBursts == 3);
        }
    }

    private void onSizeRadioButtonClicked(View view, int numBursts) {
        boolean checked = ((RadioButton) view).isChecked();
        if (!checked) return;
        mNumBursts = numBursts;
        setBufferSizeByNumBursts(numBursts);
    }

    // sets mStream, mCachedCapacity and mFramesPerBurst
    public void onStreamOpened(OboeAudioStream stream) {
        mStream = stream;
        if (mStream != null) {
            int capacity = mStream.getBufferCapacityInFrames();
            if (capacity > 0) mCachedCapacity = capacity;
            int framesPerBurst = mStream.getFramesPerBurst();
            if (framesPerBurst > 0) mFramesPerBurst = framesPerBurst;
        }
        updateBufferSize();
    }

    protected void setBufferSizeByNumBursts(int numBursts) {
        int sizeFrames = -1;
        if (mStream != null) {
            int framesPerBurst = mStream.getFramesPerBurst();
            if (framesPerBurst > 0) {
                sizeFrames = numBursts * framesPerBurst;
            }
        }
        StringBuffer message = new StringBuffer();
        message.append("bufferSize = #" + numBursts);

        setBufferSize(message, sizeFrames);
    }

    private void setBufferSizeByPosition(int progress) {
        int sizeFrames = -1;
        double normalizedThreshold = 0.0;

        StringBuffer message = new StringBuffer();

        normalizedThreshold = mTaper.linearToExponential(
                ((double) progress) / FADER_THRESHOLD_MAX);
        if (normalizedThreshold < 0.0) normalizedThreshold = 0.0;
        else if (normalizedThreshold > 1.0) normalizedThreshold = 1.0;
        int percent = (int) (normalizedThreshold * 100);
        message.append("bufferSize = " + percent + "%");

        if (mCachedCapacity > 0) {
            sizeFrames = (int) (normalizedThreshold * mCachedCapacity);
        }
        setBufferSize(message, sizeFrames);
    }

    protected void setBufferSize(StringBuffer message, int sizeFrames) {
        if (mStream != null) {
            message.append(", " + sizeFrames);
            if (mStream != null && sizeFrames >= 0) {
                mStream.setBufferSizeInFrames(sizeFrames);
            }
            int bufferSize = mStream.getBufferSizeInFrames();
            if (bufferSize >= 0) {
                message.append(" / " + bufferSize);
            }
            message.append(" / " + mCachedCapacity);
        }
        mTextLabel.setText(message.toString());
    }

    private void updateBufferSize() {
        if (mNumBursts == USE_FADER) {
            int progress = mFader.getProgress();
            setBufferSizeByPosition(progress);
        } else {
            setBufferSizeByNumBursts(mNumBursts);
        }
    }

    @Override
    public void setEnabled(boolean enabled) {
        super.setEnabled(enabled);
        mFader.setEnabled(enabled);
    }
}
