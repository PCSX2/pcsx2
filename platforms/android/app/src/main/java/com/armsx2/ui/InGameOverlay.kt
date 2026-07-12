package com.armsx2.ui

import android.content.Intent
import androidx.activity.compose.BackHandler
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.displayCutoutPadding
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.material3.Text
import androidx.compose.ui.graphics.ColorFilter
import androidx.compose.ui.graphics.vector.ImageVector
import compose.icons.LineAwesomeIcons
import compose.icons.lineawesomeicons.CompactDiscSolid
import compose.icons.lineawesomeicons.CubeSolid
import compose.icons.lineawesomeicons.EyeSlashSolid
import compose.icons.lineawesomeicons.EyeSolid
import compose.icons.lineawesomeicons.FolderOpenSolid
import compose.icons.lineawesomeicons.PlaySolid
import compose.icons.lineawesomeicons.PowerOffSolid
import compose.icons.lineawesomeicons.RedoAltSolid
import compose.icons.lineawesomeicons.SaveSolid
import compose.icons.lineawesomeicons.SdCardSolid
import compose.icons.lineawesomeicons.TachometerAltSolid
import compose.icons.lineawesomeicons.ThLargeSolid
import androidx.compose.animation.core.LinearEasing
import androidx.compose.animation.core.RepeatMode
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.keyframes
import androidx.compose.animation.core.rememberInfiniteTransition
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.foundation.focusable
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.focus.onFocusChanged
import androidx.compose.ui.draw.shadow
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.runtime.setValue
import androidx.compose.ui.draw.clipToBounds
import androidx.compose.runtime.rememberCoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import androidx.compose.ui.layout.onSizeChanged
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.rememberTextMeasurer
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import coil.compose.SubcomposeAsyncImage
import coil.request.ImageRequest
import com.armsx2.EmuState
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import com.armsx2.CustomCovers
import com.armsx2.GameInfo
import com.armsx2.Main
import com.armsx2.PlayTime
import com.armsx2.R
import com.armsx2.config.ConfigStore
import com.armsx2.config.LiveGsApplyQueue
import com.armsx2.config.Settings
import com.armsx2.config.SettingsScope
import com.armsx2.ui.settings.AppTab
import com.armsx2.ui.settings.AudioTab
import com.armsx2.ui.settings.controllerFocusable
import com.armsx2.ui.settings.FixesTab
import com.armsx2.ui.settings.HotkeysTab
import com.armsx2.ui.settings.NetworkTab
import com.armsx2.ui.settings.OverlayTab
import com.armsx2.ui.settings.PadTab
import com.armsx2.ui.settings.PatchesTab
import com.armsx2.ui.settings.PerformanceTab
import com.armsx2.ui.settings.RecompilerTab
import com.armsx2.ui.settings.RendererTab
import com.armsx2.ui.settings.SettingsControllerNav
import com.armsx2.ui.settings.SkinsTab
import kr.co.iefriends.pcsx2.NativeApp

/**
 * In-game pause overlay. Layout matches the look of pcsx2-qt's in-game
 * fullscreen menu:
 *
 *   ┌──────────────────────────────────────────────────────┐
 *   │ Game Title                          ARMSX2  [logo]   │
 *   │ SLUS-12345 · ★★★★☆                  v2.7.304-3       │
 *   │                                                       │
 *   │              (game surface visible through            │
 *   │               the partial-alpha backdrop)             │
 *   │                                                       │
 *   │ Resume Game                                           │
 *   │ Show Toolbar                                          │
 *   │ Save State                                            │
 *   │ ...                                                   │
 *   │ Close Game                                            │
 *   └──────────────────────────────────────────────────────┘
 *
 * Trigger: long-press on the game surface while RUNNING (Main.kt's
 * detectTapGestures handler). Open auto-pauses; close paths inside the
 * overlay decide whether to resume.
 *
 * Mirrors PCSX2 ImGui FullscreenUI's DrawPauseMenu (FullscreenUI.cpp:1549+)
 * minus features that aren't wired on the Android port (achievements,
 * per-game props, screenshot snapshot).
 */
object InGameOverlay {
    private sealed class State {
        data object Root : State()
        data object SaveStateSlots : State()
        data object LoadStateSlots : State()
        data object ExitConfirm : State()
        data object ResetConfirm : State()
        data object AchievementsLogin : State()
        data object Achievements : State()
        data object HardcoreEnableConfirm : State()
        data object HardcoreDisableConfirm : State()
        data object HardcoreSaveStateBlocked : State()
    }

    private val state = mutableStateOf<State>(State.Root)
    private val settingsOnly = mutableStateOf(false)
    private val playSelection = mutableStateOf(0)
    private val modalSelection = mutableStateOf(0)
    private var settingsAdjustHeldDir = 0

    // Tab selection inside the Root state. Tabs are config groups —
    // PlayingNow holds the existing pause-menu options (Resume / Save
    // State / Swap Disc / Reset / Close / etc), Performance and
    // Renderer host the speedhack + GS toggles backed by ConfigStore.
    private enum class Tab(val label: String) {
        PlayingNow("Play"),
        Performance("Perf"),
        Renderer("Render"),
        Fixes("Fixes"),
        Audio("Audio"),
        Patches("Patches"),
        Network("Network"),
        Overlay("Overlay"),
        Pad("Pad"),
        Skins("Skins"),
        Hotkeys("Hotkeys"),
        Recompiler("JIT"),
        App("Language"),
        Info("Info"),
    }
    private val currentTab = mutableStateOf(Tab.PlayingNow)

    // Live Settings state shared across the Performance + Renderer tabs.
    // Hydrated from ConfigStore on every overlay open so we pick up any
    // out-of-band edits (e.g. from a future global Settings screen).
    private val settingsState = mutableStateOf(Settings())
    private val previewGame = mutableStateOf<GameInfo?>(null)

    /** The library game whose settings were opened via long-press before
     *  launch (null once a game is actually running). Lets the Patches tab
     *  browse the patch database for a game you haven't booted yet. */
    val patchPreviewGame: GameInfo? get() = previewGame.value

    // Settings scope picked by the overlay header switch. Defaults to
    // [Game] when a game is loaded on open; falls back to [Global] when
    // no game serial is available. The settings tabs read this to decide
    // which tier to persist a change to (see ConfigStore.save). Held on
    // the overlay singleton so it survives tab switches but resets on
    // each open (the default-from-serial pass runs in open()).
    val settingsScope = mutableStateOf(SettingsScope.Global)

    // Serial of the currently-loaded game (null when on BIOS / disc swap
    // limbo). Resolved on overlay open via the cached Main.currentGame
    // first, falling back to NativeApp.getPauseGameSerial — same chain
    // GameInfoHeader uses. The scope toggle's "Game" option is gated on
    // this being non-null.
    val currentSerial = mutableStateOf<String?>(null)

    // Polled from NativeApp.isHardcoreMode while the overlay is open. Drives
    // the Save/Load state row dimming and the AchievementsPanel button
    // colour. Updates on every overlay open and every AchievementsPanel
    // poll (which already runs every 4s) — see Render() below.
    val hardcoreOn = mutableStateOf(false)

    // Controller scroll signal for the signed-in achievement list (a lazy list,
    // so it's scrolled rather than item-nav'd). Each ±1 = one step up/down; the
    // AchievementsPanel observes the delta and scrolls its LazyColumn.
    val achievementsScroll = mutableStateOf(0)
    // True when the signed-in achievement list is scrolled to the very top.
    // The Softcore/Hardcore toggle sits above the list, so it can only be
    // focused (by pressing Up) once the list is already at the top — otherwise
    // Up just scrolls the list up. AchievementsPanel keeps this in sync.
    val achievementsAtTop = mutableStateOf(true)

    // Locally tracked frame-limit toggle. 0 = Nominal (60fps cap),
    // 3 = Unlimited. Matches LimiterModeType / SpeedhackButton wiring.
    //
    // Default ON at app start (60fps cap). The state is held by this
    // singleton object so within an app session toggling once persists
    // across game launches — Main.start() re-applies it to native after
    // Settings.applyTo() runs on each VM init. Process restart resets to
    // the default (ON).
    val frameLimitOn = mutableStateOf(true)

    /** Renderer pill mode. Cycle: Auto → Hardware → Software → Auto.
     *
     *  - Auto = the default. Honours the wizard's backend choice
     *    (Main.renderer = "opengl" / "vulkan") and uses its HW path, but
     *    does NOT pin anything — emucore is free to swap to SW for
     *    things like SoftwareRendererFMVHack during FMVs, and the pill
     *    sync stays out of the way so the label keeps reading "Auto".
     *  - Hardware = pin HW on the picked backend.
     *  - Software = pin SW (display still on the picked backend's device).
     *
     *  We do NOT call NativeApp.renderAuto() here because it writes
     *  GSRendererType::Auto which then asks GSUtil::GetPreferredRenderer
     *  to pick a backend — on Android that resolves to OpenGL regardless
     *  of the wizard pick, silently rebuilding the device and (for Vulkan
     *  users) breaking the user's chosen backend. Auto's HW path is the
     *  same JNI as Hardware; the difference is purely label + sync. */
    enum class RendererMode { Auto, Hardware, Software }

    // Mirrored from native via NativeApp.isHardwareRenderer() on open +
    // on the achievements panel's 4s poll so an emucore-driven swap
    // (e.g. SoftwareRendererFMVHack flipping to SW during an FMV) doesn't
    // desync the Hardware / Software label. Sync is suppressed while in
    // Auto so the label stays "Auto" — the user picked "let it manage".
    val rendererMode = mutableStateOf(RendererMode.Auto)

    // OSD master toggle. Default ON because native-lib's initialize() turns
    // every OsdShow* bit on at first init; we only need to mirror the state
    // here so the pill label is right and re-toggling flips them all back.
    // Singleton state means the in-session preference persists across game
    // launches, matching the frame-limit pill pattern.
    private val osdShown = mutableStateOf(true)

    // Live RetroAchievements rich-presence string. Written by
    // AchievementsPanel's 4s poll (it already polls the achievements JSON
    // on the same cadence; one more JNI call is free). Read by
    // GameInfoHeader so the in-game pause panel reflects the current RP
    // line right under the game serial / star rating row.
    val richPresence = mutableStateOf("")

    // Tracks whether THIS overlay is the one that paused the VM. If the
    // user already had the VM paused (e.g. via toolbar) before opening
    // the overlay, closing the overlay shouldn't auto-resume.
    private var pausedByOverlay = false

    /** Build version string read at runtime from BuildVersion::GitRev
     *  via NativeApp.getBuildVersion(). Format
     *  "GitTagHi.GitTagMid.GitTagLo.ARMSX2Build-SNAPSHOT". Lazy so the
     *  JNI call defers to first use; cached for subsequent reads.
     *  Mirrored by SetupImpl so the wizard's title row stays in sync. */
    private val versionString: String by lazy {
        runCatching { NativeApp.getBuildVersion() }
            .getOrNull()
            ?.takeIf { it.isNotEmpty() }
            ?.let { "v$it" }
            ?: ""
    }

    /**
     * Single entry point for the Performance / Renderer / Recompiler tabs
     * to persist a settings change. Routes through ConfigStore.save which
     * picks the global or per-game tier based on the overlay's current
     * scope, then pushes the merged effective settings to native.
     *
     * The Settings object held in [settingsState] is already the
     * effective (global ∘ overrides) view, so applying it directly to
     * native is correct regardless of where the persisted bytes landed.
     */
    fun saveSettings(updated: Settings) {
        val previous = settingsState.value
        settingsState.value = updated
        ConfigStore.save(settingsScope.value, currentSerial.value, updated)
        if (Main.eState.value == EmuState.STOPPED) {
            updated.applyTo()
        } else {
            applySafeLiveDelta(previous, updated)
        }
        // Per-game scope: also regenerate the sparse, portable game-settings INI
        // (upstream FullscreenUI style) for the running game. The live apply
        // already happened above; this only refreshes the on-disk overrides so
        // they're visible/portable and load as the game layer on next boot.
        if (settingsScope.value == SettingsScope.Game && currentSerial.value != null &&
            Main.eState.value != EmuState.STOPPED) {
            updated.writeGameSettingsIni(ConfigStore.loadGlobal())
        }
        frameLimitOn.value = updated.frameLimitEnable
        osdShown.value = anyOsdElementEnabled(updated)
    }

    /** Reset the currently-shown settings tier to its baseline. Global scope →
     *  restore Settings() defaults. Game scope → drop this game's per-game
     *  overrides so it inherits global again (and, if a VM is running, clear the
     *  game's portable INI via an empty diff). Applies live like any edit. */
    fun resetCurrentScope() {
        // Pad feel/stick-modes, system hotkeys, and UI-size live OUTSIDE the Settings
        // object (Main.prefs / UiScale state) and are GLOBAL-ONLY — there is no per-game
        // tier for them. Reset them on EITHER scope's reset: otherwise a deadzone / stick
        // mode / hotkey / UI-size change made on those tabs can't be undone from the
        // per-game "Reset this game to global" (the user is looking right at the reset
        // button on that tab). The reset is confirm-tap-gated, so this is deliberate.
        com.armsx2.input.ControllerMappings.resetTunables()
        com.armsx2.input.ControllerMappings.clearAllHotkeys()
        UiScale.resetToDefaults()

        val serial = currentSerial.value
        if (settingsScope.value == SettingsScope.Game && serial != null) {
            ConfigStore.clearOverrides(serial)
            val resolved = ConfigStore.loadGlobal()
            val previous = settingsState.value
            settingsState.value = resolved
            if (Main.eState.value == EmuState.STOPPED) {
                resolved.applyTo()
            } else {
                applySafeLiveDelta(previous, resolved)
                // Empty diff → gameIniCommitWrite deletes the running game's INI.
                resolved.writeGameSettingsIni(ConfigStore.loadGlobal())
            }
            frameLimitOn.value = resolved.frameLimitEnable
            osdShown.value = anyOsdElementEnabled(resolved)
        } else {
            saveSettings(Settings())
        }
    }

    /** Tabs that have settings to reset via the per-tab Reset button. PlayingNow has
     *  only session actions; Skins/Info are managed/read-only. */
    private fun currentTabHasReset(): Boolean = when (currentTab.value) {
        Tab.PlayingNow, Tab.Skins, Tab.Info -> false
        else -> true
    }

