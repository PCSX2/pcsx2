package com.mobileer.oboetester;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.util.AttributeSet;
import android.view.View;

import java.util.ArrayList;

/**
 * Draw a chart with multiple traces
 */
public class MultiLineChart extends View {
    public static final int NUM_DATA_VALUES = 512;
    private Paint mWavePaint;
    private Paint mCursorPaint;
    private int mBackgroundColor = 0xFFF0F0F0;
    private int mLineColor = Color.RED;
    private Paint mBackgroundPaint;
    float[] mVertices = new float[4];
    float[] mSecondaryColorVertices = new float[4];

    CircularFloatArray mXData = new CircularFloatArray(NUM_DATA_VALUES);
    private ArrayList<Trace> mTraceList = new ArrayList<>();

    public MultiLineChart(Context context) {
        super(context);
        init(null, 0);
    }

    public MultiLineChart(Context context, AttributeSet attrs) {
        super(context, attrs);
        init(attrs, 0);
    }

    public MultiLineChart(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
        init(attrs, defStyle);
    }

    private void init(AttributeSet attrs, int defStyle) {
        // Load attributes
        final TypedArray a = getContext().obtainStyledAttributes(
                attrs, R.styleable.MultiLineChart, defStyle, 0);

        mBackgroundColor = a.getColor(
                R.styleable.MultiLineChart_backgroundColor,
                mBackgroundColor);
        mLineColor = a.getColor(
                R.styleable.MultiLineChart_backgroundColor,
                mLineColor);

        a.recycle();

        mWavePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        mWavePaint.setColor(mLineColor);

        mCursorPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        mCursorPaint.setColor(Color.RED);
        mCursorPaint.setStrokeWidth(3.0f);

        mBackgroundPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        mBackgroundPaint.setColor(mBackgroundColor);
        mBackgroundPaint.setStyle(Paint.Style.FILL);
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);

        canvas.drawRect(0.0f, 0.0f, getWidth(),
                getHeight(), mBackgroundPaint);

