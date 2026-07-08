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
import android.content.res.TypedArray;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.widget.LinearLayout;
import android.widget.SeekBar;
import android.widget.TextView;

import java.util.Locale;

public class ExponentialSliderView extends LinearLayout {
    protected TextView mTextView;
    protected SeekBar mSeekBar;

    private String mLabel;
    protected ExponentialTaper mExponentialTaper;
    private double mMinRange;
    private double mMaxRange;
    private double mExponent = 10.0;
    private int mDefaultValue = 0;

    public interface OnValueChangedListener {
        void onValueChanged(ExponentialSliderView view, int value);
    }

    private OnValueChangedListener mListener;

    private SeekBar.OnSeekBarChangeListener mChangeListener = new SeekBar.OnSeekBarChangeListener() {
        @Override
        public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
            setValueByPosition(progress);
            if (mListener != null && fromUser) {
                mListener.onValueChanged(ExponentialSliderView.this, getValue());
            }
        }

        @Override
        public void onStartTrackingTouch(SeekBar seekBar) {
            // Optional: Implement if needed
        }

        @Override
        public void onStopTrackingTouch(SeekBar seekBar) {
            // Optional: Implement if needed
        }
    };

    public ExponentialSliderView(Context context) {
        super(context);
        initializeViews(context, null);
    }

    public ExponentialSliderView(Context context, AttributeSet attrs) {
        super(context, attrs);
        initializeViews(context, attrs);
    }

    public ExponentialSliderView(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        initializeViews(context, attrs);
    }

    public void setOnValueChangedListener(OnValueChangedListener listener) {
        mListener = listener;
    }

    void setFaderNormalizedProgress(double fraction) {
        mSeekBar.setProgress((int) Math.round(fraction * mSeekBar.getMax()));
    }

    /**
     * Inflates the views in the layout and handles custom attributes.
     *
     * @param context the current context for the view.
     * @param attrs   the attributes from the XML layout.
     */
    private void initializeViews(Context context, AttributeSet attrs) {
        LayoutInflater inflater = (LayoutInflater) context
                .getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        inflater.inflate(R.layout.exponential_slider_view, this); // Reusing the same layout

        mTextView = findViewById(R.id.textSlider);
        mSeekBar = findViewById(R.id.faderSlider);
        mSeekBar.setOnSeekBarChangeListener(mChangeListener);

        // Handle custom attributes
        if (attrs != null) {
            TypedArray a = context.getTheme().obtainStyledAttributes(
                    attrs,
                    R.styleable.ExponentialSliderView, // Defined in attrs_exponential_slider.xml
                    0, 0);

            try {
                mLabel = a.getString(R.styleable.ExponentialSliderView_sliderLabel);
                mMinRange = a.getFloat(R.styleable.ExponentialSliderView_minValue, 0.0f);
                mMaxRange = a.getFloat(R.styleable.ExponentialSliderView_maxValue, 100.0f);
                mExponent = a.getFloat(R.styleable.ExponentialSliderView_exponent, 10.0f);
                mDefaultValue = a.getInteger(R.styleable.ExponentialSliderView_defaultValue, 0);
                if (mLabel != null) {
                    mTextView.setText(mLabel);
                }
                setRange(mMinRange, mMaxRange, mExponent);
                setValue(mDefaultValue);
            } finally {
                a.recycle();
            }
        } else {
            // Default values if no attributes are provided
            setLabel("Value");
            setRange(0.0, 100.0, mExponent);
            setValue(mDefaultValue);
        }
    }

    public void setRange(double min, double max) {
        setRange(min, max, this.mExponent);
    }

    public void setRange(double min, double max, double exponent) {
        mMinRange = min;
        mMaxRange = max;
        mExponent = exponent;
        mExponentialTaper = new ExponentialTaper(mMinRange, mMaxRange, mExponent);
        setValueByPosition(mSeekBar.getProgress()); // Update display based on new range
    }

    private void setValueByPosition(int progress) {
        int value = (int) Math.round(mExponentialTaper.linearToExponential(
                ((double) progress) / mSeekBar.getMax()));
        mTextView.setText(getLabel() + " = " + String.format(Locale.getDefault(), "%3d", value));
    }

    public void setValue(int value) {
        double normalizedProgress = mExponentialTaper.exponentialToLinear(value);
        setFaderNormalizedProgress(normalizedProgress);
        setValueByPosition(mSeekBar.getProgress()); // Ensure text view is updated
    }

    public int getValue() {
        return (int) Math.round(mExponentialTaper.linearToExponential(
                ((double) mSeekBar.getProgress()) / mSeekBar.getMax()));
    }

    @Override
    public void setEnabled(boolean enabled) {
        super.setEnabled(enabled);
        mSeekBar.setEnabled(enabled);
    }

    public String getLabel() {
        return mLabel;
    }

    public void setLabel(String label) {
        this.mLabel = label;
        mTextView.setText(mLabel);
    }
}