    /** Reset ONLY the current tab's settings, leaving the other tabs untouched. The
     *  common ask: "let me reset the Pad tab without wiping all my game settings."
     *  Settings-backed tabs reset just their own fields — to Settings() defaults in
     *  Global scope, or to the resolved-global value in Game scope (so the game
     *  re-inherits global for those fields only). Pad / Hotkeys / Overlay also reset
     *  their GLOBAL-ONLY external state (stick feel + rumble, hotkeys, UI scale), which
     *  has no per-game tier — same as resetCurrentScope handles them. Applies live like
     *  any edit. NOTE: keep each tab's field list in sync with its *Tab.kt composable. */
    fun resetCurrentTab() {
        val base = if (settingsScope.value == SettingsScope.Game && currentSerial.value != null)
            ConfigStore.loadGlobal() else Settings()
        val cur = settingsState.value
        val updated: Settings? = when (currentTab.value) {
            Tab.Performance -> cur.copy(
                eeClampMode = base.eeClampMode, eeCycleRate = base.eeCycleRate,
                eeCycleSkip = base.eeCycleSkip, eeFpuRoundMode = base.eeFpuRoundMode,
                enableFastBoot = base.enableFastBoot, enableGameFixes = base.enableGameFixes,
                fastCDVD = base.fastCDVD, fpsLimit = base.fpsLimit, frameSkip = base.frameSkip,
                framerateNtsc = base.framerateNtsc, frameratePal = base.frameratePal,
                intcStat = base.intcStat, mtvu = base.mtvu,
                nominalSpeedPercent = base.nominalSpeedPercent,
                skipDuplicateFrames = base.skipDuplicateFrames, vu0RoundMode = base.vu0RoundMode,
                vu1Instant = base.vu1Instant, vu1RoundMode = base.vu1RoundMode,
                vuClampMode = base.vuClampMode, vuDeferredWrites = base.vuDeferredWrites,
                vuFlagHack = base.vuFlagHack, vuNeonFusions = base.vuNeonFusions,
                vuSkipStallSim = base.vuSkipStallSim, waitLoop = base.waitLoop,
            )
            Tab.Renderer -> cur.copy(
                renderer = base.renderer,
                accurateBlendingUnit = base.accurateBlendingUnit, adrenoFbFetch = base.adrenoFbFetch,
                aspectRatio = base.aspectRatio, fmvAspectRatio = base.fmvAspectRatio,
                deinterlaceMode = base.deinterlaceMode,
                dumpReplaceableTextures = base.dumpReplaceableTextures, gpuProfile = base.gpuProfile,
                hardwareDownloadMode = base.hardwareDownloadMode, hwAa1 = base.hwAa1,
                hwAat = base.hwAat, hwMipmap = base.hwMipmap, hwRov = base.hwRov,
                loadTextureReplacements = base.loadTextureReplacements,
                loadTextureReplacementsAsync = base.loadTextureReplacementsAsync,
                maxAnisotropy = base.maxAnisotropy,
                osdShowTextureReplacements = base.osdShowTextureReplacements,
                precacheTextureReplacements = base.precacheTextureReplacements,
                shadeBoost = base.shadeBoost, shadeBoostBrightness = base.shadeBoostBrightness,
                shadeBoostContrast = base.shadeBoostContrast, shadeBoostGamma = base.shadeBoostGamma,
                shadeBoostSaturation = base.shadeBoostSaturation, textureFiltering = base.textureFiltering,
                texturePreloading = base.texturePreloading, triFilter = base.triFilter,
                tvShader = base.tvShader, upscaleFloat = base.upscaleFloat, vsyncEnable = base.vsyncEnable,
            )
            Tab.Fixes -> cur.copy(
                alignSprite = base.alignSprite, antiBlur = base.antiBlur, autoFlush = base.autoFlush,
                autoFlushSw = base.autoFlushSw, bilinearUpscale = base.bilinearUpscale,
                cpuClutRender = base.cpuClutRender, cpuFramebufferConversion = base.cpuFramebufferConversion,
                cpuSpriteRenderBw = base.cpuSpriteRenderBw, cpuSpriteRenderLevel = base.cpuSpriteRenderLevel,
                disableDepthEmulation = base.disableDepthEmulation,
                disableFramebufferFetch = base.disableFramebufferFetch,
                disableInterlaceOffset = base.disableInterlaceOffset,
                disablePartialInvalidation = base.disablePartialInvalidation,
                disableRenderFixes = base.disableRenderFixes, disableSafeFeatures = base.disableSafeFeatures,
                disableShaderCache = base.disableShaderCache,
                disableVertexShaderExpand = base.disableVertexShaderExpand, dithering = base.dithering,
                drawBuffering = base.drawBuffering, estimateTextureRegion = base.estimateTextureRegion,
                forceEvenSpritePosition = base.forceEvenSpritePosition,
                gpuPaletteConversion = base.gpuPaletteConversion, gpuTargetClut = base.gpuTargetClut,
                halfPixelOffset = base.halfPixelOffset, hwAccurateAlphaTest = base.hwAccurateAlphaTest,
                integerScaling = base.integerScaling, limit24BitDepth = base.limit24BitDepth,
                manualUserHacks = base.manualUserHacks, mergeSprite = base.mergeSprite,
                mipmapSw = base.mipmapSw, nativeScaling = base.nativeScaling,
                overrideTextureBarriers = base.overrideTextureBarriers, preloadFrameData = base.preloadFrameData,
                readTargetsWhenClosing = base.readTargetsWhenClosing, roundSprite = base.roundSprite,
                screenOffsets = base.screenOffsets, showOverscan = base.showOverscan,
                skipDrawEnd = base.skipDrawEnd, skipDrawStart = base.skipDrawStart,
                spinCpuReadbacks = base.spinCpuReadbacks, spinGpuReadbacks = base.spinGpuReadbacks,
                swThreads = base.swThreads, swThreadsHeight = base.swThreadsHeight,
                syncToHostRefresh = base.syncToHostRefresh, textureInsideRt = base.textureInsideRt,
                textureOffsetX = base.textureOffsetX, textureOffsetY = base.textureOffsetY,
                unscaledPaletteDraw = base.unscaledPaletteDraw, useBlitSwapChain = base.useBlitSwapChain,
                vsyncQueueSize = base.vsyncQueueSize,
            )
            Tab.Audio -> cur.copy(
                audioBufferMs = base.audioBufferMs, audioFastForwardVolume = base.audioFastForwardVolume,
                audioMuted = base.audioMuted, audioOutputLatencyMs = base.audioOutputLatencyMs,
                audioTimeStretch = base.audioTimeStretch, audioVolume = base.audioVolume,
                audioSwapChannels = base.audioSwapChannels, spu2NeonReverb = base.spu2NeonReverb,
            )
            Tab.Patches -> cur.copy(
                enableCheats = base.enableCheats,
                enableNoInterlacingPatches = base.enableNoInterlacingPatches,
                enablePatches = base.enablePatches, enableWideScreenPatches = base.enableWideScreenPatches,
                hostFs = base.hostFs,
            )
            Tab.Network -> cur.copy(
                dev9AutoGateway = base.dev9AutoGateway, dev9AutoMask = base.dev9AutoMask,
                dev9Dns1 = base.dev9Dns1, dev9Dns2 = base.dev9Dns2, dev9EthApi = base.dev9EthApi,
                dev9EthDevice = base.dev9EthDevice, dev9EthEnable = base.dev9EthEnable,
                dev9EthLogDhcp = base.dev9EthLogDhcp, dev9EthLogDns = base.dev9EthLogDns,
                dev9Gateway = base.dev9Gateway, dev9HddEnable = base.dev9HddEnable,
                dev9HddFile = base.dev9HddFile, dev9InterceptDhcp = base.dev9InterceptDhcp,
                dev9Mask = base.dev9Mask, dev9ModeDns1 = base.dev9ModeDns1,
                dev9ModeDns2 = base.dev9ModeDns2, dev9Ps2Ip = base.dev9Ps2Ip,
                dev9EthHosts = base.dev9EthHosts,
            )
            Tab.Recompiler -> cur.copy(
                enableFastmem = base.enableFastmem, recEE = base.recEE, recIOP = base.recIOP,
                recVU0 = base.recVU0, recVU1 = base.recVU1,
            )
            Tab.Overlay -> {
                UiScale.resetToDefaults()
                cur.copy(
                    osdShowCpu = base.osdShowCpu, osdShowFps = base.osdShowFps,
                    osdShowFrameTimes = base.osdShowFrameTimes, osdShowGpu = base.osdShowGpu,
                    osdShowGsStats = base.osdShowGsStats, osdShowHardwareInfo = base.osdShowHardwareInfo,
                    osdShowGpuStats = base.osdShowGpuStats,
                    osdShowResolution = base.osdShowResolution, osdShowSpeed = base.osdShowSpeed,
                    osdShowVersion = base.osdShowVersion, osdShowVps = base.osdShowVps,
                    osdShowSettings = base.osdShowSettings, osdShowInputs = base.osdShowInputs,
                )
            }
            Tab.Pad -> {
                // Stick feel/deadzone/modes + rumble live OUTSIDE Settings (Main.prefs);
                // mirror what resetCurrentScope does for the pad. Button binds + macros
                // keep their own per-row resets, so this won't nuke careful mappings.
                com.armsx2.input.ControllerMappings.resetTunables()
                com.armsx2.input.ControllerMappings.setRumbleEnabled(true)
                null
            }
            Tab.Hotkeys -> {
                com.armsx2.input.ControllerMappings.clearAllHotkeys()
                null
            }
            else -> null // PlayingNow / Skins / Info — nothing persisted to reset
        }
        if (updated != null) saveSettings(updated)
    }

    private fun anyOsdElementEnabled(settings: Settings): Boolean =
        settings.osdShowFps ||
            settings.osdShowVps ||
            settings.osdShowSpeed ||
            settings.osdShowCpu ||
            settings.osdShowGpu ||
            settings.osdShowResolution ||
            settings.osdShowGsStats ||
            settings.osdShowFrameTimes ||
            settings.osdShowHardwareInfo ||
            settings.osdShowVersion

    private fun withAllOsdElements(settings: Settings, enabled: Boolean): Settings =
        settings.copy(
            osdShowFps = enabled,
            osdShowVps = enabled,
            osdShowSpeed = enabled,
            osdShowCpu = enabled,
            osdShowGpu = enabled,
            osdShowResolution = enabled,
            osdShowGsStats = enabled,
            osdShowFrameTimes = enabled,
            osdShowHardwareInfo = enabled,
            osdShowVersion = enabled,
        )

    private fun syncQuickTogglesFromSettings(settings: Settings) {
        frameLimitOn.value = settings.frameLimitEnable
        osdShown.value = anyOsdElementEnabled(settings)
    }

