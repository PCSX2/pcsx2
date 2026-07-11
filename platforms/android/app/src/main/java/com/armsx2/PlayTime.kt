package com.armsx2

import com.armsx2.runtime.MainActivityRuntime

import android.os.SystemClock
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/**
 * Per-game play-time + last-played tracking, persisted in [MainActivityRuntime.prefs] keyed by
 * PS2 serial. A "session" runs while the VM is RUNNING; pausing, stopping or
 * backgrounding accumulates the elapsed time into the per-serial total. Shown in
 * the Game Properties info tab.
 *
 * Session length uses the monotonic [SystemClock.elapsedRealtime] (immune to
 * wall-clock changes); the last-played stamp uses real time.
 */
object PlayTime {
    private const val SECS_PREFIX = "playtime.secs."
    private const val LAST_PREFIX = "playtime.last."

    private var sessionSerial: String? = null
    private var sessionStart: Long = 0L

    /** Begin counting for [serial], closing any open session first. No-op for a
     *  blank serial (BIOS boot / unidentified disc). */
    @Synchronized
    fun startSession(serial: String?) {
        endSession()
        val s = serial?.takeIf { it.isNotEmpty() } ?: return
        sessionSerial = s
        sessionStart = SystemClock.elapsedRealtime()
        MainActivityRuntime.prefs.edit().putLong(LAST_PREFIX + s, System.currentTimeMillis()).apply()
    }

    /** Accumulate the open session's elapsed time into its serial's total. */
    @Synchronized
    fun endSession() {
        val s = sessionSerial ?: return
        sessionSerial = null
        val elapsedMs = SystemClock.elapsedRealtime() - sessionStart
        if (elapsedMs < 1000L) return
        val prev = MainActivityRuntime.prefs.getLong(SECS_PREFIX + s, 0L)
        MainActivityRuntime.prefs.edit().putLong(SECS_PREFIX + s, prev + elapsedMs / 1000L).apply()
    }

    fun playedSeconds(serial: String?): Long =
        serial?.takeIf { it.isNotEmpty() }?.let { MainActivityRuntime.prefs.getLong(SECS_PREFIX + it, 0L) } ?: 0L

    fun lastPlayedMillis(serial: String?): Long =
        serial?.takeIf { it.isNotEmpty() }?.let { MainActivityRuntime.prefs.getLong(LAST_PREFIX + it, 0L) } ?: 0L

    /** "" when zero, else e.g. "2h 15m", "45m", "30s". */
    fun formatPlayed(seconds: Long): String {
        if (seconds <= 0L) return ""
        val h = seconds / 3600L
        val m = (seconds % 3600L) / 60L
        return when {
            h > 0L -> "${h}h ${m}m"
            m > 0L -> "${m}m"
            else -> "${seconds}s"
        }
    }

    /** "" when never played, else e.g. "Jun 26, 2026 04:02". */
    fun formatLastPlayed(millis: Long): String {
        if (millis <= 0L) return ""
        return SimpleDateFormat("MMM d, yyyy HH:mm", Locale.getDefault()).format(Date(millis))
    }
}
