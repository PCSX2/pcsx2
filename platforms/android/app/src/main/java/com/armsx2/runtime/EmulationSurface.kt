package com.armsx2.runtime

import android.app.Activity
import android.content.Context
import android.content.ContextWrapper
import android.hardware.display.DisplayManager
import android.os.Build
import android.view.Display
import android.view.Surface
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.Window
import android.view.WindowManager
import com.armsx2.config.ConfigStore
import kr.co.iefriends.pcsx2.NativeApp
import kotlin.math.abs
import kotlin.math.roundToInt

class EmulationSurface(context: Context) :
    SurfaceView(context),
    SurfaceHolder.Callback,
    DisplayManager.DisplayListener {
    private var viewWidth = 0
    private var viewHeight = 0
    private var gameActive = false
    private var lastRequestedFrameRate = Float.NaN
    private val displayManager =
        context.getSystemService(Context.DISPLAY_SERVICE) as DisplayManager
    private val frameRateMonitor = object : Runnable {
        override fun run() {
            if (!gameActive || !isAttachedToWindow) return
            applyFrameRatePreference()
            // The VM's nominal rate becomes available after the Surface. Keep this
            // cheap poll alive so PAL/NTSC changes and disc swaps are picked up too.
            postDelayed(this, FRAME_RATE_MONITOR_MS)
        }
    }

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
        displayManager.registerDisplayListener(this, null)
    }

    override fun onDetachedFromWindow() {
        removeCallbacks(frameRateMonitor)
        clearFrameRatePreference()
        displayManager.unregisterDisplayListener(this)
        hostWindow()?.clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        super.onDetachedFromWindow()
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        requestFocus()
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        reportActualDisplayRefreshRate()
        applyFrameRatePreference()
        NativeApp.onNativeSurfaceChanged(holder.surface, width, height)
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        lastRequestedFrameRate = Float.NaN
        NativeApp.onNativeSurfaceChanged(null, 0, 0)
    }

    override fun onDisplayAdded(displayId: Int) = Unit

    override fun onDisplayRemoved(displayId: Int) = Unit

    override fun onDisplayChanged(displayId: Int) {
        if (displayId != currentDisplay()?.displayId) return
        reportActualDisplayRefreshRate()
    }

    /**
     * Called from the Activity's VM-state effect. The high-refresh vote only
     * exists during gameplay; menus and the library remain under Android's
     * normal power-saving policy.
     */
    fun setGameActive(active: Boolean) {
        if (gameActive == active) {
            if (active) applyFrameRatePreference()
            return
        }
        gameActive = active
        removeCallbacks(frameRateMonitor)
        if (active) {
            applyFrameRatePreference()
            postDelayed(frameRateMonitor, FRAME_RATE_RETRY_MS)
        } else {
            clearFrameRatePreference()
        }
    }

    /**
     * Low Latency Mode asks Android 11+ for a high-refresh mode which is an
     * integer multiple of the emulated game's nominal rate. Thus ~60 fps picks
     * 120 Hz and PAL 50 fps prefers 100 Hz, while a 90 Hz-only panel falls back
     * to its ~60 Hz mode instead of introducing 3:2 cadence.
     */
    fun applyFrameRatePreference() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R ||
            !gameActive ||
            !holder.surface.isValid ||
            !lowLatencyEnabled()
        ) {
            clearFrameRatePreference()
            return
        }

        val nominalRate = runCatching { NativeApp.getNominalFrameRate() }
            .getOrDefault(0f)
            .takeIf { it in MIN_GAME_RATE..MAX_GAME_RATE }
            ?: DEFAULT_GAME_RATE
        val requestedRate = preferredDisplayRefreshRate(nominalRate)
        if (!requestedRate.isFinite() || requestedRate <= 0f ||
            abs(requestedRate - lastRequestedFrameRate) < RATE_EPSILON
        ) return

        runCatching {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                holder.surface.setFrameRate(
                    requestedRate,
                    Surface.FRAME_RATE_COMPATIBILITY_DEFAULT,
                    Surface.CHANGE_FRAME_RATE_ONLY_IF_SEAMLESS,
                )
            } else {
                @Suppress("DEPRECATION")
                holder.surface.setFrameRate(
                    requestedRate,
                    Surface.FRAME_RATE_COMPATIBILITY_DEFAULT,
                )
            }
            lastRequestedFrameRate = requestedRate
        }
    }

    private fun clearFrameRatePreference() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R ||
            !holder.surface.isValid ||
            lastRequestedFrameRate.isNaN()
        ) return
        runCatching {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                holder.surface.setFrameRate(
                    0f,
                    Surface.FRAME_RATE_COMPATIBILITY_DEFAULT,
                    Surface.CHANGE_FRAME_RATE_ONLY_IF_SEAMLESS,
                )
            } else {
                @Suppress("DEPRECATION")
                holder.surface.setFrameRate(0f, Surface.FRAME_RATE_COMPATIBILITY_DEFAULT)
            }
        }
        lastRequestedFrameRate = Float.NaN
    }

    private fun lowLatencyEnabled(): Boolean = runCatching {
        val settingsKey = MainActivityRuntime.currentGame.value?.settingsKey
            ?: NativeApp.getGameSerial().takeIf { it.isNotBlank() }
        ConfigStore.resolveForGame(settingsKey)
            .vsyncQueueSize == 0
    }.getOrDefault(false)

    private fun preferredDisplayRefreshRate(nominalRate: Float): Float {
        val current = currentDisplay() ?: return 0f
        val rates = current.supportedModes
            .asSequence()
            // Avoid a refresh vote which also asks Android to change resolution.
            .filter {
                it.physicalWidth == current.mode.physicalWidth &&
                    it.physicalHeight == current.mode.physicalHeight
            }
            .map { it.refreshRate }
            .filter { it > 0f }
            .distinct()
            .toList()
        if (rates.isEmpty()) return current.refreshRate

        val integerMultiples = rates.filter { rate ->
            val multiple = (rate / nominalRate).roundToInt()
            multiple >= 2 && abs(rate - nominalRate * multiple) <= RATE_MATCH_TOLERANCE_HZ
        }
        integerMultiples.maxOrNull()?.let { return it }

        // No clean high-refresh multiple (for example 60 fps on a 90 Hz-only
        // panel): preserve even pacing by choosing the closest native-rate mode.
        return rates.minBy { abs(it - nominalRate) }
    }

    private fun reportActualDisplayRefreshRate() {
        NativeApp.setDisplayRefreshRate(currentDisplayRefreshHz())
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

    private fun currentDisplay(): Display? =
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) context.display else display

    private fun currentDisplayRefreshHz(): Float =
        runCatching { currentDisplay()?.refreshRate ?: 0f }.getOrDefault(0f)

    private companion object {
        const val DEFAULT_GAME_RATE = 59.94f
        const val MIN_GAME_RATE = 20f
        const val MAX_GAME_RATE = 125f
        const val RATE_EPSILON = 0.05f
        const val RATE_MATCH_TOLERANCE_HZ = 0.6f
        const val FRAME_RATE_RETRY_MS = 500L
        const val FRAME_RATE_MONITOR_MS = 5_000L
    }
}
