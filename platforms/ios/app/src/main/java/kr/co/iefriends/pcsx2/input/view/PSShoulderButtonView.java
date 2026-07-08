package kr.co.iefriends.pcsx2.input.view;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.RectF;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;

import androidx.annotation.DrawableRes;
import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;

public class PSShoulderButtonView extends View {
    
    private Paint basePaint;
    private Paint pressedPaint;
    private Paint strokePaint;
    private Paint textPaint;
    
    private RectF bounds;
    private boolean isPressed = false;
    private String label = "";
    private int textColor = 0xFFFFFFFF; 
    private Drawable iconDrawable;
    
    public interface OnPSShoulderButtonListener {
        void onButtonPressed(boolean pressed);
    }
    
    private OnPSShoulderButtonListener listener;
    
    public PSShoulderButtonView(Context context) {
        super(context);
        init();
    }
    
    public PSShoulderButtonView(Context context, AttributeSet attrs) {
        super(context, attrs);
        init();
    }
    
    public PSShoulderButtonView(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        init();
    }
    
    private void init() {
        basePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        basePaint.setColor(0x60FFFFFF);
        basePaint.setStyle(Paint.Style.FILL);
        
        pressedPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        pressedPaint.setColor(0xFFFFFFFF);
        pressedPaint.setStyle(Paint.Style.FILL);
        
        strokePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        strokePaint.setColor(0x80000000);
        strokePaint.setStyle(Paint.Style.STROKE);
        strokePaint.setStrokeWidth(2.0f);
        
        textPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        textPaint.setTextAlign(Paint.Align.CENTER);
        textPaint.setStyle(Paint.Style.FILL);
        textPaint.setColor(textColor);
        
        bounds = new RectF();
    }
    
    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        
        float padding = Math.min(w, h) * 0.1f;
        bounds.set(padding, padding, w - padding, h - padding);
        
        textPaint.setTextSize(Math.min(w, h) * 0.35f);
        updateIconBounds();
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);

        if (iconDrawable != null) {
            iconDrawable.setBounds(
                    Math.round(bounds.left),
                    Math.round(bounds.top),
                    Math.round(bounds.right),
                    Math.round(bounds.bottom)
            );
            iconDrawable.setState(isPressed ? new int[]{android.R.attr.state_pressed} : new int[]{});
            iconDrawable.setAlpha(isPressed ? 200 : 255);
            iconDrawable.draw(canvas);
            return;
        }

        Paint fillPaint = isPressed ? pressedPaint : basePaint;
        
        float cornerRadius = Math.min(bounds.width(), bounds.height()) * 0.15f;
        canvas.drawRoundRect(bounds, cornerRadius, cornerRadius, fillPaint);
        canvas.drawRoundRect(bounds, cornerRadius, cornerRadius, strokePaint);
        
        if (!label.isEmpty()) {
            textPaint.setColor(isPressed ? 0xFF000000 : textColor);
            float textY = bounds.centerY() + textPaint.getTextSize() / 3;
            canvas.drawText(label, bounds.centerX(), textY, textPaint);
        }
    }
    
    @Override
    public boolean onTouchEvent(MotionEvent event) {
        switch (event.getAction()) {
            case MotionEvent.ACTION_DOWN:
                if (bounds.contains(event.getX(), event.getY())) {
                    isPressed = true;
                    if (listener != null) {
                        listener.onButtonPressed(true);
                    }
                    invalidate();
                    return true;
                }
                break;
                
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL:
                if (isPressed) {
                    isPressed = false;
                    if (listener != null) {
                        listener.onButtonPressed(false);
                    }
                    invalidate();
                    return true;
                }
                break;
        }
        
        return false;
    }

    public void setLabel(String label) {
        this.iconDrawable = null;
        this.label = label;
        invalidate();
    }

    public void setTextColor(int color) {
        this.textColor = color;
        invalidate();
    }

    public void setIconResource(@DrawableRes int resId) {
        if (resId == 0) {
            iconDrawable = null;
        } else {
            iconDrawable = AppCompatResources.getDrawable(getContext(), resId);
            if (iconDrawable != null) {
                iconDrawable = iconDrawable.mutate();
                updateIconBounds();
            }
        }
        invalidate();
    }

    public void setIconDrawable(@Nullable Drawable drawable) {
        if (drawable == null) {
            iconDrawable = null;
        } else {
            iconDrawable = drawable.mutate();
            updateIconBounds();
        }
        invalidate();
    }

    public void setOnPSShoulderButtonListener(OnPSShoulderButtonListener listener) {
        this.listener = listener;
    }

    private void updateIconBounds() {
        if (iconDrawable == null) {
            return;
        }
        iconDrawable.setBounds(
                Math.round(bounds.left),
                Math.round(bounds.top),
                Math.round(bounds.right),
                Math.round(bounds.bottom)
        );
    }
}
