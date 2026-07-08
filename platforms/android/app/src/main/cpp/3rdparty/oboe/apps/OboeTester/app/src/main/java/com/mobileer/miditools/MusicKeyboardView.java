/*
 * Copyright (C) 2015 The Android Open Source Project
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

package com.mobileer.miditools;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;

import java.util.ArrayList;
import java.util.HashMap;

/**
 * View that displays a traditional piano style keyboard. Finger presses are reported to a
 * MusicKeyListener. Keys that pressed are highlighted. Running a finger along the top of the
 * keyboard will only hit black keys. Running a finger along the bottom of the keyboard will only
 * hit white keys.
 */
public class MusicKeyboardView extends View {
    // Adjust proportions of the keys.
    private static final int WHITE_KEY_GAP = 10;
    private static final int PITCH_MIDDLE_C = 60;
    private static final int NOTES_PER_OCTAVE = 12;
    private static final int[] WHITE_KEY_OFFSETS = {
            0, 2, 4, 5, 7, 9, 11
    };
    private static final double BLACK_KEY_HEIGHT_FACTOR = 0.60;
    private static final double BLACK_KEY_WIDTH_FACTOR = 0.6;
    private static final double BLACK_KEY_OFFSET_FACTOR = 0.18;

    private static final int[] BLACK_KEY_HORIZONTAL_OFFSETS = {
            -1, 1, -1, 0, 1
    };
    private static final boolean[] NOTE_IN_OCTAVE_IS_BLACK = {
            false, true,
            false, true,
            false, false, true,
            false, true,
            false, true,
            false
    };

    // Preferences
    private int mNumKeys;
    private int mNumPortraitKeys = NOTES_PER_OCTAVE + 1;
    private int mNumLandscapeKeys = (2 * NOTES_PER_OCTAVE) + 1;
    private int mNumWhiteKeys = 15;

    // Geometry.
    private int mWidth;
    private int mHeight;
    private int mWhiteKeyWidth;
    private double mBlackKeyWidth;
    // Y position of bottom of black keys.
    private int mBlackBottom;
    private Rect[] mBlackKeyRectangles;

    // Keyboard state
    private boolean[] mNotesOnByPitch = new boolean[128];

    // Appearance
    private Paint mShadowPaint;
    private Paint mBlackOnKeyPaint;
    private Paint mBlackOffKeyPaint;
    private Paint mWhiteOnKeyPaint;
    private Paint mWhiteOffKeyPaint;
    private boolean mLegato = true;

    private HashMap<Integer, Integer> mFingerMap = new HashMap<Integer, Integer>();
    // Note number for the left most key.
    private int mLowestPitch = PITCH_MIDDLE_C - NOTES_PER_OCTAVE;
    private ArrayList<MusicKeyListener> mListeners = new ArrayList<MusicKeyListener>();

    /** Implement this to receive keyboard events. */
    public interface MusicKeyListener {
        /** This will be called when a key is pressed. */
        public void onKeyDown(int keyIndex);

        /** This will be called when a key is pressed. */
        public void onKeyUp(int keyIndex);
    }

    public MusicKeyboardView(Context context, AttributeSet attrs) {
        super(context, attrs);
        init();
    }

    void init() {
        mShadowPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        mShadowPaint.setStyle(Paint.Style.FILL);
        mShadowPaint.setColor(0xFF707070);

        mBlackOnKeyPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        mBlackOnKeyPaint.setStyle(Paint.Style.FILL);
        mBlackOnKeyPaint.setColor(0xFF2020E0);

        mBlackOffKeyPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        mBlackOffKeyPaint.setStyle(Paint.Style.FILL);
        mBlackOffKeyPaint.setColor(0xFF202020);

        mWhiteOnKeyPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        mWhiteOnKeyPaint.setStyle(Paint.Style.FILL);
        mWhiteOnKeyPaint.setColor(0xFF6060F0);

        mWhiteOffKeyPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        mWhiteOffKeyPaint.setStyle(Paint.Style.FILL);
        mWhiteOffKeyPaint.setColor(0xFFF0F0F0);

    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        mWidth = w;
        mHeight = h;
        mNumKeys = (mHeight > mWidth) ? mNumPortraitKeys : mNumLandscapeKeys;
        mNumWhiteKeys = 0;
        // Count white keys.
        for (int i = 0; i < mNumKeys; i++) {
            int pitch = mLowestPitch + i;
            if (!isPitchBlack(pitch)) {
                mNumWhiteKeys++;
            }
        }

        mWhiteKeyWidth = mWidth / mNumWhiteKeys;
        mBlackKeyWidth = mWhiteKeyWidth * BLACK_KEY_WIDTH_FACTOR;
        mBlackBottom = (int) (mHeight * BLACK_KEY_HEIGHT_FACTOR);

        makeBlackRectangles();
    }

