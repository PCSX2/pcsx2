package com.armsx2.config

import kr.co.iefriends.pcsx2.NativeApp
import java.util.concurrent.Executors
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicReference

object LiveGsApplyQueue {
    private val executor = Executors.newSingleThreadExecutor { r ->
        Thread(r, "GSLiveApply").apply { isDaemon = true }
    }

    private val latestSettings = AtomicReference<Settings?>(null)
    private val settingsRunning = AtomicBoolean(false)
    private val latestUpscale = AtomicReference<Float?>(null)
    private val upscaleRunning = AtomicBoolean(false)
    private val latestFramerate = AtomicReference<Pair<Float, Float>?>(null)
    private val framerateRunning = AtomicBoolean(false)

    fun applySettings(settings: Settings) {
        latestSettings.set(settings)
        if (settingsRunning.compareAndSet(false, true))
            executor.execute(::drainSettings)
    }

    fun applyUpscale(multiplier: Float) {
        latestUpscale.set(multiplier)
        if (upscaleRunning.compareAndSet(false, true))
            executor.execute(::drainUpscale)
    }

    /** Live per-region framerate (NTSC, PAL). Coalesced + run off the UI thread
     *  because applyFramerateLive parks the VM (UpdateVSyncRate touches EE-thread
     *  counters). Persists to the native base layer first so a later full
     *  ApplySettings reload can't revert it. */
    fun applyFramerate(ntsc: Float, pal: Float) {
        latestFramerate.set(ntsc to pal)
        if (framerateRunning.compareAndSet(false, true))
            executor.execute(::drainFramerate)
    }

    private fun drainSettings() {
        try {
            while (true) {
                val settings = latestSettings.getAndSet(null) ?: break
                val applied = runCatching { settings.applyGsLive() }
                    .onFailure { println("@@ANDROID_GS_LIVE_QUEUE@@ settings_error=${it.javaClass.simpleName}") }
                    .getOrDefault(false)
                println("@@ANDROID_GS_LIVE_QUEUE@@ settings_applied=${if (applied) 1 else 0}")
            }
        } finally {
            settingsRunning.set(false)
            if (latestSettings.get() != null && settingsRunning.compareAndSet(false, true))
                executor.execute(::drainSettings)
        }
    }

    private fun drainUpscale() {
        try {
            while (true) {
                val multiplier = latestUpscale.getAndSet(null) ?: break
                runCatching { NativeApp.renderUpscalemultiplier(multiplier) }
                    .onFailure { println("@@ANDROID_GS_LIVE_QUEUE@@ upscale_error=${it.javaClass.simpleName}") }
                println("@@ANDROID_GS_LIVE_QUEUE@@ upscale value=$multiplier")
            }
        } finally {
            upscaleRunning.set(false)
            if (latestUpscale.get() != null && upscaleRunning.compareAndSet(false, true))
                executor.execute(::drainUpscale)
        }
    }

    private fun drainFramerate() {
        try {
            while (true) {
                val (ntsc, pal) = latestFramerate.getAndSet(null) ?: break
                runCatching {
                    // Persist to the native base layer so a subsequent full
                    // ApplySettings (e.g. EE-cycle commit) keeps the new rate.
                    NativeApp.setSetting("EmuCore/GS", "FramerateNTSC", "float", ntsc.toString())
                    NativeApp.setSetting("EmuCore/GS", "FrameratePAL", "float", pal.toString())
                    NativeApp.applyFramerateLive(ntsc, pal)
                }.onFailure { println("@@ANDROID_GS_LIVE_QUEUE@@ framerate_error=${it.javaClass.simpleName}") }
                println("@@ANDROID_GS_LIVE_QUEUE@@ framerate ntsc=$ntsc pal=$pal")
            }
        } finally {
            framerateRunning.set(false)
            if (latestFramerate.get() != null && framerateRunning.compareAndSet(false, true))
                executor.execute(::drainFramerate)
        }
    }
}
