
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
import android.graphics.Path;
import android.graphics.RectF;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;

import android.graphics.drawable.Drawable;
import androidx.annotation.Nullable;

/**
 * Custom D-pad view for PCSX2 directional control
 */
public class DPadView extends View {
    
    private Paint basePaint;
    private Paint pressedPaint;
    private Paint strokePaint;
    
    private RectF bounds;
    private Path dpadPath;
    
    // D-pad regions
    private RectF upRegion, downRegion, leftRegion, rightRegion, centerRegion;
    private Drawable upDrawable, downDrawable, leftDrawable, rightDrawable;
    private Drawable baseDrawable;
    private Drawable pressedDrawable;
    
    // Press states
    private boolean upPressed = false;
    private boolean downPressed = false;
    private boolean leftPressed = false;
    private boolean rightPressed = false;
    
    public interface OnDPadListener {
        void onDirectionPressed(Direction direction, boolean pressed);
    }
    
    public enum Direction {
        UP, DOWN, LEFT, RIGHT
    }
    
    private OnDPadListener listener;
    
    public DPadView(Context context) {
        super(context);
        init();
    }
    
    public DPadView(Context context, AttributeSet attrs) {
        super(context, attrs);
        init();
    }
    
    public DPadView(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        init();
    }
    
    private void init() {
        // Base D-pad color
        basePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        basePaint.setColor(0x1AFFFFFF); 
        basePaint.setStyle(Paint.Style.FILL);
        
        // Pressed state color
        pressedPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        pressedPaint.setColor(0x4DFFFFFF); 
        pressedPaint.setStyle(Paint.Style.FILL);
        
        // Stroke for outline
        strokePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        strokePaint.setColor(0x80FFFFFF); 
        strokePaint.setStyle(Paint.Style.STROKE);
        strokePaint.setStrokeWidth(3.0f);
        
        bounds = new RectF();
        dpadPath = new Path();
        
        upRegion = new RectF();
        downRegion = new RectF();
        leftRegion = new RectF();
        rightRegion = new RectF();
        centerRegion = new RectF();
    }

    public void setDrawables(@Nullable Drawable base, @Nullable Drawable pressed) {
        baseDrawable = base != null ? base.mutate() : null;
        pressedDrawable = pressed != null ? pressed.mutate() : null;
        updateDrawableBounds();
        invalidate();
    }

    public void setDirectionalDrawables(@Nullable Drawable up, @Nullable Drawable down,
                                        @Nullable Drawable left, @Nullable Drawable right) {
        upDrawable = up != null ? up.mutate() : null;
        downDrawable = down != null ? down.mutate() : null;
        leftDrawable = left != null ? left.mutate() : null;
        rightDrawable = right != null ? right.mutate() : null;
        updateDirectionalBounds();
        invalidate();
    }

    private void updateDrawableBounds() {
        if (baseDrawable != null) {
            baseDrawable.setBounds(Math.round(bounds.left), Math.round(bounds.top),
                    Math.round(bounds.right), Math.round(bounds.bottom));
        }
        if (pressedDrawable != null) {
            pressedDrawable.setBounds(Math.round(bounds.left), Math.round(bounds.top),
                    Math.round(bounds.right), Math.round(bounds.bottom));
        }
        updateDirectionalBounds();
    }

    private void updateDirectionalBounds() {
        if (upDrawable != null) {
            upDrawable.setBounds(Math.round(upRegion.left), Math.round(upRegion.top),
                    Math.round(upRegion.right), Math.round(upRegion.bottom));
        }
        if (downDrawable != null) {
            downDrawable.setBounds(Math.round(downRegion.left), Math.round(downRegion.top),
                    Math.round(downRegion.right), Math.round(downRegion.bottom));
        }
        if (leftDrawable != null) {
            leftDrawable.setBounds(Math.round(leftRegion.left), Math.round(leftRegion.top),
                    Math.round(leftRegion.right), Math.round(leftRegion.bottom));
        }
        if (rightDrawable != null) {
            rightDrawable.setBounds(Math.round(rightRegion.left), Math.round(rightRegion.top),
                    Math.round(rightRegion.right), Math.round(rightRegion.bottom));
        }
    }
    
    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        
        float size = Math.min(w, h);
        float padding = size * 0.1f;
        float centerX = w / 2f;
        float centerY = h / 2f;
        
        bounds.set(padding, padding, w - padding, h - padding);
        
        // Create D-pad cross shape
        createDPadPath();
        
        // Define touch regions
        float armWidth = size * 0.3f;
        float armLength = size * 0.35f;
        
        // Up region
        upRegion.set(centerX - armWidth/2, bounds.top, 
                    centerX + armWidth/2, centerY - armWidth/2);
        
        // Down region  
        downRegion.set(centerX - armWidth/2, centerY + armWidth/2,
                      centerX + armWidth/2, bounds.bottom);
        
        // Left region
        leftRegion.set(bounds.left, centerY - armWidth/2,
                      centerX - armWidth/2, centerY + armWidth/2);
        
        // Right region
        rightRegion.set(centerX + armWidth/2, centerY - armWidth/2,
                       bounds.right, centerY + armWidth/2);
        
