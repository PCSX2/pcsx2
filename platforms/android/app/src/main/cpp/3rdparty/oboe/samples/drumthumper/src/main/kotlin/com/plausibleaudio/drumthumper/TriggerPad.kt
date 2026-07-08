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
package com.plausiblesoftware.drumthumper

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.RectF
import android.util.AttributeSet
import android.util.TypedValue
import android.view.MotionEvent
import android.view.View

class TriggerPad: View {

    private val mDrawRect = RectF()
    private val mPaint = Paint()

    private val mUpColor = Color.LTGRAY
    private val mDownColor = Color.DKGRAY
    private var mIsDown = false

    private var mText = "DrumPad"
    private var mTextSizeSp = 28.0f

    private val mTextColor = Color.BLACK

    val DISPLAY_MASK        = 0x00000003
    val DISPLAY_RECT        = 0x00000000
    val DISPLAY_CIRCLE      = 0x00000001
    val DISPLAY_ROUND_RECT  = 0x00000002

    private var mDisplayFlags = DISPLAY_ROUND_RECT

    interface DrumPadTriggerListener {
        fun triggerDown(pad: TriggerPad)
        fun triggerUp(pad: TriggerPad)
    }

    var mListeners = ArrayList<DrumPadTriggerListener>()

    constructor(context: Context) : super(context)

    constructor(context: Context, attrs: AttributeSet) : super(context, attrs) {
        extractAttributes(attrs)
    }

    constructor(context: Context, attrs: AttributeSet, defStyle: Int): super(context, attrs, defStyle) {
        extractAttributes(attrs)
    }

    //
    // Attributes
    //
    private fun extractAttributes(attrs: AttributeSet) {
        val xmlns = "http://schemas.android.com/apk/res/android"
        val textVal = attrs.getAttributeValue(xmlns, "text")
        if (textVal != null) {
            mText = textVal
        }
    }

    //
    // Layout Routines
    //
    private fun calcTextSizeInPixels(): Float {
        return TypedValue.applyDimension(
                TypedValue.COMPLEX_UNIT_SP,
                mTextSizeSp,
                resources.displayMetrics
        )
    }

    override fun onSizeChanged(w: Int, h: Int, oldw: Int, oldh: Int) {
        val padLeft = paddingLeft
        val padRight = paddingRight
        val padTop = paddingTop
        val padBottom = paddingBottom

        mDrawRect.set(padLeft.toFloat(),
                padTop.toFloat(),
                w - padRight.toFloat(),
                h - padBottom.toFloat())

        // mTextSize = mDrawRect.bottom / 4.0f
    }

    override fun onMeasure (widthMeasureSpec: Int, heightMeasureSpec: Int) {
        val width = MeasureSpec.getSize(widthMeasureSpec)

        val padTop = paddingTop
        val padBottom = paddingBottom

        val heightMode = MeasureSpec.getMode(heightMeasureSpec)
        var height = MeasureSpec.getSize(heightMeasureSpec)

        val textSizePixels = calcTextSizeInPixels()
        when (heightMode) {
            MeasureSpec.AT_MOST -> run {
                // mText = "AT_MOST"
                val newHeight = (textSizePixels.toInt() * 2) + padTop + padBottom
                height = minOf(height, newHeight) }

            MeasureSpec.EXACTLY -> run {
                /*mText = "EXACTLY"*/ }

            MeasureSpec.UNSPECIFIED -> run {
                // mText = "UNSPECIFIED"
                height = textSizePixels.toInt() }
        }

        setMeasuredDimension(width, height)
    }

    //
    // Drawing Routines
    //
    override fun onDraw(canvas: Canvas) {
        // Face
        if (mIsDown) {
            mPaint.color = mDownColor
        } else {
            mPaint.color = mUpColor
        }

        when (mDisplayFlags and DISPLAY_MASK) {
            DISPLAY_RECT -> canvas.drawRect(mDrawRect, mPaint)

            DISPLAY_CIRCLE -> run {
                val midX = mDrawRect.left + mDrawRect.width() / 2.0f
                val midY = mDrawRect.top + mDrawRect.height() / 2.0f
                val radius = minOf(mDrawRect.height() / 2.0f, mDrawRect.width() / 2.0f)
                canvas.drawCircle(midX, midY, radius - 5.0f, mPaint)
            }

            DISPLAY_ROUND_RECT -> run {
                val rad = minOf(mDrawRect.width() / 8.0f, mDrawRect.height() / 8.0f)
                canvas.drawRoundRect(mDrawRect, rad, rad, mPaint)
            }
        }

        // Text
        val midX = mDrawRect.width() / 2
        mPaint.textSize = calcTextSizeInPixels()
        val textWidth = mPaint.measureText(mText)
        mPaint.color = mTextColor
        val textSizePixels = calcTextSizeInPixels()
        canvas.drawText(mText, mDrawRect.left + midX - textWidth / 2,
                mDrawRect.bottom/2 + textSizePixels/2, mPaint)

    }

    //
    // Input Routines
    //
    override fun onTouchEvent(event: MotionEvent): Boolean {
        if (event.actionMasked == MotionEvent.ACTION_DOWN ||
                event.actionMasked == MotionEvent.ACTION_POINTER_DOWN) {
            mIsDown = true
            triggerDown()
            invalidate()
            return true
        } else if (event.actionMasked == MotionEvent.ACTION_UP) {
            mIsDown = false
            triggerUp()
            invalidate()
            return true
        }

        return false
    }

    //
    // Event Listeners
    //
    fun addListener(listener: DrumPadTriggerListener) {
        mListeners.add(listener)
    }

    private fun triggerDown() {
        for( listener in mListeners) {
            listener.triggerDown(this)
        }
    }

    private fun triggerUp() {
        for( listener in mListeners) {
            listener.triggerUp(this)
        }
    }
}
