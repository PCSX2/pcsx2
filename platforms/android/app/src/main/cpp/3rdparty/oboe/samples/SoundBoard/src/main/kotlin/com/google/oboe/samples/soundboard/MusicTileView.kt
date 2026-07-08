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

import android.content.Context
import android.content.res.Configuration
import android.graphics.*
import android.util.SparseArray
import android.view.MotionEvent
import android.view.View
import androidx.core.content.ContextCompat

class MusicTileView(
    context: Context?,
    private val mTiles: ArrayList<Rect>,
    private val mBorders: ArrayList<Rect>,
    tileListener: TileListener,
    configChangeListener: ConfigChangeListener
) : View(context) {
    private val mIsPressedPerRectangle: BooleanArray = BooleanArray(mTiles.size)
    private val mPaint: Paint = Paint()
    private val mLocationsOfFingers: SparseArray<PointF> = SparseArray()
    private val mTileListener: TileListener
    private val mConfigChangeListener : ConfigChangeListener

    interface ConfigChangeListener {
        fun onConfigurationChanged()
    }

    interface TileListener {
        fun onTileOn(index: Int)
        fun onTileOff(index: Int)
    }

    private fun getIndexFromLocation(pointF: PointF): Int {
        for (i in mTiles.indices) {
            if (pointF.x > mTiles[i].left && pointF.x < mTiles[i].right && pointF.y > mTiles[i].top && pointF.y < mTiles[i].bottom) {
                return i
            }
        }
        return -1
    }

    override fun onDraw(canvas: Canvas) {
        for (i in mTiles.indices) {
            mPaint.style = Paint.Style.FILL
            if (mIsPressedPerRectangle[i]) {
                mPaint.color = ContextCompat.getColor(context, R.color.colorPrimary)
            } else {
                mPaint.color = Color.BLACK
            }
            canvas.drawRect(mTiles[i], mPaint)

            // white border
            mPaint.style = Paint.Style.STROKE
            mPaint.strokeWidth = 10f
            mPaint.color = Color.WHITE
            canvas.drawRect(mTiles[i], mPaint)
        }
        for (i in mBorders.indices) {
            mPaint.style = Paint.Style.FILL
            mPaint.color = ContextCompat.getColor(context, R.color.colorPrimaryDark)
            canvas.drawRect(mBorders[i], mPaint)
        }
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        val pointerIndex = event.actionIndex
        val pointerId = event.getPointerId(pointerIndex)
        val maskedAction = event.actionMasked
        var didImageChange = false
        when (maskedAction) {
            MotionEvent.ACTION_MOVE -> {

                // Create an array to check for finger changes as multiple fingers may be on the
                // same tile. This two-pass algorithm records the overall difference before changing
                // the actual tiles.
                val notesChangedBy = IntArray(mTiles.size)
                run {
                    val size = event.pointerCount
                    var i = 0
                    while (i < size) {
                        val point = mLocationsOfFingers[event.getPointerId(i)]
                        if (point != null) {
                            val prevIndex = getIndexFromLocation(point)
                            point.x = event.getX(i)
                            point.y = event.getY(i)
                            val newIndex = getIndexFromLocation(point)
                            if (newIndex != prevIndex) {
                                if (prevIndex != -1) {
                                    notesChangedBy[prevIndex]--
                                }
                                if (newIndex != -1) {
                                    notesChangedBy[newIndex]++
                                }
                            }
                        }
                        i++
                    }
                }

                // Now go through the rectangles to see if they have changed
                var i = 0
                while (i < mTiles.size) {
                    if (notesChangedBy[i] > 0) {
                        mIsPressedPerRectangle[i] = true
                        mTileListener.onTileOn(i)
                        didImageChange = true
                    } else if (notesChangedBy[i] < 0) {
                        mIsPressedPerRectangle[i] = false
                        mTileListener.onTileOff(i)
                        didImageChange = true
                    }
                    i++
                }
            }
            MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN -> {
                val f = PointF()
                f.x = event.getX(pointerIndex)
                f.y = event.getY(pointerIndex)
                mLocationsOfFingers.put(pointerId, f)
                val curIndex = getIndexFromLocation(f)
                if (curIndex != -1) {
                    mIsPressedPerRectangle[curIndex] = true
                    mTileListener.onTileOn(curIndex)
                    didImageChange = true
                }
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_POINTER_UP, MotionEvent.ACTION_CANCEL -> {
                val curIndex =
                    getIndexFromLocation(mLocationsOfFingers[event.getPointerId(pointerIndex)])
                if (curIndex != -1) {
                    mIsPressedPerRectangle[curIndex] = false
                    mTileListener.onTileOff(curIndex)
                    didImageChange = true
                }
                mLocationsOfFingers.remove(pointerId)
            }
        }

        // Calling invalidate() will force onDraw() to be called
        if (didImageChange) {
            invalidate()
        }
        return true
    }

    override fun onConfigurationChanged(newConfig: Configuration) {
        super.onConfigurationChanged(newConfig)
        mConfigChangeListener.onConfigurationChanged()
    }

    init {
        mTileListener = tileListener
        mConfigChangeListener = configChangeListener
    }
}