    private fun applySafeLiveDelta(previous: Settings, updated: Settings) {
        // Keep this path light. Full Settings.applyTo() calls native
        // commitSettings(), which parks the VM and can rebuild GS state; doing
        // that from every in-game tap caused ANRs/crashes when users scrubbed
        // display/renderer options. Only apply tiny runtime-safe deltas here.
        // Everything else is persisted by ConfigStore.save() above and takes
        // effect on restart / next launch.
        if (previous.frameLimitEnable != updated.frameLimitEnable) {
            NativeApp.setSetting("EmuCore/GS", "FrameLimitEnable", "bool", updated.frameLimitEnable.toString())
            NativeApp.speedhackLimitermode(if (updated.frameLimitEnable) 0 else 3)
            // This limiter write supersedes a latched fast-forward toggle; clear it so
            // the next FF-toggle press starts fresh instead of reading a stale "on".
            Main.fastForwardToggleActive = false
        }

        // Speed Limit % (emulation speed → NominalScalar) and Frame Rate Control (GS
        // present rate) are INDEPENDENT light re-applies, no VM park — safe on
        // this delta path. Persistence is handled by ConfigStore.
        if (previous.nominalSpeedPercent != updated.nominalSpeedPercent)
            NativeApp.setNominalSpeed(updated.nominalSpeedPercent.coerceIn(10, 1000))
        if (previous.fpsLimit != updated.fpsLimit)
            NativeApp.setFpsCap(updated.fpsLimit.coerceIn(0, 1000))

        // Per-region NTSC/PAL framerate — applies live (NetherSX2-style) via a
        // dedicated coalesced queue (it parks the VM to recompute the vsync pacer,
        // so it can't run inline here). Persists to base inside the queue too.
        if (previous.framerateNtsc != updated.framerateNtsc ||
            previous.frameratePal != updated.frameratePal) {
            LiveGsApplyQueue.applyFramerate(updated.framerateNtsc, updated.frameratePal)
        }

        // Audio — SPU2 setters apply live to the open stream, no VM park.
        if (previous.audioVolume != updated.audioVolume)
            NativeApp.setAudioVolume(updated.audioVolume.coerceIn(0, 200))
        if (previous.audioMuted != updated.audioMuted)
            NativeApp.setAudioMuted(updated.audioMuted)

        // SyncMode / buffer / latency / FF-volume reconfigure the SPU2 stream, so
        // they need the commit path (ApplySettings → spu2 ApplyConfig). Parks the
        // VM briefly; only fires when one of them actually changed.
        if (previous.audioTimeStretch != updated.audioTimeStretch ||
            previous.audioBufferMs != updated.audioBufferMs ||
            previous.audioOutputLatencyMs != updated.audioOutputLatencyMs ||
            previous.audioFastForwardVolume != updated.audioFastForwardVolume) {
            NativeApp.setSetting("SPU2/Output", "SyncMode", "string",
                if (updated.audioTimeStretch) "TimeStretch" else "Disabled")
            NativeApp.setSetting("SPU2/Output", "BufferMS", "int", updated.audioBufferMs.coerceIn(10, 200).toString())
            NativeApp.setSetting("SPU2/Output", "OutputLatencyMS", "int", updated.audioOutputLatencyMs.coerceIn(5, 200).toString())
            NativeApp.setSetting("SPU2/Output", "FastForwardVolume", "int", updated.audioFastForwardVolume.coerceIn(0, 200).toString())
            NativeApp.commitSettings()
        }

        if (previous.vu1Instant != updated.vu1Instant)
            NativeApp.setInstantVU1(updated.vu1Instant)

        // EE Cycle Rate / Skip are baked into compiled blocks (recScaleBlockCycles
        // runs at compile time), so a change only takes effect once the EE rec is
        // reset. That needs the full commit path: setSetting + commitSettings →
        // VMManager::ApplySettings → CheckForConfigChanges → ClearCPUExecutionCaches.
        // It parks the VM (heavier than the other live deltas), but it's the only
        // way these actually apply in-game — without it they silently do nothing.
        if (previous.eeCycleRate != updated.eeCycleRate || previous.eeCycleSkip != updated.eeCycleSkip) {
            NativeApp.setSetting("EmuCore/Speedhacks", "EECycleRate", "int", updated.eeCycleRate.toString())
            NativeApp.setSetting("EmuCore/Speedhacks", "EECycleSkip", "int", updated.eeCycleSkip.toString())
            NativeApp.commitSettings()
        }

        // Manual frameskip — GS-thread global, applies on the next VSync. No
        // VM park, so it's safe to push live.
        if (previous.frameSkip != updated.frameSkip)
            NativeApp.setFrameSkip(updated.frameSkip.coerceIn(0, 5))

        if (previous.aspectRatio != updated.aspectRatio) {
            val ratio = updated.aspectRatio.coerceIn(0, 4)
            val name = when (ratio) {
                0 -> "Stretch"
                2 -> "4:3"
                3 -> "16:9"
                4 -> "10:7"
                else -> "Auto 4:3/3:2"
            }
            NativeApp.setSetting("EmuCore/GS", "AspectRatio", "string", name)
            NativeApp.setAspectRatio(ratio)
        }
        if (previous.fmvAspectRatio != updated.fmvAspectRatio) {
            val fmvRatio = updated.fmvAspectRatio.coerceIn(0, 4)
            val fmvName = when (fmvRatio) {
                1 -> "Auto 4:3/3:2"
                2 -> "4:3"
                3 -> "16:9"
                4 -> "10:7"
                else -> "Off"
            }
            NativeApp.setSetting("EmuCore/GS", "FMVAspectRatioSwitch", "string", fmvName)
            NativeApp.setFmvAspectRatio(fmvRatio)
        }

        // USB keyboard (#254) — attach/detach the emulated HID keyboard live and
        // flip the input-routing flag so physical-keyboard keys start/stop being
        // forwarded immediately. Persist [USB1] Type too so it survives a restart
        // (ConfigStore.save already stored the field; this keeps the emucore base
        // layer in sync for the next boot's USBOptions::LoadSave).
        if (previous.usbKeyboard != updated.usbKeyboard) {
            NativeApp.setSetting("USB1", "Type", "string", if (updated.usbKeyboard) "hidkbd" else "None")
            NativeApp.usbSetKeyboardEnabled(0, updated.usbKeyboard)
            Main.usbKeyboardActive = updated.usbKeyboard
        }

        // Internal resolution (upscale) applies live to the GS via the queue — no
        // VM park. The renderer BACKEND (OpenGL/Vulkan/Software) is restart-only,
        // applied by Main.applyRendererPrefs on the next launch, so nothing live
        // to do for it here.
        if (previous.upscaleFloat != updated.upscaleFloat) {
            Main.upscale.value = updated.upscaleFloat
            com.armsx2.config.LiveGsApplyQueue.applyUpscale(updated.upscaleFloat)
        }

        if (previous.loadTextureReplacements != updated.loadTextureReplacements)
            NativeApp.setSetting("EmuCore/GS", "LoadTextureReplacements", "bool", updated.loadTextureReplacements.toString())
        if (previous.loadTextureReplacementsAsync != updated.loadTextureReplacementsAsync)
            NativeApp.setSetting("EmuCore/GS", "LoadTextureReplacementsAsync", "bool", updated.loadTextureReplacementsAsync.toString())
        if (previous.precacheTextureReplacements != updated.precacheTextureReplacements)
            NativeApp.setSetting("EmuCore/GS", "PrecacheTextureReplacements", "bool", updated.precacheTextureReplacements.toString())
        if (previous.dumpReplaceableTextures != updated.dumpReplaceableTextures)
            NativeApp.setSetting("EmuCore/GS", "DumpReplaceableTextures", "bool", updated.dumpReplaceableTextures.toString())
        if (previous.osdShowTextureReplacements != updated.osdShowTextureReplacements)
            NativeApp.setSetting("EmuCore/GS", "OsdShowTextureReplacements", "bool", updated.osdShowTextureReplacements.toString())

        // Performance Overlay element toggles. Persist to base (survives the
        // next ApplySettings reload) AND push live via the native setter,
        // which flips EmuConfig.GS + MTGS::ApplySettings. Disabling GPU also
        // stops the GPU timing queries (the perf win the tester asked for).
        if (previous.osdShowFps != updated.osdShowFps) {
            NativeApp.setSetting("EmuCore/GS", "OsdShowFPS", "bool", updated.osdShowFps.toString())
            NativeApp.osdShowFPS(updated.osdShowFps)
        }
        if (previous.osdScale != updated.osdScale) {
            NativeApp.setSetting("EmuCore/GS", "OsdScale", "int", updated.osdScale.coerceIn(25, 500).toString())
            NativeApp.osdSetScale(updated.osdScale.toFloat())
        }
        if (previous.osdShowVps != updated.osdShowVps) {
            NativeApp.setSetting("EmuCore/GS", "OsdShowVPS", "bool", updated.osdShowVps.toString())
            NativeApp.osdShowVPS(updated.osdShowVps)
        }
        if (previous.osdShowSpeed != updated.osdShowSpeed) {
            NativeApp.setSetting("EmuCore/GS", "OsdShowSpeed", "bool", updated.osdShowSpeed.toString())
            NativeApp.osdShowSpeed(updated.osdShowSpeed)
        }
        if (previous.osdShowCpu != updated.osdShowCpu) {
            NativeApp.setSetting("EmuCore/GS", "OsdShowCPU", "bool", updated.osdShowCpu.toString())
            NativeApp.osdShowCPU(updated.osdShowCpu)
        }
        if (previous.osdShowGpu != updated.osdShowGpu) {
            NativeApp.setSetting("EmuCore/GS", "OsdShowGPU", "bool", updated.osdShowGpu.toString())
            NativeApp.osdShowGPU(updated.osdShowGpu)
        }
        if (previous.osdShowResolution != updated.osdShowResolution) {
            NativeApp.setSetting("EmuCore/GS", "OsdShowResolution", "bool", updated.osdShowResolution.toString())
            NativeApp.osdShowResolution(updated.osdShowResolution)
        }
        if (previous.osdShowGsStats != updated.osdShowGsStats) {
            NativeApp.setSetting("EmuCore/GS", "OsdShowGSStats", "bool", updated.osdShowGsStats.toString())
            NativeApp.osdShowGSStats(updated.osdShowGsStats)
        }
        if (previous.osdShowFrameTimes != updated.osdShowFrameTimes) {
            NativeApp.setSetting("EmuCore/GS", "OsdShowFrameTimes", "bool", updated.osdShowFrameTimes.toString())
            NativeApp.osdShowFrameTimes(updated.osdShowFrameTimes)
        }
        if (previous.osdShowHardwareInfo != updated.osdShowHardwareInfo) {
            NativeApp.setSetting("EmuCore/GS", "OsdShowHardwareInfo", "bool", updated.osdShowHardwareInfo.toString())
            NativeApp.osdShowHardwareInfo(updated.osdShowHardwareInfo)
        }
        if (previous.osdShowGpuStats != updated.osdShowGpuStats) {
            NativeApp.setSetting("EmuCore/GS", "OsdShowGPUStats", "bool", updated.osdShowGpuStats.toString())
            NativeApp.osdShowGpuStats(updated.osdShowGpuStats)
        }
        if (previous.osdShowVersion != updated.osdShowVersion) {
            NativeApp.setSetting("EmuCore/GS", "OsdShowVersion", "bool", updated.osdShowVersion.toString())
            NativeApp.osdShowVersion(updated.osdShowVersion)
        }
        if (previous.osdShowSettings != updated.osdShowSettings) {
            NativeApp.setSetting("EmuCore/GS", "OsdShowSettings", "bool", updated.osdShowSettings.toString())
            NativeApp.osdShowSettings(updated.osdShowSettings)
        }
        if (previous.osdShowInputs != updated.osdShowInputs) {
            NativeApp.setSetting("EmuCore/GS", "OsdShowInputs", "bool", updated.osdShowInputs.toString())
            NativeApp.osdShowInputs(updated.osdShowInputs)
        }

        // Renderer / hardware-fix / upscaling-fix changes apply live via a GS-only
        // reconfigure (Settings.applyGsLive → native applyGSSettingsLive). It does
        // NOT rebuild the CPU/JIT and preserves the device-identity fields, so it
        // can't trigger a GS device recreate. Gated on an actual GS diff so non-GS
        // taps (audio, frame limit, …) don't reconfigure the GS thread.
        if (previous.gsDiffersFrom(updated))
            LiveGsApplyQueue.applySettings(updated)
    }

    /** Toggle the overlay open/closed — for a physical "menu" button binding.
     *  Closing resumes the VM (the normal close-and-resume path). */
    fun toggle() {
        if (WindowImpl.overlayVisible.value) closeAndResume()
        else open()
    }

    /** Open the dedicated achievements view. Bound to the "Open Achievements"
     *  hotkey and the header trophy button. */
    fun openAchievements() {
        if (!WindowImpl.overlayVisible.value) open()
        SettingsControllerNav.clearSelection()
        achievementsAtTop.value = true
        state.value = State.Achievements
    }

    fun handleControllerMove(dx: Int, dy: Int): Boolean {
        if (!WindowImpl.overlayVisible.value) return false
        when (state.value) {
            is State.Root -> {
                if (currentTab.value == Tab.PlayingNow && !settingsOnly.value) {
                    val sel = playSelection.value
                    if (sel == 12) {
                        // Memory Cards is the tall button on the RIGHT of the 3x4 grid.
                        // Left re-enters the grid at its right column (middle row).
                        if (dx < 0) playSelection.value = 7
                        return true
                    }
                    val row = sel / 4
                    val col = sel % 4
                    if (dx > 0 && col == 3) { playSelection.value = 12; return true } // right off grid → cards
                    val nextRow = (row + dy).coerceIn(0, 2)
                    val nextCol = (col + dx).coerceIn(0, 3)
                    playSelection.value = nextRow * 4 + nextCol
                    return true
                }
                if (settingsTabActive()) {
                    if (dy != 0) {
                        settingsAdjustHeldDir = 0
                        return SettingsControllerNav.move(dy)
                    }
                    if (dx != 0) {
                        // D-pad only (the stick no longer drives adjust). Each
                        // key press is a discrete move and the auto-repeat is
                        // ignored upstream, so adjust directly — no held-dir
                        // gate that could get stuck and block the next option.
                        return SettingsControllerNav.adjust(dx) || SettingsControllerNav.hasItems()
                    }
                    return SettingsControllerNav.hasItems()
                }
            }
            is State.ExitConfirm -> {
                if (dy != 0) modalSelection.value = (modalSelection.value + dy).coerceIn(0, 2)
                return true
            }
            is State.ResetConfirm, is State.HardcoreEnableConfirm, is State.HardcoreDisableConfirm -> {
                val delta = if (dy != 0) dy else dx
                if (delta != 0) modalSelection.value = (modalSelection.value + delta).coerceIn(0, 1)
                return true
            }
            is State.AchievementsLogin -> {
                // Manual model (touch mode blocks Compose focus). Any direction
                // steps the flat control list (login fields / cancel / sign in).
                val delta = if (dy != 0) dy else dx
                if (delta != 0) return SettingsControllerNav.move(delta)
                return SettingsControllerNav.hasItems()
            }
            is State.Achievements -> {
                val delta = if (dy != 0) dy else dx
                if (delta == 0) return true
                // A stack of focusable controls (account/logout, hardcore toggle,
                // and the RA option toggles) sits ABOVE the scrollable achievement
                // list. Nav model: while a control is focused, Up/Down step through
                // the stack; Down off the LAST (bottom-most, nearest the list)
                // control releases focus back to the list so it can scroll. With
                // nothing focused, Up at the top of the list re-enters the stack at
                // its bottom; Down always scrolls. This keeps the list scrollable
                // instead of the focus getting trapped on a single header control.
                if (SettingsControllerNav.hasItems()) {
                    val sel = SettingsControllerNav.selectedIndex.value
                    if (sel >= 0) {
                        if (delta > 0) {
                            // Down: next control, or release to the list past the last.
                            if (sel >= SettingsControllerNav.count() - 1)
                                SettingsControllerNav.clearSelection()
                            else
                                SettingsControllerNav.move(1)
                        } else {
                            // Up: previous control (stops at the top of the stack).
                            SettingsControllerNav.move(-1)
                        }
                        return true
                    }
                    // Nothing focused (scrolling the list). Up enters the stack at
                    // its bottom ONLY when the list is already at the top; otherwise
                    // Up scrolls the list up. Down always scrolls.
                    if (delta < 0 && achievementsAtTop.value) {
                        SettingsControllerNav.move(-1)
                        return true
                    }
                }
                achievementsScroll.value += delta
                return true
            }
            is State.SaveStateSlots, is State.LoadStateSlots -> {
                // Zone-aware nav: header switches / slot grid / back button.
                SaveStatePicker.move(dx, dy)
                return true
            }
            else -> return false
        }
        return false
    }

    fun handleControllerConfirm(): Boolean {
        if (!WindowImpl.overlayVisible.value) return false
        when (state.value) {
            is State.Root -> {
                if (currentTab.value == Tab.PlayingNow && !settingsOnly.value) {
                    activatePlaySelection(playSelection.value)
                    return true
                }
                if (settingsTabActive())
                    return SettingsControllerNav.confirm()
            }
            is State.ExitConfirm -> {
                when (modalSelection.value.coerceIn(0, 2)) {
                    0 -> exitSaveStateAndExit()
                    1 -> exitWithoutSaving()
                    else -> enterState(State.Root)
                }
                return true
            }
            is State.ResetConfirm -> {
                if (modalSelection.value.coerceIn(0, 1) == 0) resetSystem()
                else enterState(State.Root)
                return true
            }
            is State.HardcoreSaveStateBlocked -> {
                enterState(State.Root)
                return true
            }
            is State.HardcoreEnableConfirm -> {
                if (modalSelection.value.coerceIn(0, 1) == 0) {
                    enterState(State.Root)
                } else {
                    enableHardcoreMode()
                }
                return true
            }
            is State.HardcoreDisableConfirm -> {
                if (modalSelection.value.coerceIn(0, 1) == 0) {
                    enterState(State.Root)
                } else {
                    disableHardcoreMode()
                }
                return true
            }
            is State.Achievements, is State.AchievementsLogin ->
                return SettingsControllerNav.confirm()
            is State.SaveStateSlots, is State.LoadStateSlots -> {
                SaveStatePicker.confirm()
                return true
            }
            else -> return false
        }
        return false
    }

    fun handleControllerBack(): Boolean {
        if (!WindowImpl.overlayVisible.value) return false
        when (state.value) {
            is State.Root -> closeAndResume()
            else -> enterState(State.Root)
        }
        return true
    }

    private fun settingsTabActive(): Boolean =
        state.value is State.Root &&
            (settingsOnly.value || currentTab.value != Tab.PlayingNow)

    fun handleControllerHorizontalRelease() {
        settingsAdjustHeldDir = 0
        SettingsControllerNav.resetAdjustmentGate()
    }

    private fun resetSettingsAdjustGate() {
        settingsAdjustHeldDir = 0
        SettingsControllerNav.resetAdjustmentGate()
    }

    fun handleControllerTab(delta: Int): Boolean {
        if (!WindowImpl.overlayVisible.value || state.value !is State.Root || delta == 0) return false
        cycleTab(delta)
        return true
    }

    fun handleControllerScroll(velocity: Float): Boolean {
        if (!WindowImpl.overlayVisible.value || !settingsTabActive()) {
            SettingsControllerNav.setScrollVelocity(0f)
            return false
        }
        return SettingsControllerNav.setScrollVelocity(velocity)
    }

    private fun enterState(next: State) {
        state.value = next
        modalSelection.value = 0
    }

    private fun cycleTab(delta: Int) {
        val tabs = if (settingsOnly.value) {
            listOf(Tab.Performance, Tab.Renderer, Tab.Fixes, Tab.Audio, Tab.Patches, Tab.Network, Tab.Overlay, Tab.Pad, Tab.Skins, Tab.Hotkeys, Tab.Recompiler, Tab.App, Tab.Info)
        } else {
            Tab.values().toList()
        }
        val index = tabs.indexOf(currentTab.value).takeIf { it >= 0 } ?: 0
        currentTab.value = tabs[(index + delta).floorMod(tabs.size)]
        SettingsControllerNav.clearSelection()
        resetSettingsAdjustGate()
    }

    private fun Int.floorMod(modulus: Int): Int =
        ((this % modulus) + modulus) % modulus

    private fun activatePlaySelection(index: Int) {
        when (index.coerceIn(0, 12)) {
            0 -> closeAndResume()
            1 -> openSaveStates()
            2 -> openLoadStates()
            3 -> swapDisc()
            4 -> bootDisc()
            5 -> openLibrary()
            6 -> cycleRendererMode()
            7 -> toggleFrameLimit()
            8 -> editTouchLayout()
            9 -> toggleOsd()
            10 -> enterState(State.ResetConfirm)
            11 -> closeGame()
            12 -> openMemcards()
        }
    }