        // Center region
        centerRegion.set(centerX - armWidth/2, centerY - armWidth/2,
                        centerX + armWidth/2, centerY + armWidth/2);
        updateDrawableBounds();
    }
    
    private void createDPadPath() {
        dpadPath.reset();
        
        float centerX = bounds.centerX();
        float centerY = bounds.centerY();
        float armWidth = bounds.width() * 0.3f;
        float armLength = bounds.width() * 0.35f;
        
        dpadPath.moveTo(centerX - armWidth/2, bounds.top);
        dpadPath.lineTo(centerX + armWidth/2, bounds.top);
        dpadPath.lineTo(centerX + armWidth/2, centerY - armWidth/2);
        dpadPath.lineTo(bounds.right, centerY - armWidth/2);
        dpadPath.lineTo(bounds.right, centerY + armWidth/2);
        dpadPath.lineTo(centerX + armWidth/2, centerY + armWidth/2);
        dpadPath.lineTo(centerX + armWidth/2, bounds.bottom);
        dpadPath.lineTo(centerX - armWidth/2, bounds.bottom);
        dpadPath.lineTo(centerX - armWidth/2, centerY + armWidth/2);
        dpadPath.lineTo(bounds.left, centerY + armWidth/2);
        dpadPath.lineTo(bounds.left, centerY - armWidth/2);
        dpadPath.lineTo(centerX - armWidth/2, centerY - armWidth/2);
        dpadPath.close();
    }
    
    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        boolean hasDirectionalIcons = upDrawable != null || downDrawable != null || leftDrawable != null || rightDrawable != null;

        if (baseDrawable != null) {
            baseDrawable.draw(canvas);
            if (pressedDrawable != null && (upPressed || downPressed || leftPressed || rightPressed)) {
                pressedDrawable.draw(canvas);
            }
        } else if (!hasDirectionalIcons) {
            canvas.drawPath(dpadPath, basePaint);
            if (upPressed) {
                canvas.drawRect(upRegion, pressedPaint);
            }
            if (downPressed) {
                canvas.drawRect(downRegion, pressedPaint);
            }
            if (leftPressed) {
                canvas.drawRect(leftRegion, pressedPaint);
            }
            if (rightPressed) {
                canvas.drawRect(rightRegion, pressedPaint);
            }
            canvas.drawPath(dpadPath, strokePaint);
        }

        if (hasDirectionalIcons) {
            if (upDrawable != null) {
                upDrawable.setAlpha(upPressed ? 255 : 180);
                upDrawable.draw(canvas);
            }
            if (downDrawable != null) {
                downDrawable.setAlpha(downPressed ? 255 : 180);
                downDrawable.draw(canvas);
            }
            if (leftDrawable != null) {
                leftDrawable.setAlpha(leftPressed ? 255 : 180);
                leftDrawable.draw(canvas);
            }
            if (rightDrawable != null) {
                rightDrawable.setAlpha(rightPressed ? 255 : 180);
                rightDrawable.draw(canvas);
            }
        } else {
            Paint textPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
            textPaint.setColor(0xCCFFFFFF);
            textPaint.setTextSize(bounds.width() * 0.12f);
            textPaint.setTextAlign(Paint.Align.CENTER);
            
            float textY = bounds.centerY() + textPaint.getTextSize() / 3;
            
            canvas.drawText("▲", bounds.centerX(), upRegion.centerY() + textPaint.getTextSize() / 3, textPaint);
            canvas.drawText("▼", bounds.centerX(), downRegion.centerY() + textPaint.getTextSize() / 3, textPaint);
            canvas.drawText("◀", leftRegion.centerX(), textY, textPaint);
            canvas.drawText("▶", rightRegion.centerX(), textY, textPaint);
        }
    }
    
    @Override
    public boolean onTouchEvent(MotionEvent event) {
        float x = event.getX();
        float y = event.getY();
        
        switch (event.getAction()) {
            case MotionEvent.ACTION_DOWN:
            case MotionEvent.ACTION_MOVE:
                updatePressStates(x, y);
                return true;
                
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL:
                releaseAllDirections();
                return true;
        }
        
        return false;
    }
    
    private void updatePressStates(float x, float y) {
        boolean newUp = upRegion.contains(x, y);
        boolean newDown = downRegion.contains(x, y);
        boolean newLeft = leftRegion.contains(x, y);
        boolean newRight = rightRegion.contains(x, y);
        
        if (upPressed != newUp) {
            upPressed = newUp;
            if (listener != null) {
                listener.onDirectionPressed(Direction.UP, newUp);
            }
        }
        
        if (downPressed != newDown) {
            downPressed = newDown;
            if (listener != null) {
                listener.onDirectionPressed(Direction.DOWN, newDown);
            }
        }
        
        if (leftPressed != newLeft) {
            leftPressed = newLeft;
            if (listener != null) {
                listener.onDirectionPressed(Direction.LEFT, newLeft);
            }
        }
        
        if (rightPressed != newRight) {
            rightPressed = newRight;
            if (listener != null) {
                listener.onDirectionPressed(Direction.RIGHT, newRight);
            }
        }
        
        invalidate();
    }
    
    private void releaseAllDirections() {
        if (upPressed || downPressed || leftPressed || rightPressed) {
            if (upPressed && listener != null) {
                listener.onDirectionPressed(Direction.UP, false);
            }
            if (downPressed && listener != null) {
                listener.onDirectionPressed(Direction.DOWN, false);
            }
            if (leftPressed && listener != null) {
                listener.onDirectionPressed(Direction.LEFT, false);
            }
            if (rightPressed && listener != null) {
                listener.onDirectionPressed(Direction.RIGHT, false);
            }
            
            upPressed = downPressed = leftPressed = rightPressed = false;
            invalidate();
        }
    }
    
    public void setOnDPadListener(OnDPadListener listener) {
        this.listener = listener;
    }
}