        for (Trace trace : mTraceList) {
            drawTrace(canvas, trace);
        }
    }

    void drawTrace(Canvas canvas, Trace trace) {
        // Determine bounds and XY conversion.
        int numPoints = mXData.size();
        if (numPoints < 2) return;
        // Allocate array for polyline.
        int arraySize = (numPoints - 1) * 4;
        if (arraySize > mVertices.length) {
            mVertices = new float[arraySize];
        }
        if (arraySize > mSecondaryColorVertices.length) {
            mSecondaryColorVertices = new float[arraySize];
        }
        // Setup scaling.
        float previousX = 0.0f;
        float previousY = 0.0f;
        float xMax = getXData(1);
        float xRange = xMax - getXData(numPoints);
        float yMin =  trace.getMin();
        float yRange = trace.getMax() - yMin;
        float width = getWidth();
        float height = getHeight();
        float xScaler =  width / xRange;
        float yScaler =  height / yRange;
        // Iterate through the available data.
        int vertexIndex = 0;
        int secondaryColorVertexIndex = 0;
        for (int i = 1; i < numPoints; i++) {
            float xData = getXData(i);
            float yData = trace.get(i);
            float xPos = width - ((xMax - xData) * xScaler);
            float yPos = height - ((yData - yMin) * yScaler);
            if (i > 1) {
                // Each line segment requires 4 values!
                if (trace.getUseSecondaryColor(i)) {
                    mSecondaryColorVertices[secondaryColorVertexIndex++] = previousX;
                    mSecondaryColorVertices[secondaryColorVertexIndex++] = previousY;
                    mSecondaryColorVertices[secondaryColorVertexIndex++] = xPos;
                    mSecondaryColorVertices[secondaryColorVertexIndex++] = yPos;
                } else {
                    mVertices[vertexIndex++] = previousX;
                    mVertices[vertexIndex++] = previousY;
                    mVertices[vertexIndex++] = xPos;
                    mVertices[vertexIndex++] = yPos;
                }
            }
            previousX = xPos;
            previousY = yPos;
        }
        canvas.drawLines(mVertices, 0, vertexIndex, trace.paint);
        canvas.drawLines(mSecondaryColorVertices, 0, secondaryColorVertexIndex, trace.secondaryPaint);
    }

    public float getXData(int i) {
        return mXData.get(i);
    }

    public MultiLineChart.Trace createTrace(String name, int color, int secondaryColor, float min, float max) {
        Trace trace = new Trace(name, color, secondaryColor, NUM_DATA_VALUES, min, max);
        mTraceList.add(trace);
        return trace;
    }

    public MultiLineChart.Trace createTrace(String name, int color, float min, float max) {
        Trace trace = new Trace(name, color, color, NUM_DATA_VALUES, min, max);
        mTraceList.add(trace);
        return trace;
    }

    public void update() {
        post(new Runnable() {
            public void run() {
                postInvalidate();
            }
        });
    }

    public void addX(float value) {
        mXData.add(value, false /* useSecondaryColor */);
    }

    public void reset() {
        mXData.clear();
        for (Trace trace : mTraceList) {
            trace.reset();
        }
    }

    public static class Trace {
        private final String mName;
        public Paint paint;
        public Paint secondaryPaint;
        protected float mMin;
        protected float mMax;
        protected CircularFloatArray mData;

        private Trace(String name, int color, int secondaryColor, int numValues, float min, float max) {
            mName = name;
            mMin = min;
            mMax = max;
            mData = new CircularFloatArray(numValues);

            paint = new Paint(Paint.ANTI_ALIAS_FLAG);
            paint.setColor(color);
            paint.setStrokeWidth(3.0f);

            secondaryPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
            secondaryPaint.setColor(secondaryColor);
            secondaryPaint.setStrokeWidth(3.0f);
        }

        public void reset() {
            mData.clear();
        }

        public void add(float value, boolean useSecondaryColor) {
            // Take the hit here instead of when drawing.
            final float normalizedValue = Math.min(mMax, Math.max(mMin, value));
            mData.add(normalizedValue, useSecondaryColor);
        }

        public int size() {
            return mData.size();
        }

        /**
         * Fetch a previous value. A delayIndex of 1 will return the most recently written value.
         * A delayIndex of 2 will return the previously written value;
         * @param delayIndex positive index of previously written data
         * @return old value
         */
        public float get(int delayIndex) {
            return mData.get(delayIndex);
        }
        public boolean getUseSecondaryColor(int delayIndex) {
            return mData.getUseSecondaryColor(delayIndex);
        }

        public float getMax() {
            return mMax;
        }
        public float getMin() {
            return mMin;
        }

        public void setMin(float min) {
            mMin = min;
        }
        public void setMax(float max) {
            mMax = max;
        }
    }

    // Use explicit type for performance reasons.
    private static class CircularFloatArray {
        private float[] mData;
        private boolean[] mUseSecondaryColor;
        private int mIndexMask;
        private int mCursor; // next location to be written

        public CircularFloatArray(int numValuesPowerOf2) {
            if ((numValuesPowerOf2 & (numValuesPowerOf2 - 1)) != 0) {
                throw new IllegalArgumentException("numValuesPowerOf2 not 2^N, was " + numValuesPowerOf2);
            }
            mData = new float[numValuesPowerOf2];
            mUseSecondaryColor = new boolean[numValuesPowerOf2];
            mIndexMask = numValuesPowerOf2 - 1;
        }

        /**
         * Add one value to the array.
         * This may overwrite the oldest data.
         * @param value
         */
        public void add(float value, boolean useSecondaryColor) {
            int index = mCursor & mIndexMask;
            mData[index] = value;
            mUseSecondaryColor[index] = useSecondaryColor;
            mCursor++;
        }

        /**
         * Number of valid entries.
         * @return
         */
        public int size() {
            return Math.min(mCursor, mData.length);
        }

        /**
         * Fetch a previous value. A delayIndex of 1 will return the most recently written value.
         * A delayIndex of 2 will return the previously written value;
         * @param delayIndex positive index of previously written data
         * @return old value
         */
        public float get(int delayIndex) {
            int index = (mCursor - delayIndex) & mIndexMask;
            return mData[index];
        }

        public boolean getUseSecondaryColor(int delayIndex) {
            int index = (mCursor - delayIndex) & mIndexMask;
            return mUseSecondaryColor[index];
        }

        public void clear() {
            mCursor = 0;
        }
    }
}