    private fun openSaveStates() {
        // Saving IS allowed in RetroAchievements hardcore (matches desktop PCSX2);
        // only loading is blocked — see openLoadStates + native LoadStateFromSlot.
        SaveStatePicker.resetControllerSel()
        enterState(State.SaveStateSlots)
    }

    private fun openLoadStates() {
        SaveStatePicker.resetControllerSel()
        enterState(if (hardcoreOn.value) State.HardcoreSaveStateBlocked else State.LoadStateSlots)
    }

    /** Open the pause overlay straight to the Save-State slot picker — used by the
     *  on-screen SAVE button so the user picks which slot to save to (open() resets
     *  to Root, so we enter the picker state right after). */
    fun openSaveStatePicker() {
        open()
        openSaveStates()
    }

    /** Open the pause overlay straight to the Load-State slot picker — used by the
     *  on-screen LOAD button so the user picks which slot to load from. */
    fun openLoadStatePicker() {
        open()
        openLoadStates()
    }

    private fun swapDisc() {
        val intent = Intent(Intent.ACTION_GET_CONTENT)
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
        intent.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, false)
        intent.setType("*/*")
        Main.instance?.swapDiscAction?.launch(intent)
        closeKeepingState()
    }

    private fun bootDisc() {
        val intent = Intent(Intent.ACTION_GET_CONTENT)
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
        intent.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, false)
        intent.setType("*/*")
        Main.instance?.bootDiscAction?.launch(intent)
        closeKeepingState()
    }

    private fun openLibrary() {
        if (Main.eState.value == EmuState.PAUSED) Main.resume()
        WindowImpl.showLibrary.value = true
        closeKeepingState()
    }

    /** Open the Memory Card manager over the running game. Because a game is loaded,
     *  its per-game Slot 1 picker (#137) is available here. Closing the manager
     *  returns to the (still-paused) game. */
    private fun openMemcards() {
        MemoryCardManager.visible.value = true
        closeKeepingState()
    }

    private fun cycleRendererMode() {
        val backendHw: () -> Unit = {
            if (Main.renderer.value == "vulkan")
                Main.renderVulkan() else Main.renderOpenGL()
        }
        rendererMode.value = when (rendererMode.value) {
            RendererMode.Auto -> {
                backendHw()
                RendererMode.Hardware
            }
            RendererMode.Hardware -> {
                Main.renderSoftware()
                RendererMode.Software
            }
            RendererMode.Software -> {
                backendHw()
                RendererMode.Auto
            }
        }
    }

    private fun toggleFrameLimit() {
        saveSettings(settingsState.value.copy(frameLimitEnable = !frameLimitOn.value))
    }

    fun editTouchLayout() {
        com.armsx2.ui.touch.TouchControls.ensureLoaded()
        com.armsx2.ui.touch.TouchControls.editMode.value = true
        closeKeepingState()
    }

    /** Public so the TOGGLE_OSD hotkey (Main.dispatchKeyEvent) can flip the
     *  performance overlay through the exact same path as the on-screen button. */
    fun toggleOsd() {
        val enabled = !osdShown.value
        saveSettings(withAllOsdElements(settingsState.value, enabled))
        NativeApp.osdShowAll(enabled)
    }

    private fun closeGame() {
        if (Main.prefs.getBoolean("autoSaveOnExit", false)) {
            Main.stop(saveAutosave = true)
            closeKeepingState()
        } else {
            enterState(State.ExitConfirm)
        }
    }

    private fun exitSaveStateAndExit() {
        Main.stop(saveAutosave = true)
        closeKeepingState()
    }

    private fun exitWithoutSaving() {
        Main.stop()
        closeKeepingState()
    }

    private fun resetSystem() {
        Main.restart()
        closeKeepingState()
    }

    private fun enableHardcoreMode() {
        // Persist ChallengeMode=true, then RESET the VM. Upstream defers a
        // hardcore-enable on a running game until a clean boot
        // (Achievements::UpdateSettings shows a "will enable on reset" message
        // and leaves s_hardcore_mode false); the reset path re-runs
        // ResetHardcoreMode() which actually flips it on. Without the reset the
        // flag was set but never engaged, and the poll flipped the button back
        // to SOFTCORE. Main.restart() reboots the game (same as the Reset menu
        // item) so hardcore comes up clean.
        NativeApp.setHardcoreMode(true)
        hardcoreOn.value = true
        if (Main.eState.value == EmuState.STOPPED) {
            // No game running (global / home-screen toggle): setHardcoreMode already
            // persisted Achievements/ChallengeMode, which engages on the next boot.
            // Resetting here would reboot into the BIOS — there's no game to reset — which
            // is exactly the "it just boots the BIOS" report. Just close back to root.
            enterState(State.Root)
        } else {
            resetSystem()
        }
    }

    private fun disableHardcoreMode() {
        // Drop to softcore. Unlike enable, this does NOT reset the game — it just
        // flips the flag off for the rest of the session (matching the old
        // immediate-toggle behaviour, now gated behind a confirm).
        NativeApp.setHardcoreMode(false)
        hardcoreOn.value = false
        enterState(State.Root)
    }

    /** Open the overlay. Pauses the VM. Safe to call when already open. */
    fun open() {
        if (WindowImpl.overlayVisible.value) return
        settingsOnly.value = false
        previewGame.value = null
        pausedByOverlay = (Main.eState.value == EmuState.RUNNING)
        // ALWAYS pause while the overlay is up — even if eState already
        // says PAUSED. The Kotlin flag can run ahead of the actual VM,
        // and a stale PAUSED left the VM running underneath while settings
        // changes (upscale → live GS reconfig) applied mid-frame.
        // Main.pauseForOverlay() directly signals the nonblocking native pause
        // path so the EE breaks out of Execute() as soon as the overlay opens.
        // Mid-frame-settings safety doesn't depend on this call having
        // completed: commitSettings and the savestate/GS-reconfig JNI entry
        // points enforce VM quiescence themselves (ScopedVMPause).
        if (Main.eState.value != EmuState.STOPPED) Main.pauseForOverlay()
        state.value = State.Root
        playSelection.value = 0
        modalSelection.value = 0
        // Reopen on the tab you were last on instead of snapping back to Play —
        // currentTab persists on the overlay singleton, so just leave it. Less tedious
        // when tuning a game and stepping in/out of the menu. First open = Play (the
        // field's default). Quick-resume users can still tap Play / press B.
        SettingsControllerNav.clearSelection()
        resetSettingsAdjustGate()
        // Resolve the current game's serial first; scope and settings
        // hydration both depend on it. Mirrors GameInfoHeader's chain —
        // cached GameInfo (carries through file picker paths that lack a
        // native serial), then NativeApp.getPauseGameSerial. Empty
        // string from native means "no disc loaded" → fall back to
        // global scope/settings.
        // Use settingsKey (serial for discs, filename stem for serial-less
        // ELF/homebrew) so the key the overlay SAVES to matches the key the boot
        // path READS — otherwise ELF per-game edits saved here were invisible at
        // the next boot (issue #253).
        val serial = Main.currentGame.value?.settingsKey
            ?: runCatching { NativeApp.getPauseGameSerial() }.getOrNull()
                ?.takeIf { it.isNotEmpty() }
        currentSerial.value = serial
        settingsScope.value =
            if (serial != null) SettingsScope.Game else SettingsScope.Global
        // Re-hydrate the live Settings state from disk. When a game is
        // loaded we want to see the EFFECTIVE settings (global ∘ overrides)
        // so the user edits the merged value; otherwise just the global.
        settingsState.value = ConfigStore.resolveForGame(serial)
        syncQuickTogglesFromSettings(settingsState.value)
        // Sync pill state from native — covers emucore-driven swaps that
        // happened while the overlay was closed. Auto is sticky against
        // the sync (the user picked "let it decide", so we keep showing
        // Auto even though GS resolved it to HW underneath).
        if (rendererMode.value != RendererMode.Auto) {
            runCatching {
                rendererMode.value = if (NativeApp.isHardwareRenderer())
                    RendererMode.Hardware else RendererMode.Software
            }
        }
        WindowImpl.overlayVisible.value = true
    }

    /** Open the same settings tabs from the stopped/library UI. */
    fun openGlobalSettings() {
        if (WindowImpl.overlayVisible.value) return
        settingsOnly.value = true
        previewGame.value = null
        pausedByOverlay = false
        state.value = State.Root
        playSelection.value = 0
        modalSelection.value = 0
        // Remember the last settings tab across opens instead of snapping back to
        // the first one. PlayingNow isn't shown in settings-only mode, so only fall
        // back to the first real tab when we're sitting on it (e.g. first open).
        if (currentTab.value == Tab.PlayingNow) currentTab.value = Tab.Performance
        SettingsControllerNav.clearSelection()
        resetSettingsAdjustGate()
        currentSerial.value = null
        settingsScope.value = SettingsScope.Global
        settingsState.value = ConfigStore.loadGlobal()
        syncQuickTogglesFromSettings(settingsState.value)
        WindowImpl.overlayVisible.value = true
    }

    /** Open per-game settings from a long-pressed library card before launch. */
    fun openGameSettings(game: GameInfo) {
        if (WindowImpl.overlayVisible.value) return
        settingsOnly.value = true
        previewGame.value = game
        pausedByOverlay = false
        state.value = State.Root
        playSelection.value = 0
        modalSelection.value = 0
        // Remember the last settings tab across opens (see openGlobalSettings).
        if (currentTab.value == Tab.PlayingNow) currentTab.value = Tab.Performance
        SettingsControllerNav.clearSelection()
        resetSettingsAdjustGate()
        currentSerial.value = game.settingsKey?.takeIf { it.isNotEmpty() }
        settingsScope.value =
            if (currentSerial.value != null) SettingsScope.Game else SettingsScope.Global
        settingsState.value = ConfigStore.resolveForGame(currentSerial.value)
        syncQuickTogglesFromSettings(settingsState.value)
        WindowImpl.overlayVisible.value = true
    }

    private fun closeAndResume() {
        WindowImpl.overlayVisible.value = false
        state.value = State.Root
        playSelection.value = 0
        modalSelection.value = 0
        settingsOnly.value = false
        previewGame.value = null
        SettingsControllerNav.clearSelection()
        resetSettingsAdjustGate()
        // Always resume if the VM is paused — close-paths that should
        // preserve a paused VM (Swap Disc picker, library, edit mode)
        // go through closeKeepingState instead. The earlier
        // pausedByOverlay gate stale-locked the VM after the user
        // bounced through closeKeepingState (e.g. entered edit mode then
        // re-opened pause): pausedByOverlay was cleared, so the next
        // Resume tap did nothing.
        if (pausedByOverlay || Main.eState.value == EmuState.PAUSED) Main.resume()
        pausedByOverlay = false
    }

    private fun closeKeepingState() {
        WindowImpl.overlayVisible.value = false
        state.value = State.Root
        playSelection.value = 0
        modalSelection.value = 0
        settingsOnly.value = false
        previewGame.value = null
        SettingsControllerNav.clearSelection()
        resetSettingsAdjustGate()
        pausedByOverlay = false
    }

    @Composable
    fun Render() {
        // Hardware back: descends submenu first, then dismisses (resumes).
        BackHandler(enabled = WindowImpl.overlayVisible.value) {
            when (state.value) {
                is State.Root -> closeAndResume()
                else -> state.value = State.Root
            }
        }

        // Backdrop swallows taps so the running surface beneath doesn't
        // pick up the press as a button event. Tap-outside on Root acts
        // as Resume Game. The dim layer fills the whole screen (cutout
        // included) so the partial-alpha aesthetic is uniform; only the
        // INNER content box gets displayCutoutPadding so headers / menu
        // rows aren't obscured by punch-hole or notch hardware.
        val backdropInteraction = remember { MutableInteractionSource() }
        // Controller navigation: pull Compose focus into the overlay when it
        // opens so the D-pad can traverse its buttons (focusGroup makes the
        // first focusable child take focus). Gamepad A is translated to
        // DPAD_CENTER in Main.dispatchKeyEvent so clickable items activate;
        // B / Back drills up via the BackHandler above. Main re-grabs the game
        // surface's focus when the overlay closes so controller input returns
        // to the game.
        val navFocus = remember { FocusRequester() }
        val navFocusManager = androidx.compose.ui.platform.LocalFocusManager.current
        LaunchedEffect(Unit) {
            // Let the game SurfaceView relinquish focus first (it's gated
            // non-focusable while the overlay is up), then forcibly pull focus
            // off it and into the overlay's focus group so the D-pad can
            // traverse the buttons and A (-> DPAD_CENTER) can activate them.
            runCatching { navFocusManager.clearFocus(force = true) }
            // Retry until the focus group is placed and accepts focus — the game
            // surface / library can briefly hold focus as the overlay opens, so a
            // single request may land before the container is ready.
            repeat(15) {
                if (runCatching { navFocus.requestFocus() }.isSuccess) return@LaunchedEffect
                kotlinx.coroutines.delay(20)
            }
        }
        Box(
            Modifier
                .fillMaxSize()
                .background(Color.Black.copy(alpha = 0.68f))
                // Backdrop taps are swallowed (no close) so an accidental tap on
                // empty space can't drop you out of the overlay. Exit is the ✕
                // button (touch) or B (controller).
                .clickable(
                    indication = null,
                    interactionSource = backdropInteraction,
                    onClick = { }
                ),
        ) {
            // Inner safe-area container — content laid out against this
            // box's edges automatically gets cutout-aware insets. Tap on
            // the dim band outside still falls through to the backdrop's
            // close-on-tap handler because this inner Box is non-clickable.
            Box(Modifier.fillMaxSize().displayCutoutPadding().focusRequester(navFocus).focusable()) {
            // Settings tabs use (nearly) the full width so long rows / the tab
            // strip aren't cut off. The Play tab stays compact — its 4-column
            // action grid is laid out for the narrow column (full width spread it
            // out and broke the layout). On screens narrower than that column it
            // must shrink to fit instead of overflowing, so the compact width is
            // a cap (widthIn) over fillMaxWidth, not a hard width — wide screens
            // (RP6) still get the exact same 520/560dp, small screens scale down.
            val wideContent = state.value is State.Root &&
                (settingsOnly.value || currentTab.value != Tab.PlayingNow)
            // Portrait (the new Emulation Screen Orientation setting): this header is
            // a landscape-first absolute layout — the top-left title column and the
            // top-right brand/close cluster overlap on a narrow screen. In portrait
            // we drop the decorative brand wordmark and tighten the content width so
            // the title / RetroAchievements / ✕ stop colliding. Landscape unchanged.
            val isPortrait = LocalConfiguration.current.orientation ==
                android.content.res.Configuration.ORIENTATION_PORTRAIT
            // Headless poll keeps hardcore / renderer / rich-presence state in
            // sync even though the inline achievements panel is gone.
            AchievementsSync()
            Box(
                Modifier
                    .align(Alignment.TopStart)
                    .fillMaxHeight()
                    .then(if (wideContent) Modifier.fillMaxWidth(0.94f) else Modifier.widthIn(max = 560.dp).fillMaxWidth())
                    .background(
                        Brush.horizontalGradient(
                            listOf(
                                Color.Black.copy(alpha = 0.54f),
                                Color.Black.copy(alpha = 0.28f),
                                Color.Transparent,
                            )
                        )
                    )
            )
            Box(
                Modifier
                    .align(Alignment.TopEnd)
                    .fillMaxHeight()
                    .width(520.dp)
                    .background(
                        Brush.horizontalGradient(
                            listOf(
                                Color.Transparent,
                                Color.Black.copy(alpha = 0.22f),
                                Color.Black.copy(alpha = 0.50f),
                            )
                        )
                    )
            )
            // Top-left: game info, then (on Root) the tab strip and the
            // active tab's body stacked directly beneath it. Keeping the
            // strip and its content in the same column means tabs always
            // own their entries — no detached "strip up here, rows down
            // there" split. Submenu / confirm states fall through to the
            // bottom-left panel below.
            Column(
                modifier = Modifier
                    .align(Alignment.TopStart)
                    .fillMaxHeight()
                    .padding(20.dp)
                    .then(if (wideContent) Modifier.fillMaxWidth(0.90f) else Modifier.widthIn(max = 520.dp).fillMaxWidth()),
            ) {
                // In portrait reserve room on the right so the title / trophy
                // button clear the ✕ (which floats at the top-right corner).
                GameInfoHeader(modifier = if (isPortrait) Modifier.padding(end = 56.dp) else Modifier)
                if (state.value is State.Root) {
                    Spacer(Modifier.height(12.dp))
                    TabStrip()
                    Text(
                        "Press L and R to navigate between sections, on controller\n" +
                            "For touch, swipe to see all sections",
                        color = Color.White.copy(alpha = 0.45f),
                        fontSize = 10.sp,
                        lineHeight = 13.sp,
                        fontWeight = FontWeight.SemiBold,
                        modifier = Modifier.padding(top = 3.dp),
                    )
                    // The scope toggle only matters for settings tabs.
                    // PlayingNow's actions (Resume / Save State / etc.)
                    // are session controls, not persisted settings — no
                    // notion of "global vs game" applies.
                    if (currentTab.value != Tab.PlayingNow && !settingsOnly.value) {
                        Spacer(Modifier.height(4.dp))
                        ScopeToggle()
                    }
                    if (currentTab.value != Tab.PlayingNow) {
                        Spacer(Modifier.height(4.dp))
                        ResetScopeButton()
                    }
                    Spacer(Modifier.height(6.dp))
                    // weight(1f) gives RootTabs the remaining vertical
                    // space, bounding Performance/Renderer's verticalScroll
                    // so it actually scrolls instead of expanding off-screen.
                    Box(modifier = Modifier.weight(1f)) {
                        RootTabs()
                    }
                }
                else if (state.value is State.Achievements) {
                    // Render the achievements view INSIDE this top-left content
                    // column (below the GameInfoHeader) instead of the
                    // bottom-anchored modal box, which overlapped the header —
                    // the account row's username/points collided with the game
                    // cover and rich-presence line. weight(1f) hands the list the
                    // remaining height so it scrolls.
                    Spacer(Modifier.height(12.dp))
                    Box(modifier = Modifier.weight(1f)) {
                        AchievementsPanel(
                            modifier = Modifier.fillMaxWidth(),
                            onSignInClick = {
                                SettingsControllerNav.clearSelection()
                                state.value = State.AchievementsLogin
                            },
                            onHardcoreToggle = {
                                if (hardcoreOn.value) {
                                    // Gate the disable behind a confirm — users were
                                    // turning hardcore off by accident with a single
                                    // tap and silently losing challenge-mode unlocks.
                                    enterState(State.HardcoreDisableConfirm)
                                } else {
                                    // enterState (not a bare assignment) resets
                                    // modalSelection to 0 so controller focus
                                    // defaults to CANCEL — symmetric with the
                                    // disable path and safe against a stale index.
                                    enterState(State.HardcoreEnableConfirm)
                                }
                            },
                        )
                    }
                }
            }

            Column(
                modifier = Modifier
                    .align(Alignment.TopEnd)
                    .padding(20.dp),
                horizontalAlignment = Alignment.End,
            ) {
                // Top-right controls. The green ▶ only appears when configuring a
                // game's settings via long-press (settingsOnly + a previewGame, no
                // game running yet): there it BOOTS that game with the settings just
                // edited. While actually in-game it's hidden — resuming is the red
                // ✕'s job, so a second "play" button would be redundant. The ✕ is
                // red and always resumes/closes the overlay.
                Row(
                    horizontalArrangement = Arrangement.spacedBy(8.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    if (settingsOnly.value && previewGame.value != null) {
                        Box(
                            modifier = Modifier
                                .size(40.dp)
                                .clip(RoundedCornerShape(20.dp))
                                .background(Color(0xFF2E7D32).copy(alpha = 0.92f))
                                .border(1.dp, Color.White.copy(alpha = 0.30f), RoundedCornerShape(20.dp))
                                .clickable {
                                    val g = previewGame.value
                                    closeKeepingState()
                                    if (g != null) {
                                        // Mirror the library card's launch path: hide the
                                        // library so the new game's surface shows, and hand
                                        // file:// (all-files) games a bare /storage path — the
                                        // SAF FD bridge is only for content:// URIs, so a raw
                                        // "file://…" arg fails to open.
                                        WindowImpl.showLibrary.value = false
                                        val arg = if (g.uri.scheme == "file")
                                            (g.uri.path ?: g.uri.toString())
                                        else g.uri.toString()
                                        Main.launchGame(arg, g)
                                    }
                                },
                            contentAlignment = Alignment.Center,
                        ) {
                            Text("▶", color = Color.White, fontSize = 16.sp, fontWeight = FontWeight.Bold)
                        }
                    }
                    Box(
                        modifier = Modifier
                            .size(40.dp)
                            .clip(RoundedCornerShape(20.dp))
                            .background(Color(0xFFC62828).copy(alpha = 0.92f))
                            .border(1.dp, Color.White.copy(alpha = 0.30f), RoundedCornerShape(20.dp))
                            .clickable { closeAndResume() },
                        contentAlignment = Alignment.Center,
                    ) {
                        Text("✕", color = Color.White, fontSize = 18.sp, fontWeight = FontWeight.Bold)
                    }
                }
                Spacer(Modifier.height(4.dp))
                Text(
                    "Press B to exit on controller",
                    color = Color.White.copy(alpha = 0.45f),
                    fontSize = 9.sp,
                    fontWeight = FontWeight.SemiBold,
                    textAlign = TextAlign.End,
                )
                Text(
                    "Press Y to open RetroAchievements on controller",
                    color = Color.White.copy(alpha = 0.45f),
                    fontSize = 9.sp,
                    fontWeight = FontWeight.SemiBold,
                    textAlign = TextAlign.End,
                )
            }

            // Brand sits in the top-right-of-centre band: right of long game
            // titles (which start top-left) but left of the close button, so it
            // stops clashing with both. Anchored to the end edge for a stable
            // gap from the ✕ across screen sizes. Hidden in portrait, where the
            // narrow width makes its fixed 135dp inset overlap the title.
            if (!isPortrait) {
                BrandHeader(
                    modifier = Modifier
                        .align(Alignment.TopEnd)
                        .padding(top = 20.dp, end = 135.dp)
                )
            }

            // (The inline bottom-right achievements panel was removed — it's now
            // the header trophy button → dedicated achievements view, so the Play
            // tab can use the full width like every other tab.)

            // Bottom-left: confirm dialogs and slot pickers only. Root
            // content lives in the top-left column above with its tab
            // strip; this panel just hosts modal-ish flows that should
            // sit at the bottom of the screen (Exit / Reset confirms,
            // save-state slot grid).
            // State.Achievements renders in the top-left column (above) so it sits
            // below the header; everything else modal-ish uses this bottom box.
            if (state.value !is State.Root && state.value !is State.Achievements) {
                // The login form needs more vertical room than the other
                // bottom-left states (it has 2 text fields + disclaimer +
                // buttons), and on landscape phones with the keyboard up
                // 75% of the screen isn't quite enough — content clips and
                // verticalScroll only barely engages. Give the login state
                // a taller box.
                val maxFrac = when {
                    state.value is State.AchievementsLogin -> 0.92f
                    else -> 0.75f
                }
                // Slot pickers (Save/Load) span the full screen width so
                // the 2-row horizontal grid actually reaches across. Other
                // states (confirms, login) stay at the 360dp left column.
                val isSlotPicker = state.value is State.SaveStateSlots
                    || state.value is State.LoadStateSlots
                Box(
                    Modifier
                        .align(Alignment.BottomStart)
                        .fillMaxHeight(maxFrac)
                        .let { if (isSlotPicker) it.fillMaxWidth() else it.width(520.dp) }
                        .padding(start = 20.dp, end = if (isSlotPicker) 20.dp else 0.dp, bottom = 20.dp, top = 20.dp),
                    contentAlignment = Alignment.BottomStart,
                ) {
                    when (state.value) {
                        is State.SaveStateSlots -> SaveStatePicker.Render(
                            mode = SaveStatePicker.Mode.Save,
                            onDone = { closeAndResume() },
                            onBack = { state.value = State.Root },
                        )
                        is State.LoadStateSlots -> SaveStatePicker.Render(
                            mode = SaveStatePicker.Mode.Load,
                            onDone = { closeAndResume() },
                            onBack = { state.value = State.Root },
                        )
                        is State.ExitConfirm -> ExitConfirm()
                        is State.ResetConfirm -> ResetConfirm()
                        is State.AchievementsLogin -> AchievementsLoginPanel(
                            onClose = {
                                SettingsControllerNav.clearSelection()
                                state.value = State.Root
                            },
                        )
                        is State.HardcoreSaveStateBlocked -> HardcoreBlockedBubble()
                        is State.HardcoreEnableConfirm -> Unit // rendered fullscreen below
                        is State.HardcoreDisableConfirm -> Unit // rendered fullscreen below
                        is State.Achievements -> Unit // rendered in the top-left column
                        is State.Root -> Unit
                    }
                }
            }

            // PS2-BIOS-style fullscreen confirm for enabling hardcore mode.
            // Painted on top of everything else, so it eats taps and the
            // user must explicitly confirm or cancel before returning to
            // the menu. Enabling hardcore resets the running game (per
            // upstream Achievements::ResetHardcoreMode) so we want the
            // user to know exactly what's about to happen.
            if (state.value is State.HardcoreEnableConfirm) {
                HardcoreEnableConfirmFullscreen()
            }
            // Same PS2-BIOS-style fullscreen confirm for DISABLING hardcore.
            // Turning hardcore off mid-session drops you to softcore for the
            // rest of the run, so make the user explicitly confirm instead of
            // letting a stray tap silently flip it.
            if (state.value is State.HardcoreDisableConfirm) {
                HardcoreDisableConfirmFullscreen()
            }
            } // displayCutoutPadding inner box
        }
    }

    @Composable
    private fun GameInfoHeader(modifier: Modifier = Modifier) {
        // Prefer the cached GameInfo from Main.currentGame — it has the
        // pre-resolved compat (computed at library scan time), the cover
        // URL, and the container extension. Fallback to NativeApp.getPause*
        // for paths that lack a GameInfo (Swap/Boot Disc file picker, BIOS).
        val globalSettingsView =
            settingsOnly.value && currentSerial.value == null && previewGame.value == null
        val cached = if (globalSettingsView) null else (previewGame.value ?: Main.currentGame.value)
        val cachedTitle = cached?.title?.takeIf { it.isNotEmpty() }
        val nativeTitle = if (!globalSettingsView)
            NativeApp.getPauseGameTitle()?.takeIf { it.isNotEmpty() }
        else null
        val title = cachedTitle ?: nativeTitle ?: if (globalSettingsView) "General Settings" else "PS2 BIOS"
        val serial = cached?.serial
            ?: if (!globalSettingsView) NativeApp.getPauseGameSerial()?.takeIf { it.isNotEmpty() } else null
        val compatStars = when {
            cached != null -> cached.compatibility
            serial != null -> (NativeApp.getCompatibilityForSerial(serial) - 1).coerceIn(0, 5)
            else -> 0
        }
        val coverUrl = cached?.coverUrl
        val extension = cached?.extension?.takeIf { it.isNotEmpty() }

        Row(modifier = modifier, verticalAlignment = Alignment.Top) {
            // Cover thumbnail to the left, matching the library card's
            // 0.7 aspect ratio. 72dp tall fits next to a 22sp title +
            // 13sp serial line without crowding.
            Box(
                Modifier
                    .height(72.dp)
                    .aspectRatio(0.7f)
                    .clip(RoundedCornerShape(4.dp))
                    .background(Color(0xFF1B1A1A).copy(alpha = 0.5f)),
                contentAlignment = Alignment.Center,
            ) {
                val context = LocalContext.current
                if (coverUrl != null) {
                    // PS2 boxart matches the 0.7 aspect cell — Crop fills.
                    // PS1 jewel-case covers are squarer; Fit + Center
                    // letterboxes them inside the same cell so the full
                    // art is visible without cropping.
                    val scale = when (cached?.platform) {
                        com.armsx2.GamePlatform.PS1 -> ContentScale.Fit
                        else -> ContentScale.Crop
                    }
                    SubcomposeAsyncImage(
                        model = ImageRequest.Builder(context).data(coverUrl).crossfade(true).build(),
                        contentDescription = "$title cover",
                        contentScale = scale,
                        alignment = Alignment.Center,
                        modifier = Modifier.fillMaxSize(),
                        loading = { /* dim background shows through */ },
                        error = { Text("📀", color = Color(0xFF3F3F3F), fontSize = 28.sp) },
                    )
                } else {
                    Text("📀", color = Color(0xFF3F3F3F), fontSize = 28.sp)
                }
            }
            Spacer(Modifier.width(12.dp))

            // weight(fill = false) keeps the title from squeezing the trailing
            // RetroAchievements button off-screen on the narrow Play tab; the
            // widthIn cap keeps the button at a CONSISTENT position so it doesn't
            // drift toward the brand on the wider settings tabs.
            Column(modifier = Modifier.weight(1f, fill = false).widthIn(max = 240.dp)) {
                Text(
                    title,
                    color = Color.White,
                    fontSize = 22.sp,
                    fontWeight = FontWeight.Bold,
                    maxLines = 2,
                    overflow = TextOverflow.Ellipsis,
                )
                Spacer(Modifier.height(4.dp))
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Text(
                        if (globalSettingsView) "Global" else serial ?: "No disc",
                        color = if (serial != null || globalSettingsView) Color(0xFFAACCFF) else Color(0xFF808080),
                        fontSize = 13.sp,
                    )
                    if (serial != null) {
                        Spacer(Modifier.width(10.dp))
                        CompatStars(compatStars)
                    }
                    if (extension != null) {
                        Spacer(Modifier.width(8.dp))
                        ExtensionBadge(extension)
                    }
                    if (hardcoreOn.value) {
                        Spacer(Modifier.width(6.dp))
                        HardcoreBadge()
                    }
                }
                // Live RetroAchievements rich-presence subtitle. Mirrors
                // what RA's website shows under your active game; we surface
                // it locally so the user can see what the server is being
                // told. AchievementsPanel's 4s poll owns the writes.
                val rp = richPresence.value
                if (rp.isNotEmpty()) {
                    Spacer(Modifier.height(3.dp))
                    MarqueeRichPresence(rp)
                }
            }
            // RetroAchievements entry — sits just after the title (or next to
            // "General Settings" in the global view). Opens the dedicated
            // achievements view; controller users can also use the Open
            // Achievements hotkey / B to exit it.
            Spacer(Modifier.width(16.dp))
            Box(
                Modifier
                    .clip(RoundedCornerShape(8.dp))
                    .background(Color(0xFFB7892B).copy(alpha = 0.20f))
                    .border(1.dp, Color(0xFFE0A93A).copy(alpha = 0.55f), RoundedCornerShape(8.dp))
                    .clickable { state.value = State.Achievements }
                    .padding(horizontal = 10.dp, vertical = 6.dp),
                contentAlignment = Alignment.Center,
            ) {
                Text(
                    "🏆 RetroAchievements",
                    color = Color(0xFFFFD98A),
                    fontSize = 12.sp,
                    fontWeight = FontWeight.Bold,
                    maxLines = 1,
                )
            }
        }
    }

    /** One-line rich-presence subtitle. Renders directly when the string
     *  fits within the available width; runs a back-and-forth ping-pong
     *  marquee when it overflows. Holds at each end so the user has time
     *  to read both the start and the tail before the slide flips
     *  direction. Scrolls just far enough to align the right edge of the
     *  text with the right edge of the container — never past, never
     *  off-screen. */
    @Composable
    private fun MarqueeRichPresence(text: String) {
        // Measure the text's INTRINSIC width via TextMeasurer. onSizeChanged
        // on the rendered Text reports the constrained width (== container)
        // when the parent caps it, which makes overflow detection a no-op.
        val style = remember {
            TextStyle(color = Color(0xFFBBBBBB), fontSize = 11.sp)
        }
        val measurer = rememberTextMeasurer()
        val intrinsicTextWidth = remember(text, style) {
            measurer.measure(text = text, style = style, softWrap = false, maxLines = 1).size.width
        }
        var containerWidth by remember(text) { mutableIntStateOf(0) }

        Box(
            modifier = Modifier
                .fillMaxWidth()
                .clipToBounds()
                .onSizeChanged { containerWidth = it.width },
        ) {
            val overflowPx = intrinsicTextWidth - containerWidth
            if (containerWidth <= 0 || overflowPx <= 0) {
                // Fits, or container not yet measured — plain Text. Compose
                // settles the layout in the next frame and recomposes us
                // with a real containerWidth, at which point we either stay
                // here (fits) or drop into the marquee branch below.
                Text(
                    text = text,
                    style = style,
                    maxLines = 1,
                    softWrap = false,
                    overflow = TextOverflow.Visible,
                )
            } else {
                // Marquee branch. Composables here only enter the slot
                // table when overflowPx is real, so the keyframes can't
                // capture a "containerWidth = 0" max-offset and stick.
                MarqueeText(text = text, style = style, overflowPx = overflowPx)
            }
        }
    }

    @Composable
    private fun MarqueeText(text: String, style: TextStyle, overflowPx: Int) {
        // ~40 px/sec scroll. Brisk enough to feel responsive on long
        // rich-presence strings without overrunning the reading pace.
        // 1.5s hold at each end so eye has time to land before the slide
        // resumes.
        val holdMs = 1500
        val scrollMs = (overflowPx * 1000 / 40).coerceAtLeast(1200)
        val totalMs = holdMs * 2 + scrollMs * 2
        val maxOffset = overflowPx.toFloat()

        val transition = rememberInfiniteTransition(label = "rp-marquee")
        val offsetPx by transition.animateFloat(
            initialValue = 0f,
            targetValue = 0f,
            animationSpec = infiniteRepeatable(
                animation = keyframes {
                    durationMillis = totalMs
                    0f at 0
                    0f at holdMs                                using LinearEasing
                    -maxOffset at holdMs + scrollMs              using LinearEasing
                    -maxOffset at holdMs + scrollMs + holdMs    using LinearEasing
                    0f at totalMs                                using LinearEasing
                },
                repeatMode = RepeatMode.Restart,
            ),
            label = "rp-offset",
        )

        Text(
            text = text,
            style = style,
            maxLines = 1,
            softWrap = false,
            overflow = TextOverflow.Visible,
            modifier = Modifier.offset { IntOffset(offsetPx.toInt(), 0) },
        )
    }

    @Composable
    private fun CompatStars(filled: Int) {
        Row {
            repeat(5) { i ->
                val on = i < filled
                Text(
                    if (on) "★" else "☆",
                    color = if (on) Color(0xFFFFD33A) else Color(0xFF555555),
                    fontSize = 13.sp,
                )
            }
        }
    }

    /** PS2-blue rounded chip showing the container format (ISO / CHD /
     *  BIN / etc.). Mirrors GamesList's ExtensionBadge for parity with
     *  the library card chrome. */
    @Composable
    private fun ExtensionBadge(ext: String) {
        Box(
            Modifier
                .clip(RoundedCornerShape(4.dp))
                .background(Colors.pasx2_blue)
                .padding(horizontal = 5.dp, vertical = 1.dp),
        ) {
            Text(
                ext,
                color = Color.White,
                fontSize = 9.sp,
                fontWeight = FontWeight.Bold,
                maxLines = 1,
            )
        }
    }

    /** Small red badge displayed next to the extension badge when
     *  RetroAchievements Hardcore mode is active. Same shape as
     *  ExtensionBadge so they line up visually. */
    @Composable
    private fun HardcoreBadge() {
        Box(
            Modifier
                .clip(RoundedCornerShape(4.dp))
                .background(Color(0xFFB22222))
                .padding(horizontal = 5.dp, vertical = 1.dp),
        ) {
            Text(
                "HC",
                color = Color.White,
                fontSize = 9.sp,
                fontWeight = FontWeight.Bold,
                maxLines = 1,
            )
        }
    }

    @Composable
    private fun BrandHeader(modifier: Modifier = Modifier) {
        // Brand text column (ARMSX2 + version stacked, version centered
        // under the ARMSX2 text only) sits beside the logo. The version
        // line is horizontally centered against the ARMSX2 word's
        // intrinsic width — not against the whole row including the
        // icon — so it reads as a label belonging to the wordmark.
        Row(
            modifier = modifier,
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Column(horizontalAlignment = Alignment.CenterHorizontally) {
                Text("ARMSX2", color = Color.White, fontSize = 18.sp, fontWeight = FontWeight.Bold)
                Text(versionString, color = Color(0xFF888888), fontSize = 11.sp)
            }
            Spacer(Modifier.width(8.dp))
            Image(
                painter = painterResource(id = R.drawable.savetowerforeground),
                contentDescription = null,
                modifier = Modifier.size(32.dp),
            )
        }
    }

    /** Active tab body. Rendered in the top-left column directly under
     *  TabStrip, so the strip and its entries stay visually attached.
     *  Width and horizontal padding come from the parent column. */
    @Composable
    private fun RootTabs() {
        val tab = currentTab.value
        if (tab == Tab.PlayingNow && !settingsOnly.value) {
            PlayingNowTab()
            return
        }

        SettingsControllerNav.begin(tab.name)
        when (tab) {
            Tab.PlayingNow -> PlayingNowTab()
            Tab.Performance -> PerformanceTab(settingsState)
            Tab.Renderer -> RendererTab(settingsState)
            Tab.Fixes -> FixesTab(settingsState)
            Tab.Audio -> AudioTab(settingsState)
            Tab.Patches -> PatchesTab(settingsState)
            Tab.Network -> NetworkTab(settingsState)
            Tab.Overlay -> OverlayTab(settingsState)
            Tab.Pad -> PadTab(settingsState)
            Tab.Skins -> SkinsTab(settingsState)
            Tab.Hotkeys -> HotkeysTab(settingsState)
            Tab.Recompiler -> RecompilerTab(settingsState)
            Tab.App -> AppTab()
            Tab.Info -> GameInfoTab()
        }
        SettingsControllerNav.end()
    }

    /** Game properties — Title / Serial / CRC / Region / Type / Path, each row
     *  tap-to-copy. Uses the previewed (long-pressed) game, or the running game.
     *  CRC comes from the game-list entry (works without booting). */
    @Composable
    private fun GameInfoTab() {
        val game = previewGame.value ?: Main.currentGame.value
        val context = androidx.compose.ui.platform.LocalContext.current
        // getGameTitle now falls back to an on-demand disc scan when the native
        // game-list cache misses (which it usually does — the Android library is
        // scanned in Kotlin), so resolve the CRC OFF the UI thread to avoid
        // janking the Info tab. The row stays hidden until it resolves.
        var crc by remember(game?.uri) { mutableStateOf<String?>(null) }
        LaunchedEffect(game?.uri) {
            crc = withContext(Dispatchers.IO) {
                runCatching {
                    game?.uri?.let { uri ->
                        NativeApp.getGameTitle(uri.toString()).split("|").getOrNull(2)
                            ?.substringAfter('(', "")?.substringBefore(')')?.trim()
                            ?.takeIf { it.isNotEmpty() && it != "00000000" }
                    }
                }.getOrNull()
            }
        }
        Column(
            Modifier
                .fillMaxWidth()
                .verticalScroll(rememberScrollState())
                .padding(horizontal = 4.dp),
        ) {
            InfoCopyRow("Title", game?.title)
            InfoCopyRow("Serial", game?.serial)
            InfoCopyRow("CRC", crc)
            InfoCopyRow(
                "Time Played",
                PlayTime.formatPlayed(PlayTime.playedSeconds(game?.serial)).takeIf { it.isNotEmpty() },
            )
            InfoCopyRow(
                "Last Played",
                PlayTime.formatLastPlayed(PlayTime.lastPlayedMillis(game?.serial)).takeIf { it.isNotEmpty() },
            )
            InfoCopyRow("Region", game?.region)
            InfoCopyRow("Type", game?.extension?.takeIf { it.isNotEmpty() })
            InfoCopyRow("Path", game?.uri?.toString())
            if (game != null) {
                Spacer(Modifier.height(4.dp))
                CustomCoverControls(game)
                ProfileImportExportControls(game)
                // Always show — some launchers report isRequestPinShortcutSupported=false
                // even when they accept the pin, and others fall back to the legacy
                // INSTALL_SHORTCUT broadcast; pin() tries both and toasts on real failure.
                Spacer(Modifier.height(8.dp))
                Text(
                    "Add to Home Screen",
                    color = Colors.pasx2_blue,
                    fontSize = 12.sp,
                    fontWeight = FontWeight.Bold,
                    modifier = Modifier
                        .fillMaxWidth()
                        .clickable {
                            val ok = com.armsx2.HomeShortcuts.pin(context, game)
                            android.widget.Toast.makeText(
                                context,
                                if (ok) "Added to home screen — check your launcher"
                                else "Your launcher didn't accept the shortcut",
                                android.widget.Toast.LENGTH_SHORT,
                            ).show()
                        }
                        .padding(vertical = 8.dp, horizontal = 4.dp),
                )
            }
            Spacer(Modifier.height(8.dp))
            Text(
                "Tap a row to copy it to the clipboard.",
                color = Color.White.copy(alpha = 0.5f),
                fontSize = 11.sp,
            )
        }
    }

    /** Lets the user pick a local image as this game's cover — the only way to
     *  give a cover to serial-less games (homebrew, ELF ports) the online repo
     *  can't match. Writes via [CustomCovers]; the library shelf picks it up. */
    @Composable
    private fun CustomCoverControls(game: GameInfo) {
        val context = LocalContext.current
        val scope = rememberCoroutineScope()
        val hasCustom = remember(game.uri, CustomCovers.version.value) { mutableStateOf(false) }
        LaunchedEffect(game.uri, CustomCovers.version.value) {
            hasCustom.value = withContext(Dispatchers.IO) { CustomCovers.fileFor(context, game) != null }
        }
        val picker = rememberLauncherForActivityResult(
            ActivityResultContracts.OpenDocument(),
        ) { uri ->
            if (uri != null) {
                // The copy streams a possibly-multi-MB image over the SAF FD bridge,
                // so do it off the main thread (matches the read side); the version
                // bump inside set() re-resolves the shelf + this panel.
                scope.launch {
                    val ok = withContext(Dispatchers.IO) { CustomCovers.set(context, game, uri) }
                    android.widget.Toast.makeText(
                        context,
                        if (ok) "Custom cover set" else "Couldn't set cover",
                        android.widget.Toast.LENGTH_SHORT,
                    ).show()
                }
            }
        }
        Column(Modifier.fillMaxWidth().padding(vertical = 8.dp, horizontal = 4.dp)) {
            Text("Cover", color = Colors.pasx2_blue, fontSize = 11.sp)
            Spacer(Modifier.height(6.dp))
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                CoverPill(if (hasCustom.value) "Change cover" else "Set custom cover") {
                    picker.launch(arrayOf("image/*"))
                }
                if (hasCustom.value) {
                    CoverPill("Remove") {
                        scope.launch {
                            withContext(Dispatchers.IO) { CustomCovers.remove(context, game) }
                            android.widget.Toast.makeText(
                                context, "Custom cover removed", android.widget.Toast.LENGTH_SHORT,
                            ).show()
                        }
                    }
                }
            }
            Spacer(Modifier.height(4.dp))
            Text(
                "Use your own image for this game's cover — handy for homebrew or discs with no serial.",
                color = Color.White.copy(alpha = 0.5f),
                fontSize = 11.sp,
            )
        }
    }

    /** Export / import this game's tuned settings as a portable .json profile so a
     *  known-good per-game config can be shared or moved between installs. Keyed by
     *  serial; export snapshots the fully-resolved Settings, import re-persists them
     *  as this game's per-game override (applies on next launch). */
    @Composable
    private fun ProfileImportExportControls(game: GameInfo) {
        val serial = game.serial?.takeIf { it.isNotBlank() } ?: return
        val context = LocalContext.current
        val scope = rememberCoroutineScope()
        val exporter = rememberLauncherForActivityResult(
            ActivityResultContracts.CreateDocument("application/json"),
        ) { uri ->
            if (uri != null) scope.launch {
                val ok = withContext(Dispatchers.IO) {
                    runCatching {
                        val json = ConfigStore.resolveForGame(serial).toJson().toString(2)
                        context.contentResolver.openOutputStream(uri)?.use { it.write(json.toByteArray()) }
                        true
                    }.getOrDefault(false)
                }
                android.widget.Toast.makeText(
                    context, if (ok) "Profile exported" else "Export failed",
                    android.widget.Toast.LENGTH_SHORT,
                ).show()
            }
        }
        val importer = rememberLauncherForActivityResult(
            ActivityResultContracts.OpenDocument(),
        ) { uri ->
            if (uri != null) scope.launch {
                val ok = withContext(Dispatchers.IO) {
                    runCatching {
                        val text = context.contentResolver.openInputStream(uri)
                            ?.bufferedReader()?.use { it.readText() } ?: return@runCatching false
                        val imported = Settings.fromJson(org.json.JSONObject(text))
                        ConfigStore.save(SettingsScope.Game, serial, imported)
                        true
                    }.getOrDefault(false)
                }
                android.widget.Toast.makeText(
                    context,
                    if (ok) "Profile imported — applies on next launch" else "Import failed",
                    android.widget.Toast.LENGTH_SHORT,
                ).show()
            }
        }
        Column(Modifier.fillMaxWidth().padding(vertical = 8.dp, horizontal = 4.dp)) {
            Text("Per-Game Profile", color = Colors.pasx2_blue, fontSize = 11.sp)
            Spacer(Modifier.height(6.dp))
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                CoverPill("Export") { exporter.launch("armsx2-profile-$serial.json") }
                CoverPill("Import") {
                    importer.launch(arrayOf("application/json", "application/octet-stream", "text/plain"))
                }
            }
            Spacer(Modifier.height(4.dp))
            Text(
                "Save this game's tuned settings to a file, or load a shared profile.",
                color = Color.White.copy(alpha = 0.5f),
                fontSize = 11.sp,
            )
        }
    }

    @Composable
    private fun CoverPill(label: String, onClick: () -> Unit) {
        Box(
            Modifier
                .clip(RoundedCornerShape(16.dp))
                .background(Colors.pasx2_blue.copy(alpha = 0.20f))
                .border(1.dp, Colors.pasx2_blue.copy(alpha = 0.55f), RoundedCornerShape(16.dp))
                .clickable(onClick = onClick)
                .padding(horizontal = 14.dp, vertical = 8.dp),
        ) {
            Text(label, color = Color.White, fontSize = 12.sp, fontWeight = FontWeight.Medium)
        }
    }

    @Composable
    private fun InfoCopyRow(label: String, value: String?) {
        if (value.isNullOrEmpty()) return
        val ctx = LocalContext.current
        Column(
            Modifier
                .fillMaxWidth()
                .clickable {
                    runCatching {
                        val cb = ctx.getSystemService(android.content.Context.CLIPBOARD_SERVICE)
                            as android.content.ClipboardManager
                        cb.setPrimaryClip(android.content.ClipData.newPlainText(label, value))
                    }
                    android.widget.Toast
                        .makeText(ctx, "$label copied", android.widget.Toast.LENGTH_SHORT)
                        .show()
                }
                .padding(vertical = 8.dp, horizontal = 4.dp),
        ) {
            Text(
                label,
                color = Colors.pasx2_blue,
                fontSize = 11.sp,
                fontWeight = FontWeight.Bold,
            )
            Text(value, color = Color.White, fontSize = 13.sp)
        }
    }

    /** Horizontal tab chip strip. Active tab gets PS2-blue underline +
     *  brighter text; inactive tabs are dim. Tappable across the whole
     *  chip area. */
    @Composable
    private fun TabStrip() {
        val scroll = rememberScrollState()
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .horizontalScroll(scroll),
            horizontalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            val tabs = if (settingsOnly.value) {
                listOf(Tab.Performance, Tab.Renderer, Tab.Fixes, Tab.Audio, Tab.Patches, Tab.Network, Tab.Overlay, Tab.Pad, Tab.Skins, Tab.Hotkeys, Tab.Recompiler, Tab.App, Tab.Info)
            } else {
                Tab.values().toList()
            }
            tabs.forEach { tab ->
                val active = currentTab.value == tab
                val chipWidth = when (tab) {
                    Tab.Patches, Tab.Network, Tab.Overlay, Tab.Hotkeys -> 72.dp
                    Tab.Pad -> 52.dp
                    Tab.PlayingNow -> 52.dp
                    else -> 64.dp
                }
                Column(
                    modifier = Modifier
                        .width(chipWidth)
                        .clickable {
                            currentTab.value = tab
                            SettingsControllerNav.clearSelection()
                            resetSettingsAdjustGate()
                        }
                        .padding(vertical = 4.dp),
                    horizontalAlignment = Alignment.CenterHorizontally,
                ) {
                    Text(
                        tab.label,
                        color = if (active) Color.White else Color(0xFF888888),
                        fontSize = 12.sp,
                        fontWeight = FontWeight.Bold,
                    )
                    Spacer(Modifier.height(3.dp))
                    Box(
                        Modifier
                            .fillMaxWidth()
                            .height(2.dp)
                            .background(
                                if (active) Colors.pasx2_blue else Color.Transparent
                            ),
                    )
                }
            }
        }
    }

    /** Tiny Global / Game pill that decides where settings tab edits
     *  land. Game side is disabled (and the toggle locked to Global)
     *  when there's no current serial — BIOS boots have nowhere to write
     *  per-game overrides. Active half gets the PS2-blue accent so the
     *  user can see at a glance what scope is "live". */
    @Composable
    private fun ScopeToggle() {
        val serial = currentSerial.value
        val gameEnabled = serial != null
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .clip(RoundedCornerShape(4.dp))
                .background(Color(0xFF1A1A1A))
                .border(1.dp, Color.White.copy(alpha = 0.08f), RoundedCornerShape(4.dp))
                .height(20.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            ScopeHalf(
                label = "Global",
                active = settingsScope.value == SettingsScope.Global,
                enabled = true,
                modifier = Modifier.weight(1f),
            ) {
                if (settingsScope.value != SettingsScope.Global) {
                    // Switching tiers must RE-HYDRATE the edited Settings from
                    // the tier being switched TO. Without this the prior tier's
                    // values stay in settingsState and the next edit persists
                    // them into the wrong tier (the global ↔ per-game bleed).
                    settingsScope.value = SettingsScope.Global
                    settingsState.value = ConfigStore.loadGlobal()
                    syncQuickTogglesFromSettings(settingsState.value)
                }
            }
            ScopeHalf(
                label = if (serial != null) "Game · $serial" else "Game",
                active = settingsScope.value == SettingsScope.Game,
                enabled = gameEnabled,
                modifier = Modifier.weight(1f),
            ) {
                if (gameEnabled && settingsScope.value != SettingsScope.Game) {
                    settingsScope.value = SettingsScope.Game
                    settingsState.value = ConfigStore.resolveForGame(currentSerial.value)
                    syncQuickTogglesFromSettings(settingsState.value)
                }
            }
        }
    }

    @Composable
    private fun ScopeHalf(
        label: String,
        active: Boolean,
        enabled: Boolean,
        modifier: Modifier = Modifier,
        onClick: () -> Unit,
    ) {
        val bg = when {
            !enabled -> Color.Transparent
            active   -> Colors.pasx2_blue.copy(alpha = 0.30f)
            else     -> Color.Transparent
        }
        val fg = when {
            !enabled -> Color(0xFF555555)
            active   -> Color.White
            else     -> Color(0xFF888888)
        }
        Box(
            modifier = modifier
                .fillMaxHeight()
                .background(bg)
                .clickable(enabled = enabled, onClick = onClick),
            contentAlignment = Alignment.Center,
        ) {
            Text(
                label,
                color = fg,
                fontSize = 9.sp,
                fontWeight = FontWeight.Bold,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
            )
        }
    }

    /** Scope-aware "reset to defaults" row, shown under the scope toggle. Two buttons:
     *  "Reset Tab" clears only the tab you're looking at (so e.g. resetting the Pad tab
     *  no longer wipes every other game setting), and "Reset All"/"Reset Game" does the
     *  whole-tier reset (the old behaviour). Each requires a confirming second tap;
     *  arming one disarms the other. The per-tab button hides on tabs with nothing to
     *  reset (Play/Skins/Info). */
    @Composable
    private fun ResetScopeButton() {
        val tabArmed = remember(currentTab.value, settingsScope.value, settingsOnly.value) {
            mutableStateOf(false)
        }
        val allArmed = remember(currentTab.value, settingsScope.value, settingsOnly.value) {
            mutableStateOf(false)
        }
        val game = settingsScope.value == SettingsScope.Game && currentSerial.value != null
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(4.dp),
        ) {
            if (currentTabHasReset()) {
                ResetChip(
                    modifier = Modifier.weight(1f),
                    id = "settings-reset-tab",
                    label = if (tabArmed.value) "Tap to confirm" else "Reset Tab",
                    armed = tabArmed.value,
                ) {
                    if (tabArmed.value) {
                        resetCurrentTab(); tabArmed.value = false
                    } else {
                        tabArmed.value = true; allArmed.value = false
                    }
                }
            }
            ResetChip(
                modifier = Modifier.weight(1f),
                id = "settings-reset-all",
                label = when {
                    allArmed.value -> "Tap to confirm"
                    game -> "Reset Game"
                    else -> "Reset All"
                },
                armed = allArmed.value,
            ) {
                if (allArmed.value) {
                    resetCurrentScope(); allArmed.value = false
                } else {
                    allArmed.value = true; tabArmed.value = false
                }
            }
        }
    }

    /** One pill in the reset row. Red when armed (awaiting confirm tap). */
    @Composable
    private fun RowScope.ResetChip(
        modifier: Modifier,
        id: String,
        label: String,
        armed: Boolean,
        onClick: () -> Unit,
    ) {
        Box(
            modifier = modifier
                .clip(RoundedCornerShape(4.dp))
                .background(Color(0xFF1A1A1A))
                .border(1.dp, Color.White.copy(alpha = 0.08f), RoundedCornerShape(4.dp))
                .height(20.dp)
                .controllerFocusable(id, onConfirm = onClick)
                .clickable(onClick = onClick),
            contentAlignment = Alignment.Center,
        ) {
            Text(
                label,
                color = if (armed) Color(0xFFE53935) else Colors.pasx2_blue,
                fontSize = 9.sp,
                fontWeight = FontWeight.Bold,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
            )
        }
    }

    /** Playing Now — pause-menu actions laid out as a 4-wide bubble grid.
     *  Three rows of four cells keep widths constant and fit without
     *  scrolling on phone landscape. Primary (Resume) and Danger (Close)
     *  accents land the eye on the most common entry and the destructive
     *  exit. Toggleable bubbles (Renderer, Frame Limit) carry their
     *  current value on a second line. */
    @Composable
    private fun PlayingNowTab() {
      BoxWithConstraints(Modifier.fillMaxSize()) {
        // Every cell gets the SAME explicit height (rows are uniform). Earlier the
        // cells used aspectRatio, which yields to content — so cells with a state
        // line (Renderer/Frame Limit/OSD) grew taller than plain cells and the
        // rows looked ragged / overran the bottom. Derive one height that fits all
        // 3 rows in the available space, clamped so cells aren't tiny or huge.
        val gap = 8.dp
        // Cap shorter (was 112) so the Play-tab bubbles stay compact on
        // tall panels / small screens instead of ballooning to fill height.
        // 4 rows now (added Memory Cards), so divide the height by 4.
        // 3 uniform rows again (Memory Cards moved out to the right column, so the
        // bubbles no longer shrink to fit a 4th row).
        val cellH = ((maxHeight - gap * 2) / 3).coerceIn(64.dp, 96.dp)
        Row(
            modifier = Modifier
                .align(Alignment.TopStart)
                .fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(gap),
        ) {
          Column(
            modifier = Modifier.weight(1f),
            verticalArrangement = Arrangement.spacedBy(gap),
          ) {
            // Row 1: primary + save/load + swap disc.
	            BubbleRow(cellH) {
	                BubbleButton(
	                    "Resume",
	                    LineAwesomeIcons.PlaySolid,
	                    accent = BubbleAccent.Primary,
	                    selected = playSelection.value == 0,
	                    modifier = Modifier.weight(1f),
	                ) { activatePlaySelection(0) }
	                BubbleButton(
	                    "Save State",
	                    LineAwesomeIcons.SaveSolid,
	                    // Saving is allowed in hardcore — only loading is blocked.
	                    selected = playSelection.value == 1,
	                    modifier = Modifier.weight(1f),
	                ) { activatePlaySelection(1) }
	                BubbleButton(
	                    "Load State",
	                    LineAwesomeIcons.FolderOpenSolid,
	                    dim = hardcoreOn.value,
	                    selected = playSelection.value == 2,
	                    modifier = Modifier.weight(1f),
	                ) { activatePlaySelection(2) }
	                BubbleButton(
	                    "Swap Disc",
	                    LineAwesomeIcons.CompactDiscSolid,
	                    selected = playSelection.value == 3,
	                    modifier = Modifier.weight(1f),
	                ) { activatePlaySelection(3) }
	            }
            // Row 2: boot disc + library + renderer + frame limit.
            BubbleRow(cellH) {
	                BubbleButton(
	                    "Boot Disc",
	                    LineAwesomeIcons.CompactDiscSolid,
	                    selected = playSelection.value == 4,
	                    modifier = Modifier.weight(1f),
	                ) { activatePlaySelection(4) }
	                BubbleButton(
	                    "Library",
	                    LineAwesomeIcons.ThLargeSolid,
	                    selected = playSelection.value == 5,
	                    modifier = Modifier.weight(1f),
	                ) { activatePlaySelection(5) }
	                BubbleButton(
	                    "Renderer",
	                    LineAwesomeIcons.CubeSolid,
                    stateLine = when (rendererMode.value) {
                        RendererMode.Auto -> "Auto"
                        RendererMode.Hardware -> "Hardware"
                        RendererMode.Software -> "Software"
	                    },
	                    accent = BubbleAccent.Active,
	                    selected = playSelection.value == 6,
	                    modifier = Modifier.weight(1f),
	                ) { activatePlaySelection(6) }
	                BubbleButton(
	                    "Frame Limit",
	                    LineAwesomeIcons.TachometerAltSolid,
                    stateLine = if (frameLimitOn.value) "On" else "Off",
	                    accent = if (frameLimitOn.value)
	                        BubbleAccent.Active else BubbleAccent.Normal,
	                    selected = playSelection.value == 7,
	                    modifier = Modifier.weight(1f),
	                ) { activatePlaySelection(7) }
	            }
            // Row 3: touch layout, OSD master toggle, reset, close.
            BubbleRow(cellH) {
	                BubbleButton(
	                    "Touch Layout",
	                    LineAwesomeIcons.ThLargeSolid,
	                    selected = playSelection.value == 8,
	                    modifier = Modifier.weight(1f),
	                ) { activatePlaySelection(8) }
	                BubbleButton(
	                    "OSD",
	                    if (osdShown.value) LineAwesomeIcons.EyeSolid
                    else LineAwesomeIcons.EyeSlashSolid,
                    stateLine = if (osdShown.value) "On" else "Off",
	                    accent = if (osdShown.value)
	                        BubbleAccent.Active else BubbleAccent.Normal,
	                    selected = playSelection.value == 9,
	                    modifier = Modifier.weight(1f),
	                ) { activatePlaySelection(9) }
	                BubbleButton(
	                    "Reset",
	                    LineAwesomeIcons.RedoAltSolid,
	                    selected = playSelection.value == 10,
	                    modifier = Modifier.weight(1f),
	                ) { activatePlaySelection(10) }
	                BubbleButton(
	                    "Close Game",
	                    LineAwesomeIcons.PowerOffSolid,
	                    accent = BubbleAccent.Danger,
	                    selected = playSelection.value == 11,
	                    modifier = Modifier.weight(1f),
	                ) { activatePlaySelection(11) }
	            }
          }
          // Memory Cards — full-height button on the RIGHT of the grid (its per-game
          // Slot 1 picker #137 opens here in-game). Kept beside the grid so the main
          // bubbles keep their full 3-row height instead of shrinking for a 4th row.
          BubbleButton(
              "Memory Cards",
              LineAwesomeIcons.SdCardSolid,
              selected = playSelection.value == 12,
              modifier = Modifier
                  .width(94.dp)
                  .height(cellH * 3 + gap * 2),
          ) { activatePlaySelection(12) }
        }
      }
    }

    /** Even-spaced four-cell row at a fixed [height], used by [PlayingNowTab].
     *  The fixed height makes every cell uniform regardless of its content. */
    @Composable
    private fun BubbleRow(height: Dp, content: @Composable RowScope.() -> Unit) {
        Row(
            modifier = Modifier.fillMaxWidth().height(height),
            horizontalArrangement = Arrangement.spacedBy(8.dp),
            content = content,
        )
    }

    /** Visual variants for [BubbleButton]. Normal is the default surface;
     *  Primary tints the bubble with the PS2-blue accent (Resume); Active
     *  is a softer accent used for toggleable bubbles whose state is
     *  currently "on"; Danger paints the bubble red for destructive
     *  actions. */
    private enum class BubbleAccent { Normal, Primary, Active, Danger }

    /** Square bubble action. Icon centered on top, label below, optional
     *  state line under the label (e.g. "On" / "Software") so toggleable
     *  bubbles communicate their current value without separate text. */
    @Composable
	    private fun BubbleButton(
	        label: String,
	        icon: ImageVector,
	        modifier: Modifier = Modifier,
	        stateLine: String? = null,
	        accent: BubbleAccent = BubbleAccent.Normal,
	        dim: Boolean = false,
	        selected: Boolean = false,
	        onClick: () -> Unit,
	    ) {
        // Per-variant palette. Dim wins over any accent so a Hardcore-mode
        // Save State row still reads "blocked".
        val bg: Color
        val border: Color
        val fg: Color
        when {
            dim -> {
                bg = Color(0xFF1F1F1F)
                border = Color(0xFF2E2E2E)
                fg = Color(0xFF666666)
            }
            accent == BubbleAccent.Primary -> {
                bg = Colors.pasx2_blue.copy(alpha = 0.22f)
                border = Colors.pasx2_blue.copy(alpha = 0.65f)
                fg = Color.White
            }
            accent == BubbleAccent.Active -> {
                bg = Color(0xFF222F40)
                border = Colors.pasx2_blue.copy(alpha = 0.50f)
                fg = Color.White
            }
            accent == BubbleAccent.Danger -> {
                bg = Color(0xFF2E1818)
                border = Color(0xFFFF6B6B).copy(alpha = 0.55f)
                fg = Color(0xFFFF8B8B)
            }
            else -> {
                bg = Color(0xFF1F2123)
                border = Color.White.copy(alpha = 0.10f)
                fg = Color.White
            }
        }

	        var focused by remember { mutableStateOf(false) }
	        val highlighted = focused || selected
	        val glowBlue = Color(0xFF3DA5FF)
	        Column(
	            modifier = modifier
	                .fillMaxHeight()
	                .onFocusChanged { focused = it.isFocused }
                // Controller selection highlight: blue glow + outline when this
	                // bubble has D-pad focus.
	                .then(
	                    if (highlighted)
	                        Modifier.shadow(10.dp, RoundedCornerShape(10.dp), ambientColor = glowBlue, spotColor = glowBlue)
	                    else Modifier
	                )
	                .clip(RoundedCornerShape(10.dp))
	                .background(bg)
	                .border(if (highlighted) 2.dp else 1.dp, if (highlighted) glowBlue else border, RoundedCornerShape(10.dp))
                .clickable(enabled = !dim, onClick = onClick)
                .padding(horizontal = 4.dp, vertical = 4.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.Center,
        ) {
            Image(
                imageVector = icon,
                contentDescription = null,
                colorFilter = ColorFilter.tint(fg),
                modifier = Modifier.size(16.dp),
            )
            Spacer(Modifier.height(2.dp))
            Text(
                label,
                color = fg,
                fontSize = 9.sp,
                fontWeight = FontWeight.Bold,
                textAlign = TextAlign.Center,
                maxLines = 2,
                overflow = TextOverflow.Ellipsis,
            )
            if (stateLine != null) {
                // Active accent → state line in PS2 blue (matches the
                // ToggleBubble "On" treatment in the Performance grid).
                // Other variants keep the muted-fg colour so Normal bubbles
                // don't accidentally read as active.
                val stateColor = if (accent == BubbleAccent.Active)
                    Colors.pasx2_blue else fg.copy(alpha = 0.7f)
                Text(
                    stateLine,
                    color = stateColor,
                    fontSize = 10.sp,
                    fontWeight = FontWeight.Bold,
                    textAlign = TextAlign.Center,
                    maxLines = 1,
                )
            }
        }
    }


    /**
     * Thin horizontal divider with a left-anchored fade — opaque at the
     * left edge (where the row label starts) and fading to transparent
     * at the right edge. The row "aura" backgrounds use a fainter
     * version of this same gradient so the dividers and rows visually
     * tie together.
     */
    @Composable
    private fun MenuDivider() {
        Box(
            Modifier
                .fillMaxWidth()
                .height(1.dp)
                .background(
                    Brush.horizontalGradient(
                        listOf(
                            Color.White.copy(alpha = 0.35f),
                            Color.Transparent,
                        )
                    )
                ),
        )
    }

    @Composable
    private fun ExitConfirm() {
        Column(verticalArrangement = Arrangement.spacedBy(2.dp)) {
            Text(
                "Close current game?",
                color = Color.White,
                fontSize = 14.sp,
                modifier = Modifier.padding(bottom = 6.dp),
            )
	            MenuRow("Save State And Exit", selected = modalSelection.value == 0) {
	                exitSaveStateAndExit()
	            }
	            MenuRow("Exit Without Saving", danger = true, selected = modalSelection.value == 1) {
	                exitWithoutSaving()
	            }
	            MenuRow("Back", selected = modalSelection.value == 2) { enterState(State.Root) }
        }
    }

    @Composable
    private fun ResetConfirm() {
        Column(verticalArrangement = Arrangement.spacedBy(2.dp)) {
            Text(
                "Reset the system? Unsaved progress will be lost.",
                color = Color.White,
                fontSize = 14.sp,
                modifier = Modifier.padding(bottom = 6.dp),
            )
	            MenuRow("Yes, Reset", danger = true, selected = modalSelection.value == 0) {
	                resetSystem()
	            }
	            MenuRow("Back", selected = modalSelection.value == 1) { enterState(State.Root) }
        }
    }

    /** Modern red-bubble hint shown when the user taps Save / Load State
     *  while hardcore mode is active. Fits inside the bottom-left modal
     *  box; auto-dismisses on tap. */
    @Composable
    private fun HardcoreBlockedBubble() {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .clip(RoundedCornerShape(10.dp))
                .background(Color(0xFF5A1A1A))
                .clickable { state.value = State.Root }
                .padding(14.dp),
        ) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text("⛔", fontSize = 16.sp, color = Color(0xFFFF6B6B))
                Spacer(Modifier.width(8.dp))
                Text(
                    "Disabled in Hardcore",
                    color = Color.White,
                    fontSize = 14.sp,
                    fontWeight = FontWeight.Bold,
                )
            }
            Spacer(Modifier.height(6.dp))
            Text(
                "Loading save states is blocked while RetroAchievements Hardcore mode is active (saving is still allowed). Disable Hardcore from the Achievements panel to load.",
                color = Color(0xFFEEDDDD),
                fontSize = 11.sp,
            )
            Spacer(Modifier.height(8.dp))
            Text(
                "Tap to dismiss",
                color = Color(0xFFFF6B6B),
                fontSize = 10.sp,
            )
        }
    }

    /** Fullscreen confirmation modal for enabling hardcore mode. Styled
     *  to evoke the PS2 BIOS UI — centered black panel with a thin double
     *  border and chunky monospace-feeling title. The action enables the
     *  Achievements/HardcoreMode flag in the BASE settings layer; native
     *  ApplySettings folds that into Achievements::ResetHardcoreMode
     *  which resets the running game. */
    @Composable
    private fun HardcoreEnableConfirmFullscreen() {
        Box(
            modifier = Modifier
                .fillMaxSize()
                .background(Color(0xCC000000))
                // Eat taps so background controls don't trigger.
                .clickable(
                    indication = null,
                    interactionSource = remember { androidx.compose.foundation.interaction.MutableInteractionSource() },
                ) {},
            contentAlignment = Alignment.Center,
        ) {
            Column(
                modifier = Modifier
                    .width(420.dp)
                    .clip(RoundedCornerShape(2.dp))
                    .background(Color(0xFF0A0A18))
                    .border(2.dp, Color(0xFF8888AA), RoundedCornerShape(2.dp))
                    .padding(2.dp)
                    .border(1.dp, Color(0xFF333366), RoundedCornerShape(2.dp))
                    .padding(20.dp),
                horizontalAlignment = Alignment.CenterHorizontally,
            ) {
                Text(
                    "ENABLE HARDCORE MODE",
                    color = Color(0xFFFF8888),
                    fontSize = 18.sp,
                    fontWeight = FontWeight.Bold,
                    letterSpacing = 2.sp,
                )
                Spacer(Modifier.height(2.dp))
                Box(
                    Modifier
                        .fillMaxWidth()
                        .height(1.dp)
                        .background(Color(0xFF8888AA)),
                )
                Spacer(Modifier.height(14.dp))
                Text(
                    "Hardcore mode disables loading save states and cheats (you can still save, and widescreen/interlace patches still apply). Achievements unlocked while hardcore is active are recorded as such on RetroAchievements.",
                    color = Color(0xFFDDDDEE),
                    fontSize = 13.sp,
                )
                Spacer(Modifier.height(8.dp))
                Text(
                    // No game loaded (global toggle): it's saved now and engages on the
                    // next launch — nothing to reset. With a game running it reboots clean.
                    if (Main.eState.value == EmuState.STOPPED)
                        "Enabled globally — applies when you launch a game."
                    else
                        "The current game will reset.",
                    color = Color(0xFFFFAAAA),
                    fontSize = 13.sp,
                    fontWeight = FontWeight.Bold,
                )
                Spacer(Modifier.height(20.dp))
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(10.dp),
                ) {
	                    BiosLikeButton(
	                        label = "CANCEL",
	                        primary = false,
	                        selected = modalSelection.value == 0,
	                        modifier = Modifier.weight(1f),
	                    ) { enterState(State.Root) }
	                    BiosLikeButton(
	                        label = "ENABLE",
	                        primary = true,
	                        selected = modalSelection.value == 1,
	                        modifier = Modifier.weight(1f),
	                    ) { enableHardcoreMode() }
                }
            }
        }
    }

    @Composable
    private fun HardcoreDisableConfirmFullscreen() {
        Box(
            modifier = Modifier
                .fillMaxSize()
                .background(Color(0xCC000000))
                // Eat taps so background controls don't trigger.
                .clickable(
                    indication = null,
                    interactionSource = remember { androidx.compose.foundation.interaction.MutableInteractionSource() },
                ) {},
            contentAlignment = Alignment.Center,
        ) {
            Column(
                modifier = Modifier
                    .width(420.dp)
                    .clip(RoundedCornerShape(2.dp))
                    .background(Color(0xFF0A0A18))
                    .border(2.dp, Color(0xFF8888AA), RoundedCornerShape(2.dp))
                    .padding(2.dp)
                    .border(1.dp, Color(0xFF333366), RoundedCornerShape(2.dp))
                    .padding(20.dp),
                horizontalAlignment = Alignment.CenterHorizontally,
            ) {
                Text(
                    "DISABLE HARDCORE MODE",
                    color = Color(0xFFFFCC66),
                    fontSize = 18.sp,
                    fontWeight = FontWeight.Bold,
                    letterSpacing = 2.sp,
                )
                Spacer(Modifier.height(2.dp))
                Box(
                    Modifier
                        .fillMaxWidth()
                        .height(1.dp)
                        .background(Color(0xFF8888AA)),
                )
                Spacer(Modifier.height(14.dp))
                Text(
                    "This drops you to Casual for the rest of this session. Achievements you unlock will no longer count as hardcore on RetroAchievements, and save states and cheats become available again.",
                    color = Color(0xFFDDDDEE),
                    fontSize = 13.sp,
                )
                Spacer(Modifier.height(8.dp))
                Text(
                    "You can't turn hardcore back on without resetting the game.",
                    color = Color(0xFFFFCC66),
                    fontSize = 13.sp,
                    fontWeight = FontWeight.Bold,
                )
                Spacer(Modifier.height(20.dp))
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(10.dp),
                ) {
                    BiosLikeButton(
                        label = "CANCEL",
                        primary = false,
                        selected = modalSelection.value == 0,
                        modifier = Modifier.weight(1f),
                    ) { enterState(State.Root) }
                    BiosLikeButton(
                        label = "DISABLE",
                        primary = true,
                        selected = modalSelection.value == 1,
                        modifier = Modifier.weight(1f),
                    ) { disableHardcoreMode() }
                }
            }
        }
    }

    @Composable
	    private fun BiosLikeButton(
	        label: String,
	        primary: Boolean,
	        selected: Boolean = false,
	        modifier: Modifier = Modifier,
	        onClick: () -> Unit,
	    ) {
        val bg = if (primary) Color(0xFF5A1A1A) else Color(0xFF1A1A2A)
        val border = if (primary) Color(0xFFFF6B6B) else Color(0xFF8888AA)
        val fg = if (primary) Color(0xFFFFCCCC) else Color(0xFFDDDDEE)
        Box(
            modifier = modifier
	                .clip(RoundedCornerShape(2.dp))
	                .background(bg)
	                .border(if (selected) 2.dp else 1.dp, if (selected) Color(0xFF3DA5FF) else border, RoundedCornerShape(2.dp))
                .clickable(onClick = onClick)
                .padding(vertical = 12.dp),
            contentAlignment = Alignment.Center,
        ) {
            Text(
                label,
                color = fg,
                fontSize = 14.sp,
                fontWeight = FontWeight.Bold,
                letterSpacing = 2.sp,
            )
        }
    }

    /** Single menu row. Compact (24dp) so all 10 root items fit without
     *  scrolling on phone landscape. Background is a left-anchored
     *  alpha gradient that "auras" the text and matches the divider
     *  fade direction. Danger variant tints text red for destructive
     *  actions (Close Game / Exit Without Saving). Optional icon
     *  rendered on the left, tinted with the same color as the label
     *  text — mirrors PCSX2 ImGui FullscreenUI's leading-icon menu
     *  rows in DrawPauseMenu. */
    @Composable
	    private fun MenuRow(
	        label: String,
	        icon: ImageVector? = null,
	        danger: Boolean = false,
	        dim: Boolean = false,
	        selected: Boolean = false,
	        onClick: () -> Unit,
	    ) {
        val textColor = when {
            dim -> Color(0xFF666666)
            danger -> Color(0xFFFF6B6B)
            else -> Color.White
        }
        val auraStart = when {
            dim -> Color.White.copy(alpha = 0.02f)
            danger -> Color(0xFFFF6B6B).copy(alpha = 0.10f)
            else -> Color.White.copy(alpha = 0.06f)
        }
        Box(
	            Modifier
	                .fillMaxWidth()
	                .height(24.dp)
	                .background(
                    Brush.horizontalGradient(
                        listOf(
                            auraStart,
                            Color.Transparent,
                        )
                    )
	                )
	                .border(
	                    if (selected) 1.dp else 0.dp,
	                    if (selected) Color(0xFF3DA5FF) else Color.Transparent,
	                    RoundedCornerShape(3.dp)
	                )
	                .clickable(onClick = onClick)
	                .padding(horizontal = 6.dp),
            contentAlignment = Alignment.CenterStart,
        ) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                if (icon != null) {
                    Image(
                        imageVector = icon,
                        contentDescription = null,
                        colorFilter = ColorFilter.tint(textColor),
                        modifier = Modifier.size(14.dp),
                    )
                    Spacer(Modifier.width(8.dp))
                } else {
                    // Reserve same space so labels align across rows w/
                    // and w/o icons (Confirm-screen Back rows skip icons).
                    Spacer(Modifier.width(22.dp))
                }
                Text(label, color = textColor, fontSize = 13.sp, fontWeight = FontWeight.SemiBold)
            }
        }
    }
}
