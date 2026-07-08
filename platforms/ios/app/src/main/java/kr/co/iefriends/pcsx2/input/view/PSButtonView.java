package kr.co.iefriends.pcsx2.input.view;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.RectF;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;

import androidx.annotation.DrawableRes;
import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;

public class PSButtonView extends View {
    
    private Paint basePaint;
    private Paint pressedPaint;
    private Paint strokePaint;
    private Paint symbolPaint;
    
    private RectF bounds;
    private boolean isPressed = false;
    private String symbol = "";
    private int symbolColor = 0xFFFFFFFF; 
    private boolean isRectangular = false;
    private Drawable iconDrawable;
    
    public interface OnPSButtonListener {
        void onButtonPressed(boolean pressed);
    }
    
    private OnPSButtonListener listener;
    
    public PSButtonView(Context context) {
        super(context);
        init();
    }
    
    public PSButtonView(Context context, AttributeSet attrs) {
        super(context, attrs);
        init();
    }
    
    public PSButtonView(Context context, AttributeSet attrs, int defStyleAttr) {
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
        
        symbolPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        symbolPaint.setTextAlign(Paint.Align.CENTER);
        symbolPaint.setStyle(Paint.Style.FILL);
        
        bounds = new RectF();
    }
    
    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        
        float padding = Math.min(w, h) * 0.1f;
        bounds.set(padding, padding, w - padding, h - padding);
        
        symbolPaint.setTextSize(Math.min(w, h) * 0.4f);
        updateIconBounds();
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);

        float centerX = bounds.centerX();
        float centerY = bounds.centerY();

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
        
        if (isRectangular) {
            float cornerRadius = Math.min(bounds.width(), bounds.height()) * 0.1f;
            canvas.drawRoundRect(bounds, cornerRadius, cornerRadius, fillPaint);
            canvas.drawRoundRect(bounds, cornerRadius, cornerRadius, strokePaint);
        } else {
            float radius = Math.min(bounds.width(), bounds.height()) / 2f;
            canvas.drawCircle(centerX, centerY, radius, fillPaint);
            canvas.drawCircle(centerX, centerY, radius, strokePaint);
        }
        
        if (!symbol.isEmpty()) {
            symbolPaint.setColor(symbolColor);
            
            if ("▢".equals(symbol)) {
                drawSquareSymbol(canvas, centerX, centerY, Math.min(bounds.width(), bounds.height()) / 2f * 0.4f);
            } else if ("△".equals(symbol)) {
                drawTriangleSymbol(canvas, centerX, centerY, Math.min(bounds.width(), bounds.height()) / 2f * 0.4f);
            } else if ("⨉".equals(symbol)) {
                drawXSymbol(canvas, centerX, centerY, Math.min(bounds.width(), bounds.height()) / 2f * 0.4f);
            } else if ("○".equals(symbol)) {
                drawCircleSymbol(canvas, centerX, centerY, Math.min(bounds.width(), bounds.height()) / 2f * 0.4f);
            } else if ("▷".equals(symbol)) {
                drawRightTriangleSymbol(canvas, centerX, centerY, Math.min(bounds.width(), bounds.height()) / 2f * 0.4f);
            } else {
                float textY = centerY + symbolPaint.getTextSize() / 3;
                canvas.drawText(symbol, centerX, textY, symbolPaint);
            }
        }
    }
    
    private void drawSquareSymbol(Canvas canvas, float centerX, float centerY, float size) {
        Paint squarePaint = new Paint(symbolPaint);
        squarePaint.setStyle(Paint.Style.STROKE);
        squarePaint.setStrokeWidth(size * 0.15f);
        
        RectF square = new RectF(
            centerX - size, centerY - size,
            centerX + size, centerY + size
        );
        canvas.drawRect(square, squarePaint);
    }
    
    private void drawTriangleSymbol(Canvas canvas, float centerX, float centerY, float size) {
        Paint trianglePaint = new Paint(symbolPaint);
        trianglePaint.setStyle(Paint.Style.STROKE);
        trianglePaint.setStrokeWidth(size * 0.15f);
        trianglePaint.setStrokeJoin(Paint.Join.ROUND);
        
        Path triangle = new Path();
        triangle.moveTo(centerX, centerY - size);
        triangle.lineTo(centerX - size * 0.866f, centerY + size * 0.5f);
        triangle.lineTo(centerX + size * 0.866f, centerY + size * 0.5f);
        triangle.close();
        
        canvas.drawPath(triangle, trianglePaint);
    }
    
    private void drawXSymbol(Canvas canvas, float centerX, float centerY, float size) {
        Paint xPaint = new Paint(symbolPaint);
        xPaint.setStyle(Paint.Style.STROKE);
        xPaint.setStrokeWidth(size * 0.15f);
        xPaint.setStrokeCap(Paint.Cap.ROUND);
        
        canvas.drawLine(centerX - size, centerY - size, centerX + size, centerY + size, xPaint);
        canvas.drawLine(centerX + size, centerY - size, centerX - size, centerY + size, xPaint);
    }
    
    private void drawCircleSymbol(Canvas canvas, float centerX, float centerY, float size) {
        Paint circlePaint = new Paint(symbolPaint);
        circlePaint.setStyle(Paint.Style.STROKE);
        circlePaint.setStrokeWidth(size * 0.15f);
        
        canvas.drawCircle(centerX, centerY, size, circlePaint);
    }
    
    private void drawRightTriangleSymbol(Canvas canvas, float centerX, float centerY, float size) {
        Paint trianglePaint = new Paint(symbolPaint);
        trianglePaint.setStyle(Paint.Style.FILL);
        
        Path triangle = new Path();
        triangle.moveTo(centerX - size * 0.4f, centerY - size * 0.6f);
        triangle.lineTo(centerX + size * 0.6f, centerY);
        triangle.lineTo(centerX - size * 0.4f, centerY + size * 0.6f);
        triangle.close();
        
        canvas.drawPath(triangle, trianglePaint);
    }
    
    @Override
    public boolean onTouchEvent(MotionEvent event) {
        switch (event.getAction()) {
            case MotionEvent.ACTION_DOWN:
                boolean hitTest;
                if (isRectangular) {
                    hitTest = bounds.contains(event.getX(), event.getY());
                } else {
                    float dx = event.getX() - bounds.centerX();
                    float dy = event.getY() - bounds.centerY();
                    float radius = Math.min(bounds.width(), bounds.height()) / 2f;
                    float distance = (float) Math.sqrt(dx * dx + dy * dy);
                    hitTest = distance <= radius;
                }
                
                if (hitTest) {
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
    
    public void setSymbol(String symbol) {
        this.iconDrawable = null;
        this.symbol = symbol;
        invalidate();
    }

    public void setSymbolColor(int color) {
        this.symbolColor = color;
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

    public void setOnPSButtonListener(OnPSButtonListener listener) {
        this.listener = listener;
    }

    public void setRectangular(boolean rectangular) {
        this.isRectangular = rectangular;
        invalidate();
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
