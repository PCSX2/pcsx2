package com.armsx2.ui.settingshub

import com.armsx2.config.Settings
import com.armsx2.navigation.SettingsCategory
import org.json.JSONObject

/**
 * Which [Settings] fields each settings tab owns, so Reset can scope to the tab you're
 * looking at instead of wiping everything.
 *
 * The Reset button used to call `Settings()` (or clear the whole per-game override blob),
 * which reset EVERY tab: pressing Reset on the Renderer page also wiped Audio, Network and
 * Fixes. Controller settings appeared to survive only because they live in a separate store
 * (ControllerMappings), not because Reset was scoped — the Controls tab owns no Settings
 * fields at all, which is why it has no entry below.
 *
 * Names are the JSON keys from [Settings.toJson], which IS the persistence format
 * (ConfigStore.saveGlobal stores `toJson().toString()` and loads it back through
 * `Settings.fromJson`). That round-trip is therefore known-complete, which is what lets
 * [resetCategory] swap individual values against a default Settings() safely.
 *
 * KEEP IN SYNC with the matching *Tab.kt when you add a setting — a field missing here just
 * won't be reset by its tab's button (it is never destructive, only incomplete).
 */
internal val SETTINGS_CATEGORY_FIELDS: Map<SettingsCategory, List<String>> = mapOf(
    // PerformanceTab.kt
    SettingsCategory.Performance to listOf(
        "eeClampMode", "eeCycleRate", "eeCycleSkip", "eeFpuRoundMode", "enableFastBoot",
        "enableGameFixes", "fastCDVD", "fpsLimit", "frameSkip", "framerateNtsc", "frameratePal",
        "intcStat", "mtvu", "nominalSpeedPercent", "skipDuplicateFrames", "vu0RoundMode",
        "vu1Instant", "vu1RoundMode", "vuClampMode", "vuDeferredWrites", "vuFlagHack",
        "vuNeonFusions", "vuSkipStallSim", "waitLoop",
    ),
    // RendererTab.kt
    SettingsCategory.Graphics to listOf(
        "accurateBlendingUnit", "adrenoFbFetch", "aspectRatio", "casMode", "casSharpness",
        "deinterlaceMode", "displayBilinear", "dumpReplaceableTextures", "fmvAspectRatio",
        "forceMaliFbFetch", "fxaa", "gpuProfile", "gsBackThreadMode", "hardwareDownloadMode",
        "hwAa1", "hwAccurateAlphaTest", "hwMipmap", "hwRov", "loadTextureReplacements",
        "loadTextureReplacementsAsync", "maxAnisotropy", "orientation",
        "osdShowTextureReplacements", "portraitRenderTop", "precacheTextureReplacements",
        "shadeBoost", "shadeBoostBrightness", "shadeBoostContrast", "shadeBoostGamma",
        "shadeBoostSaturation", "shaderChainEnabled", "shaderChainParams", "shaderChainPreset",
        "textureFiltering", "texturePreloading", "triFilter", "tvShader", "upscaleFloat",
        "vsyncEnable",
    ),
    // AudioTab.kt
    SettingsCategory.Audio to listOf(
        "audioBufferMs", "audioFastForwardVolume", "audioMuted", "audioOutputLatencyMs",
        "audioSwapChannels", "audioTimeStretch", "audioVolume", "spu2NeonReverb",
    ),
    // NetworkTab.kt
    SettingsCategory.Network to listOf(
        "dev9AutoGateway", "dev9AutoMask", "dev9Dns1", "dev9Dns2", "dev9EthApi", "dev9EthDevice",
        "dev9EthEnable", "dev9EthHosts", "dev9EthLogDhcp", "dev9EthLogDns", "dev9Gateway",
        "dev9HddEnable", "dev9HddFile", "dev9InterceptDhcp", "dev9Mask", "dev9ModeDns1",
        "dev9ModeDns2", "dev9Ps2Ip", "ip", "url", "usbKeyboard",
    ),
    // OverlayTab.kt
    SettingsCategory.OnScreen to listOf(
        "osdColor", "osdScale", "osdShowCpu", "osdShowFps", "osdShowFrameTimes", "osdShowGpu",
        "osdShowGpuStats", "osdShowGsStats", "osdShowHardwareInfo", "osdShowInputs",
        "osdShowMessages", "osdShowResolution", "osdShowSettings", "osdShowSpeed",
        "osdShowVersion", "osdShowVps",
    ),
    // FixesTab.kt
    SettingsCategory.Advanced to listOf(
        "alignSprite", "antiBlur", "autoFlush", "autoFlushSw", "bilinearUpscale", "cpuClutRender",
        "cpuFramebufferConversion", "cpuSpriteRenderBw", "cpuSpriteRenderLevel", "cropBottom",
        "cropLeft", "cropRight", "cropTop", "displayZoom", "disableDepthEmulation", "disableFramebufferFetch",
        "disableInterlaceOffset", "disablePartialInvalidation", "disableRenderFixes",
        "disableSafeFeatures", "disableShaderCache", "disableVertexShaderExpand", "dithering",
        "drawBuffering", "estimateTextureRegion", "forceEvenSpritePosition",
        "gpuPaletteConversion", "gpuTargetClut", "halfPixelOffset", "hwAccurateAlphaTest",
        "integerScaling", "limit24BitDepth", "manualUserHacks", "mergeSprite", "mipmapSw",
        "nativeScaling", "overrideTextureBarriers", "preloadFrameData", "readTargetsWhenClosing",
        "roundSprite", "screenOffsets", "showOverscan", "skipDrawEnd", "skipDrawStart",
        "spinCpuReadbacks", "spinGpuReadbacks", "swThreads", "swThreadsHeight",
        "syncToHostRefresh", "textureInsideRt", "textureOffsetX", "textureOffsetY",
        "unscaledPaletteDraw", "useBlitSwapChain", "vsyncQueueSize",
    ),
    // RecompilerTab.kt
    SettingsCategory.Recompiler to listOf(
        "enableFastmem", "recEE", "recIOP", "recVU0", "recVU1",
    ),
    // Controls / Hotkeys / Skins / General / Info / Patches / About own no Settings fields —
    // Controls keeps its binds and tunables in ControllerMappings and has its own reset row.
)

/** Default values for just this category's fields, leaving every other tab untouched. */
internal fun Settings.resetCategory(category: SettingsCategory): Settings {
    val fields = SETTINGS_CATEGORY_FIELDS[category] ?: return this
    val current = toJson()
    val defaults = Settings().toJson()
    for (key in fields) {
        if (defaults.has(key)) current.put(key, defaults.get(key)) else current.remove(key)
    }
    return Settings.fromJson(current)
}

/** The per-game override keys belonging to [category], for a scoped per-game reset. */
internal fun categoryOverrideKeys(category: SettingsCategory): List<String> =
    SETTINGS_CATEGORY_FIELDS[category].orEmpty()

/** True when this tab has anything the Reset button could restore. */
internal fun categoryHasResettableSettings(category: SettingsCategory): Boolean =
    !SETTINGS_CATEGORY_FIELDS[category].isNullOrEmpty()

/** Strip [keys] from a per-game override blob; null when nothing is left to store. */
internal fun pruneOverrides(overrides: JSONObject, keys: List<String>): JSONObject? {
    for (key in keys) overrides.remove(key)
    return if (overrides.length() == 0) null else overrides
}
