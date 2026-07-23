package com.armsx2.runtime

import android.app.Activity
import android.content.Context
import android.content.ContextWrapper
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.Window
import android.view.WindowManager
import kr.co.iefriends.pcsx2.NativeApp

class EmulationSurface(context: Context) : SurfaceView(context), SurfaceHolder.Callback {
    private var viewWidth = 0
    private var viewHeight = 0

    init {
        holder.addCallback(this)
        isFocusable = true
        isFocusableInTouchMode = true
        defaultFocusHighlightEnabled = false
        keepScreenOn = true
    }

    private fun hostWindow(): Window? {
        var current: Context? = context
        while (current is ContextWrapper) {
            if (current is Activity) return current.window
            current = current.baseContext
        }
        return null
    }

    override fun onAttachedToWindow() {
        super.onAttachedToWindow()
        hostWindow()?.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
    }

    override fun onDetachedFromWindow() {
        hostWindow()?.clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        super.onDetachedFromWindow()
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        requestFocus()
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        NativeApp.setDisplayRefreshRate(currentDisplayRefreshHz())
        NativeApp.onNativeSurfaceChanged(holder.surface, width, height)
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        NativeApp.onNativeSurfaceChanged(null, 0, 0)
    }

    override fun onSizeChanged(width: Int, height: Int, oldWidth: Int, oldHeight: Int) {
        super.onSizeChanged(width, height, oldWidth, oldHeight)
        viewWidth = width
        viewHeight = height
        applyOutputScale()
    }

    fun applyOutputScale() {
        if (viewWidth <= 0 || viewHeight <= 0) return
        val multiplier = MainActivityRuntime.prefs.getInt("ui.hwScaler", 0)

        // Base output resolution: an explicit "WxH" override when set (fixes wrong panel detection,
        // e.g. a 1920x1080 panel mis-reported as 1920x1200 which squishes 16:9 games — issue #398),
        // otherwise the SurfaceView's laid-out size.
        val override = parseResOverride(MainActivityRuntime.prefs.getString("ui.screenResOverride", null))
        val baseW = override?.first ?: viewWidth
        val baseH = override?.second ?: viewHeight

        // hwScaler (if > 0) downscales the short side to 448*multiplier for weak GPUs, applied to
        // whatever base we picked above.
        val shortSide = minOf(baseW, baseH)
        val targetShortSide = if (multiplier > 0) 448 * multiplier else shortSide
        val scale = if (targetShortSide in 1 until shortSide) targetShortSide.toFloat() / shortSide else 1f

        // With no override and no downscale, let the surface track the layout (native panel size) —
        // the original behavior. Otherwise pin an explicit buffer size and let the compositor scale.
        if (override == null && scale == 1f) {
            holder.setSizeFromLayout()
            return
        }
        holder.setFixedSize(
            (baseW * scale).toInt().coerceAtLeast(1),
            (baseH * scale).toInt().coerceAtLeast(1),
        )
    }

    /** Parse a "WIDTHxHEIGHT" override pref (e.g. "1920x1080") to a size, or null for "auto"/unset/bad. */
    private fun parseResOverride(value: String?): Pair<Int, Int>? {
        if (value.isNullOrBlank() || value == "auto") return null
        val m = Regex("^\\s*(\\d{2,5})\\s*[xX]\\s*(\\d{2,5})\\s*$").find(value) ?: return null
        val w = m.groupValues[1].toIntOrNull() ?: return null
        val h = m.groupValues[2].toIntOrNull() ?: return null
        return if (w in 64..8192 && h in 64..8192) Pair(w, h) else null
    }

    private fun currentDisplayRefreshHz(): Float = runCatching {
        val currentDisplay = if (android.os.Build.VERSION.SDK_INT >= 30) context.display else display
        currentDisplay?.refreshRate ?: 0f
    }.getOrDefault(0f)
}