    private void makeBlackRectangles() {
        int top = 0;
        ArrayList<Rect> rectangles = new ArrayList<Rect>();

        int whiteKeyIndex = 0;
        int blackKeyIndex = 0;
        for (int i = 0; i < mNumKeys; i++) {
            int x = mWhiteKeyWidth * whiteKeyIndex;
            int pitch = mLowestPitch + i;
            int note = pitch % NOTES_PER_OCTAVE;
            if (NOTE_IN_OCTAVE_IS_BLACK[note]) {
                double offset = BLACK_KEY_OFFSET_FACTOR
                        * BLACK_KEY_HORIZONTAL_OFFSETS[blackKeyIndex % 5];
                int left = (int) (x - mBlackKeyWidth * (0.6 - offset));
                left += WHITE_KEY_GAP / 2;
                int right = (int) (left + mBlackKeyWidth);
                Rect rect = new Rect(left, top, right, mBlackBottom);
                rectangles.add(rect);
                blackKeyIndex++;
            } else {
                whiteKeyIndex++;
            }
        }
        mBlackKeyRectangles = rectangles.toArray(new Rect[0]);
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        int whiteKeyIndex = 0;
        canvas.drawRect(0, 0, mWidth, mHeight, mShadowPaint);
        // Draw white keys first.
        for (int i = 0; i < mNumKeys; i++) {
            int pitch = mLowestPitch + i;
            int note = pitch % NOTES_PER_OCTAVE;
            if (!NOTE_IN_OCTAVE_IS_BLACK[note]) {
                int x = (mWhiteKeyWidth * whiteKeyIndex) + (WHITE_KEY_GAP / 2);
                Paint paint = mNotesOnByPitch[pitch] ? mWhiteOnKeyPaint
                        : mWhiteOffKeyPaint;
                canvas.drawRect(x, 0, x + mWhiteKeyWidth - WHITE_KEY_GAP, mHeight,
                        paint);
                whiteKeyIndex++;
            }
        }
        // Then draw black keys over the white keys.
        int blackKeyIndex = 0;
        for (int i = 0; i < mNumKeys; i++) {
            int pitch = mLowestPitch + i;
            int note = pitch % NOTES_PER_OCTAVE;
            if (NOTE_IN_OCTAVE_IS_BLACK[note]) {
                Rect r = mBlackKeyRectangles[blackKeyIndex];
                Paint paint = mNotesOnByPitch[pitch] ? mBlackOnKeyPaint
                        : mBlackOffKeyPaint;
                canvas.drawRect(r, paint);
                blackKeyIndex++;
            }
        }
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        super.onTouchEvent(event);
        int action = event.getActionMasked();
        // Track individual fingers.
        int pointerIndex = event.getActionIndex();
        int id = event.getPointerId(pointerIndex);
        // Get the pointer's current position
        float x = event.getX(pointerIndex);
        float y = event.getY(pointerIndex);
        switch (action) {
            case MotionEvent.ACTION_DOWN:
            case MotionEvent.ACTION_POINTER_DOWN:
                onFingerDown(id, x, y);
                break;
            case MotionEvent.ACTION_MOVE:
                onFingerMove(id, x, y);
                break;
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_POINTER_UP:
                onFingerUp(id, x, y);
                break;
        }
        // Must return true or we do not get the ACTION_MOVE and
        // ACTION_UP events.
        return true;
    }

    private void onFingerDown(int id, float x, float y) {
        int pitch = xyToPitch(x, y);
        fireKeyDown(pitch);
        mFingerMap.put(id, pitch);
    }

