/*
 * Copyright 2021 The Android Open Source Project
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

package com.google.oboe.samples.soundboard

import android.app.Activity
import android.content.Context
import android.content.res.Configuration
import android.graphics.Point
import android.graphics.Rect
import android.media.AudioManager
import android.os.Build
import android.os.Bundle
import android.view.WindowInsets
import android.view.WindowManager
import androidx.annotation.RequiresApi
import androidx.appcompat.app.AppCompatActivity
import kotlin.math.min

class MainActivity : AppCompatActivity() {
    private external fun startEngine(numSignals: Int): Long
    private external fun stopEngine(engineHandle: Long)
    private external fun native_setDefaultStreamValues(sampleRate: Int, framesPerBurst: Int)

    companion object {
        private const val DIMENSION_MIN_SIZE = 6
        private const val DIMENSION_MAX_SIZE = 8
        private var mNumColumns : Int = 0;
        private var mNumRows : Int = 0;
        private var mTiles = ArrayList<Rect>()
        private var mBorders = ArrayList<Rect>()

        private var mEngineHandle: Long = 0

        // Used to load the 'native-lib' library on application startup.
        init {
            System.loadLibrary("soundboard")
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
    }

    override fun onResume() {
        super.onResume()
        setup()
    }

    override fun onPause() {
        stopEngine(mEngineHandle)
        super.onPause()
    }

    private fun setup() {
        setDefaultStreamValues(this)
        calculateAndSetRectangles(this)
        mEngineHandle = startEngine(mNumRows * mNumColumns)
        createMusicTiles(this)
    }

    private fun setDefaultStreamValues(context: Context) {
        val myAudioMgr = context.getSystemService(AUDIO_SERVICE) as AudioManager
        val sampleRateStr = myAudioMgr.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE)
        val defaultSampleRate = sampleRateStr.toInt()
        val framesPerBurstStr =
            myAudioMgr.getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER)
        val defaultFramesPerBurst = framesPerBurstStr.toInt()
        native_setDefaultStreamValues(defaultSampleRate, defaultFramesPerBurst)
    }

    private fun calculateAndSetRectangles(context: Context) {
        val windowManager = context.getSystemService(Context.WINDOW_SERVICE) as WindowManager
        val display = windowManager.defaultDisplay
        val size = Point()

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            val windowMetrics = windowManager.currentWindowMetrics
            val windowInsets = windowMetrics.windowInsets
            val width = windowMetrics.bounds.width()
            val height = windowMetrics.bounds.height()
            size.set(width, height)
        } else {
            display.getSize(size) // Use getSize to exclude navigation bar if visible
        }

        val width = size.x
        val height = size.y

        if (height > width) {
            mNumColumns = DIMENSION_MIN_SIZE
            mNumRows = min(DIMENSION_MIN_SIZE * height / width, DIMENSION_MAX_SIZE)
        } else {
            mNumRows = DIMENSION_MIN_SIZE
            mNumColumns = min(DIMENSION_MIN_SIZE * width / height, DIMENSION_MAX_SIZE)
        }
        val tileLength = min(height / mNumRows, width / mNumColumns)
        val xStartLocation = (width - tileLength * mNumColumns) / 2
        val yStartLocation = (height - tileLength * mNumRows) / 2

        mTiles = ArrayList<Rect>()
        for (i in 0 until mNumRows) {
            for (j in 0 until mNumColumns) {
                val rectangle = Rect(
                    xStartLocation + j * tileLength,
                    yStartLocation + i * tileLength,
                    xStartLocation + j * tileLength + tileLength,
                    yStartLocation + i * tileLength + tileLength
                )
                mTiles.add(rectangle)
            }
        }

        mBorders = ArrayList<Rect>()
        // Top border
        mBorders.add(Rect(0, 0, width, yStartLocation))
        // Bottom border
        mBorders.add(Rect(0, yStartLocation + tileLength * mNumRows, width, height))
        // Left border
        mBorders.add(Rect(0, 0, xStartLocation, height))
        // Right border
        mBorders.add(Rect(xStartLocation + tileLength * mNumColumns, 0, width, height))
    }

    private fun createMusicTiles(context: Context) {
        setContentView(MusicTileView(this, mTiles, mBorders, NoteListener(mEngineHandle),
                ScreenChangeListener { setup() }))
    }

    class ScreenChangeListener(private var mFunc: () -> Unit) : MusicTileView.ConfigChangeListener {
        override fun onConfigurationChanged() {
            mFunc()
        }
    }

}
