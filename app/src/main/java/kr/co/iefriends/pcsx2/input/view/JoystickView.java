
/*

By MoonPower (Momo-AUX1) GPLv3 License
   This file is part of ARMSX2.

   ARMSX2 is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   ARMSX2 is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with ARMSX2.  If not, see <http://www.gnu.org/licenses/>.

*/

package kr.co.iefriends.pcsx2.input.view;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.PointF;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;

import android.graphics.drawable.Drawable;

import androidx.annotation.Nullable;

public class JoystickView extends View {
    
    private Paint basePaint;
    private Paint knobPaint;
    private Paint strokePaint;
    
    private float centerX, centerY;
    private float baseRadius;
    private float knobRadius;
    private PointF knobPosition;
    private boolean isDragging = false;
    
    private float analogX = 0.0f;
    private float analogY = 0.0f;
    private Drawable baseDrawable;
    private Drawable knobDrawable;
    private float knobScaleFactor = 1.0f;
    
    public interface OnJoystickMoveListener {
        void onJoystickMove(float x, float y);
    }
    
    private OnJoystickMoveListener listener;
    
    public JoystickView(Context context) {
        super(context);
        init();
    }
    
    public JoystickView(Context context, AttributeSet attrs) {
        super(context, attrs);
        init();
    }
    
    public JoystickView(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        init();
    }
    
    private void init() {
        basePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        basePaint.setColor(0x0DFFFFFF); 
        basePaint.setStyle(Paint.Style.FILL);

        knobPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        knobPaint.setColor(0x26FFFFFF); 
        knobPaint.setStyle(Paint.Style.FILL);

        strokePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        strokePaint.setColor(0x66FFFFFF); 
        strokePaint.setStyle(Paint.Style.STROKE);
        strokePaint.setStrokeWidth(2.0f);
        
        knobPosition = new PointF();
    }

    public void setDrawables(@Nullable Drawable base, @Nullable Drawable knob) {
        this.baseDrawable = base != null ? base.mutate() : null;
        this.knobDrawable = knob != null ? knob.mutate() : null;
        updateDrawableBounds();
        invalidate();
    }

    public void setKnobScaleFactor(float factor) {
        float clamped = Math.max(0.1f, Math.min(3.0f, factor));
        if (Math.abs(knobScaleFactor - clamped) < 0.001f) {
            return;
        }
        knobScaleFactor = clamped;
        updateDrawableBounds();
        invalidate();
    }

    private void updateDrawableBounds() {
        if (baseDrawable != null) {
            baseDrawable.setBounds(0, 0, getWidth(), getHeight());
        }
        if (knobDrawable != null) {
            int knobSize = Math.round(knobRadius * knobScaleFactor * 2f);
            int left = Math.round(knobPosition.x - knobSize / 2f);
            int top = Math.round(knobPosition.y - knobSize / 2f);
            knobDrawable.setBounds(left, top, left + knobSize, top + knobSize);
        }
    }
    
    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        
        centerX = w / 2f;
        centerY = h / 2f;
        baseRadius = Math.min(w, h) / 2f - 20; 
        knobRadius = baseRadius * 0.3f;
        
        knobPosition.set(centerX, centerY);
        updateDrawableBounds();
    }
    
    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        if (baseDrawable != null) {
            baseDrawable.draw(canvas);
        } else {
            canvas.drawCircle(centerX, centerY, baseRadius, basePaint);
            canvas.drawCircle(centerX, centerY, baseRadius, strokePaint);
        }

        if (knobDrawable != null) {
            int knobSize = Math.round(knobRadius * knobScaleFactor * 2f);
            int left = Math.round(knobPosition.x - knobSize / 2f);
            int top = Math.round(knobPosition.y - knobSize / 2f);
            knobDrawable.setBounds(left, top, left + knobSize, top + knobSize);
            knobDrawable.draw(canvas);
        } else {
            float drawRadius = knobRadius * knobScaleFactor;
            canvas.drawCircle(knobPosition.x, knobPosition.y, drawRadius, knobPaint);
            canvas.drawCircle(knobPosition.x, knobPosition.y, drawRadius, strokePaint);
        }
    }
    
    @Override
    public boolean onTouchEvent(MotionEvent event) {
        switch (event.getAction()) {
            case MotionEvent.ACTION_DOWN:
                float dx = event.getX() - centerX;
                float dy = event.getY() - centerY;
                float distance = (float) Math.sqrt(dx * dx + dy * dy);
                
                if (distance <= baseRadius) {
                    isDragging = true;
                    updateKnobPosition(event.getX(), event.getY());
                    return true;
                }
                break;
                
            case MotionEvent.ACTION_MOVE:
                if (isDragging) {
                    updateKnobPosition(event.getX(), event.getY());
                    return true;
                }
                break;
                
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL:
                if (isDragging) {
                    isDragging = false;
                    knobPosition.set(centerX, centerY);
                    analogX = 0.0f;
                    analogY = 0.0f;
                    if (listener != null) {
                        listener.onJoystickMove(analogX, analogY);
                    }
                    updateDrawableBounds();
                    invalidate();
                    return true;
                }
                break;
        }
        
        return false;
    }
    
    private void updateKnobPosition(float x, float y) {
        float dx = x - centerX;
        float dy = y - centerY;
        float distance = (float) Math.sqrt(dx * dx + dy * dy);
        float knobVisualRadius = knobRadius * knobScaleFactor;
        float maxDistance = Math.max(0f, baseRadius - knobVisualRadius);
        if (distance <= maxDistance) {
            knobPosition.set(x, y);
            if (maxDistance > 0f) {
                analogX = dx / maxDistance;
                analogY = dy / maxDistance;
            } else {
                analogX = 0f;
                analogY = 0f;
            }
        } else {
            float angle = (float) Math.atan2(dy, dx);
            knobPosition.x = centerX + (float) Math.cos(angle) * maxDistance;
            knobPosition.y = centerY + (float) Math.sin(angle) * maxDistance;
            analogX = (float) Math.cos(angle);
            analogY = (float) Math.sin(angle);
        }
        
        if (listener != null) {
            listener.onJoystickMove(analogX, analogY);
        }
        updateDrawableBounds();
        invalidate();
    }
    
    public void setOnJoystickMoveListener(OnJoystickMoveListener listener) {
        this.listener = listener;
    }
    
    public float getAnalogX() {
        return analogX;
    }
    
    public float getAnalogY() {
        return analogY;
    }
}