    private void onFingerMove(int id, float x, float y) {
        Integer previousPitch = mFingerMap.get(id);
        if (previousPitch != null) {
            int pitch = -1;
            if (y < mBlackBottom) {
                // Only hit black keys if above line.
                pitch = xyToBlackPitch(x, y);
            } else {
                pitch = xToWhitePitch(x);
            }
            // Did we change to a new key.
            if ((pitch >= 0) && (pitch != previousPitch)) {
                if (mLegato) {
                    fireKeyDown(pitch);
                    fireKeyUp(previousPitch);
                } else {
                    fireKeyUp(previousPitch);
                    fireKeyDown(pitch);
                }
                mFingerMap.put(id, pitch);
            }
        }
    }

    private void onFingerUp(int id, float x, float y) {
        Integer previousPitch = mFingerMap.get(id);
        if (previousPitch != null) {
            fireKeyUp(previousPitch);
            mFingerMap.remove(id);
        } else {
            int pitch = xyToPitch(x, y);
            fireKeyUp(pitch);
        }
    }

    private void fireKeyDown(int pitch) {
        for (MusicKeyListener listener : mListeners) {
            listener.onKeyDown(pitch);
        }
        mNotesOnByPitch[pitch] = true;
        invalidate();
    }

    private void fireKeyUp(int pitch) {
        for (MusicKeyListener listener : mListeners) {
            listener.onKeyUp(pitch);
        }
        mNotesOnByPitch[pitch] = false;
        invalidate();
    }

    private int xyToPitch(float x, float y) {
        int pitch = -1;
        if (y < mBlackBottom) {
            pitch = xyToBlackPitch(x, y);
        }
        if (pitch < 0) {
            pitch = xToWhitePitch(x);
        }
        return pitch;
    }

    private boolean isPitchBlack(int pitch) {
        int note = pitch % NOTES_PER_OCTAVE;
        return NOTE_IN_OCTAVE_IS_BLACK[note];
    }

    // Convert x to MIDI pitch. Ignores black keys.
    private int xToWhitePitch(float x) {
        int whiteKeyIndex = (int) (x / mWhiteKeyWidth);
        int octave = whiteKeyIndex / WHITE_KEY_OFFSETS.length;
        int indexInOctave = whiteKeyIndex - (octave * WHITE_KEY_OFFSETS.length);
        int pitch = mLowestPitch + (octave * NOTES_PER_OCTAVE) +
                WHITE_KEY_OFFSETS[indexInOctave];
        return pitch;
    }

    // Convert x to MIDI pitch. Ignores white keys.
    private int xyToBlackPitch(float x, float y) {
        int result = -1;
        int blackKeyIndex = 0;
        for (int i = 0; i < mNumKeys; i++) {
            int pitch = mLowestPitch + i;
            if (isPitchBlack(pitch)) {
                Rect rect = mBlackKeyRectangles[blackKeyIndex];
                if (rect.contains((int) x, (int) y)) {
                    result = pitch;
                    break;
                }
                blackKeyIndex++;
            }
        }
        return result;
    }

    public void addMusicKeyListener(MusicKeyListener musicKeyListener) {
        mListeners.add(musicKeyListener);
    }

    public void removeMusicKeyListener(MusicKeyListener musicKeyListener) {
        mListeners.remove(musicKeyListener);
    }

    /**
     * Set the pitch of the lowest, leftmost key. If you set it to a black key then it will get
     * adjusted upwards to a white key. Forces a redraw.
     */
    public void setLowestPitch(int pitch) {
        if (isPitchBlack(pitch)) {
            pitch++; // force to next white key
        }
        mLowestPitch = pitch;
        postInvalidate();
    }

    public int getLowestPitch() {
        return mLowestPitch;
    }

    /**
     * Set the number of white keys in portrait mode.
     */
    public void setNumPortraitKeys(int numPortraitKeys) {
        mNumPortraitKeys = numPortraitKeys;
        postInvalidate();
    }

    public int getNumPortraitKeys() {
        return mNumPortraitKeys;
    }

    /**
     * Set the number of white keys in landscape mode.
     */
    public void setNumLandscapeKeys(int numLandscapeKeys) {
        mNumLandscapeKeys = numLandscapeKeys;
        postInvalidate();
    }

    public int getNumLandscapeKeys() {
        return mNumLandscapeKeys;
    }
}
