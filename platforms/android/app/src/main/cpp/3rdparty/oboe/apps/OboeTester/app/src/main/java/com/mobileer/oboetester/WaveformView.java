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
import android.content.res.Resources;
import android.content.res.TypedArray;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.util.AttributeSet;
import android.view.View;

/**
 * Display an audio waveform in a custom View.
 * Data is assumed to have a constant x-axis increment, ie. isochronous.
 */
public class WaveformView extends View {
    private static final float MESSAGE_TEXT_SIZE = 80;
    private Paint mWavePaint;
    private Paint mBackgroundPaint;
    private Paint mMessagePaint;
    private int mCurrentWidth;
    private int mCurrentHeight;
    private float[] mData;
    private int mSampleCount;
    private int mSampleOffset;
    private float mOffsetY;
    private float mScaleY;
    private int[] mCursors;
    private String mMessage;
    private Paint mCursorPaint;

    public WaveformView(Context context, AttributeSet attrs) {
        super(context, attrs);
        TypedArray a = context.getTheme().obtainStyledAttributes(attrs,
                R.styleable.WaveformView, 0, 0);
        init();
    }
    @SuppressWarnings("deprecation")
    private void init() {
        Resources res = getResources();

        mWavePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        mWavePaint.setColor(res.getColor(R.color.waveform_line));
        float strokeWidth = res.getDimension(R.dimen.waveform_stroke_width);
        mWavePaint.setStrokeWidth(strokeWidth);

        mCursorPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        mCursorPaint.setColor(Color.RED);
        mCursorPaint.setStrokeWidth(3.0f);

        mBackgroundPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        mBackgroundPaint.setColor(res.getColor(R.color.waveform_background));
        mBackgroundPaint.setStyle(Paint.Style.FILL);

        mMessagePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        mMessagePaint.setColor(res.getColor(R.color.waveform_line));
        mMessagePaint.setTextSize(MESSAGE_TEXT_SIZE);
    }

    public void updateTheme(int waveColor, int backgroundColor, int cursorColor) {
        mWavePaint.setColor(waveColor);
        mBackgroundPaint.setColor(backgroundColor);
        mCursorPaint.setColor(cursorColor);
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        mCurrentWidth = w;
        mCurrentHeight = h;
        mOffsetY = 0.5f * h; // Center waveform vertically in the viewport.
        // Scale down so that we can see the top of the waveforms if they are clipped.
        mScaleY = -0.95f * mOffsetY; // Negate so positive values are on top.
    }

    public String getMessage() {
        return mMessage;
    }

    /**
     * Set message to be displayed over the waveform.
     * Set null for no message.
     * @param message text or null
     */
    public void setMessage(String message) {
        this.mMessage = message;
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        float [] localData = mData;
        canvas.drawRect(0.0f, 0.0f, mCurrentWidth,
                mCurrentHeight, mBackgroundPaint);
        if (mMessage != null) {
            canvas.drawText(mMessage, 20, mCurrentHeight - 20, mMessagePaint);
        }
        if (localData == null || mSampleCount == 0) {
            return;
        }

        float xScale = ((float) mCurrentWidth) / (mSampleCount - 1);

        // Draw cursors.
        if (mCursors != null) {
            for (int i = 0; i < mCursors.length; i++) {
                float x = mCursors[i] * xScale;
                canvas.drawLine(x, 0, x, mCurrentHeight, mCursorPaint);
            }
        }

        // Draw waveform.
        float x0 = 0.0f;
        if (xScale < 1.0) {
            // Draw a vertical bar for multiple samples.
            float ymin = mOffsetY; // vertical center
            float ymax = mOffsetY; // vertical center
            for (int i = 0; i < mSampleCount; i++) {
                float x1 = i * xScale;
                if ((int) x0 != (int) x1) {
                    // draw old data
                    canvas.drawLine(x0, ymin, x0, ymax, mWavePaint);
                    x0 = x1;
                    ymin = mOffsetY;
                    ymax = mOffsetY;
                }
                float y1 = (localData[i] * mScaleY) + mOffsetY;
                ymin = Math.min(ymin, y1);
                ymax = Math.max(ymax, y1);
            }
        } else {
            // Draw line between samples.
            float y0 = (localData[0] * mScaleY) + mOffsetY;
            for (int i = 1; i < mSampleCount; i++) {
                float x1 = i * xScale;
                float y1 = (localData[i] * mScaleY) + mOffsetY;
                canvas.drawLine(x0, y0, x1, y1, mWavePaint);
                x0 = x1;
                y0 = y1;
            }
        }
    }

    /**
     * Copy data into internal buffer.
     * Caller should then postInvalidate().
     */
    public void setSampleData(float[] samples) {
        setSampleData(samples, 0, samples.length);
    }

    public void setSampleData(float[] samples, int offset, int count) {
        if ((offset+count) > samples.length) {
            throw new IllegalArgumentException("Exceed array bounds. ("
                    + offset + " + " + count + ") > " + samples.length);
        }
        if (mData == null || count > mData.length) {
            mData = new float[count];
        }
        System.arraycopy(samples, offset, mData, 0, count);
        mSampleCount = count;
        mSampleOffset = offset;
    }

    public void clearSampleData() {
        mData = null;
        mSampleCount = 0;
        mSampleOffset = 0;
    }

    /**
     * Copy cursor positions into internal buffer.
     * Caller should then postInvalidate().
     */
    public void setCursorData(int[] cursors) {
        if (cursors == null) {
            mCursors = null;
        } else {
            if (mCursors == null || cursors.length != mCursors.length) {
                mCursors = new int[cursors.length];
            }
            System.arraycopy(cursors, 0, mCursors, 0, mCursors.length);
        }
    }
}
