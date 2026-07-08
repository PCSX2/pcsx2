/*
 * Copyright 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.google.oboe.samples.rhythmgame;

import android.content.Context;
import android.opengl.GLSurfaceView;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.SurfaceHolder;

public class GameSurfaceView extends GLSurfaceView {

    public static native void native_onTouchInput(int eventType, long timeSinceBootMs, int pixel_x, int pixel_y);
    public static native void native_surfaceDestroyed();
    private final RendererWrapper mRenderer;

    public GameSurfaceView(Context context) {
        super(context);
        setEGLContextClientVersion(2);
        mRenderer = new RendererWrapper();
        // Set the Renderer for drawing on the GLSurfaceView
        setRenderer(mRenderer);
    }

    public GameSurfaceView(Context context, AttributeSet attrs) {
        super(context, attrs);
        setEGLContextClientVersion(2);
        mRenderer = new RendererWrapper();
        // Set the Renderer for drawing on the GLSurfaceView
        setRenderer(mRenderer);
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        native_surfaceDestroyed();
        super.surfaceDestroyed(holder);
    }

    @Override
    public boolean onTouchEvent(MotionEvent e) {
        // MotionEvent reports input details from the touch screen
        // and other input controls. In our case we care about DOWN events.
        switch (e.getAction()) {
            case MotionEvent.ACTION_DOWN:
                native_onTouchInput(0, e.getEventTime(), (int)e.getX(), (int)e.getY());
                break;
        }
        return true;
    }
}
