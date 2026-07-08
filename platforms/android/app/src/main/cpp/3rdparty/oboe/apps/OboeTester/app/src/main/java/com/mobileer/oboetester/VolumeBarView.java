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

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.view.View;

import java.util.Locale;

/**
 * Display volume in dB as a red bar.
 * Display the amplitude, 0.0 to 1.0, as text in the bar.
 */
public class VolumeBarView extends View {

    private static final float MIN_VOLUME_DB = -84;
    private static final float MIN_AMPLITUDE_AT_MIN_VOLUME_DB =
            (float) Math.pow(10.0, MIN_VOLUME_DB / 20);
    public static final String FORMAT_AMPLITUDE = "%8.6f";

    private float mClippedAmplitude;
    private float mVolume = MIN_VOLUME_DB;

    private Paint mBarPaint;
    private Paint mTextPaint;
    private Paint mBackgroundPaint;
    private int mCurrentWidth;
    private int mCurrentHeight;
    private final Rect mTextBounds = new Rect();

    public VolumeBarView(Context context, AttributeSet attrs) {
        super(context, attrs);
//        TypedArray a = context.getTheme().obtainStyledAttributes(attrs,
//                R.styleable.VolumeBarView, 0, 0);
        init();
    }

    private void init() {
        mBarPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        mBarPaint.setColor(Color.rgb(240, 100, 100));
        mBarPaint.setStyle(Paint.Style.FILL);

        mBackgroundPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        mBackgroundPaint.setColor(Color.LTGRAY);
        mBackgroundPaint.setStyle(Paint.Style.FILL);

        mTextPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        mTextPaint.setColor(Color.BLACK);
        mTextPaint.setStyle(Paint.Style.FILL);
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        mCurrentWidth = w;
        mCurrentHeight = h;
        mTextPaint.setTextSize(0.8f * h);
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        canvas.drawRect(0.0f, 0.0f, mCurrentWidth,
                mCurrentHeight, mBackgroundPaint);
        float volumeWidth = ((MIN_VOLUME_DB - mVolume) / MIN_VOLUME_DB) * mCurrentWidth;
        canvas.drawRect(0.0f, 0.0f, volumeWidth,
                mCurrentHeight, mBarPaint);
        String text = String.format(Locale.getDefault(), FORMAT_AMPLITUDE, mClippedAmplitude);
        mTextPaint.getTextBounds(text, 0, text.length(), mTextBounds);
        canvas.drawText(text,
                20.0f,
                (mCurrentHeight/2) - mTextBounds.exactCenterY(),
                mTextPaint);
    }

    /**
     * Set peak amplitude of the signal.
     * Should be between 0.0 and 1.0
     */
    public void setAmplitude(float amplitude) {
        mClippedAmplitude = (float) Math.min(1.0, Math.max(0.0, amplitude));
        if (amplitude < MIN_AMPLITUDE_AT_MIN_VOLUME_DB) {
            mVolume = MIN_VOLUME_DB;
        } else {
            mVolume = 20.0f * (float)Math.log10(amplitude);
        }
        postInvalidate();
    }

}
