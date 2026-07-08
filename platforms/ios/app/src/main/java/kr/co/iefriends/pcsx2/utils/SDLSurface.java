package kr.co.iefriends.pcsx2.utils;

import android.content.Context;
import android.util.AttributeSet;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import androidx.annotation.NonNull;

import kr.co.iefriends.pcsx2.NativeApp;
import kr.co.iefriends.pcsx2.activities.MainActivity;

public class SDLSurface extends SurfaceView implements SurfaceHolder.Callback {
    public SDLSurface(Context p_context) {
        super(p_context);
        myInit();
    }

    public SDLSurface(Context p_context, AttributeSet attrs) {
        super(p_context, attrs);
        myInit();
    }

    public SDLSurface(Context p_context, AttributeSet attrs, int defStyle) {
        super(p_context, attrs, defStyle);
        myInit();
    }

    private void myInit() {
        getHolder().addCallback(this);
    }

    // Called when we have a valid drawing surface
    @Override
    public void surfaceCreated(@NonNull SurfaceHolder p_holder) {
    }

    // Called when the surface is resized
    @Override
    public void surfaceChanged(@NonNull SurfaceHolder p_holder, int p_format, int p_width, int p_height) {
        NativeApp.onNativeSurfaceChanged(p_holder.getSurface(), p_width, p_height);
        ////
        MainActivity _nativeActivity = (MainActivity) getContext();
        if(_nativeActivity != null) {
            _nativeActivity.onSurfaceReady();
        }
    }

    // Called when we lose the surface
    @Override
    public void surfaceDestroyed(@NonNull SurfaceHolder p_holder) {
        NativeApp.onNativeSurfaceChanged(null, 0, 0);
    }
}
