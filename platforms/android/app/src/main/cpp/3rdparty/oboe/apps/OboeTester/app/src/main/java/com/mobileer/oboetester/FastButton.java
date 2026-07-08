/*
 * Copyright 2015 The Android Open Source Project
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
import android.view.MotionEvent;
import android.widget.TextView;

import java.util.ArrayList;

/**
 * Button-like View that responds quickly to touch events.
 */
public class FastButton extends TextView {

    public FastButton(Context context) {
        super(context);
    }

    public FastButton(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public FastButton(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
    }

    private ArrayList<FastButtonListener> mListeners = new ArrayList<FastButtonListener>();

    /**
     * Implement this to receive keyboard events.
     */
    public interface FastButtonListener {
        /**
         * This will be called when a key is pressed.
         */
        public void onKeyDown(int id);

        /**
         * This will be called when a key is pressed.
         */
        public void onKeyUp(int id);
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        super.onTouchEvent(event);
        int action = event.getActionMasked();
        // Track individual fingers.
        int pointerIndex = event.getActionIndex();
        int id = event.getPointerId(pointerIndex);
        switch (action) {
            case MotionEvent.ACTION_DOWN:
            case MotionEvent.ACTION_POINTER_DOWN:
                fireKeyDown(id);
                break;
            case MotionEvent.ACTION_MOVE:
                break;
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_POINTER_UP:
                fireKeyUp(id);
                break;
        }
        // Must return true or we do not get the ACTION_MOVE and
        // ACTION_UP events.
        return true;
    }

    private void fireKeyDown(int id) {
        for (FastButtonListener listener : mListeners) {
            listener.onKeyDown(id);
        }
        invalidate();
    }

    private void fireKeyUp(int id) {
        for (FastButtonListener listener : mListeners) {
            listener.onKeyUp(id);
        }
        invalidate();
    }

    public void addFastButtonListener(FastButtonListener listener) {
        mListeners.add(listener);
    }

    public void removeFastButtonListener(FastButtonListener listener) {
        mListeners.remove(listener);
    }
}
