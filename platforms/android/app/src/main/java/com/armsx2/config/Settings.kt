package com.armsx2.config

import com.armsx2.ShaderParams
import com.armsx2.config.Settings.Companion.emitSink
import com.armsx2.config.Settings.Companion.merge
import com.armsx2.runtime.MainActivityRuntime
import kr.co.iefriends.pcsx2.NativeApp
import org.json.JSONArray
import org.json.JSONObject

/**
 * Resolved emulator config used to drive a VM launch / live-apply.
 *
 * Field naming convention: each field comments the upstream
 * `<section>/<key>` it maps to (grep against pcsx2 docs / Pcsx2Config.cpp).
 *
 * Settings are pushed via [applyTo] which calls NativeApp.setSetting per
 * field, then a single NativeApp.commitSettings to push the queued writes
 * into the running VM (or persist them for the next launch).
 *
 * Adding a new setting:
 *   1. Add a field with an upstream-matching default,
 *   2. Add a setSetting line in applyTo,
 *   3. Add the JSON mapping in toJson + fromJson + merge,
 *   4. Surface a widget in the appropriate Settings tab.
 */
/** One DEV9 internal-DNS host override: [url] resolves to [ip] when DNS mode = Internal.
 *  Used for private/fan servers (e.g. obsrv for RE Outbreak) that redirect specific hostnames. */
data class Dev9HostMapping(
    val url: String = "",
    val ip: String = "0.0.0.0",
    val enabled: Boolean = true,
)

/** `{"<preset path>": {"<parameter name>": value}}` — the wire form of
 *  [Settings.shaderChainParams]. Spelled once and shared by all four places the map has to
 *  cross a boundary (the JSON store, the per-game override diff, the override merge and the
 *  INI seed), because four hand-rolled copies of the same nesting is four chances for one
 *  of them to drift. */
private fun shaderChainParamsToJson(value: Map<String, Map<String, Float>>): JSONObject =
    JSONObject().apply {
        value.forEach { (preset, params) ->
            if (preset.isNotEmpty() && params.isNotEmpty()) {
                put(preset, JSONObject().apply {
                    params.forEach { (name, v) -> put(name, v.toDouble()) }
                })
            }
        }
    }

private fun shaderChainParamsFromJson(json: JSONObject?): Map<String, Map<String, Float>> {
    if (json == null) return emptyMap()
    return buildMap {
        json.keys().forEach { preset ->
            val params = json.optJSONObject(preset) ?: return@forEach
            val values = buildMap<String, Float> {
                params.keys().forEach { name -> put(name, params.optDouble(name, 0.0).toFloat()) }
            }
            // Drop presets whose overrides all went away rather than persisting an empty
            // object that would read back as "this preset is tweaked" forever.
            if (values.isNotEmpty()) put(preset, values)
        }
    }
}

data class Settings(
    // ---- EmuCore/Speedhacks ----
    /** EmuCore/Speedhacks/EECycleRate — −3..+3 (50%..300%). 0 = nominal. */
    val eeCycleRate: Int = 0,
    /** EmuCore/Speedhacks/EECycleSkip — 0..3. 0 = no skip. */
    val eeCycleSkip: Int = 0,
    /** EE/FPU clamp mode — 0 None / 1 Normal / 2 Extra / 3 Full (PCSX2 default Normal).
     *  Unpacks to EmuCore/CPU/Recompiler fpuOverflow/fpuExtraOverflow/fpuFullMode. */
    val eeClampMode: Int = 1,
    /** VU clamp mode — 0 None / 1 Normal / 2 Extra / 3 Extra+Sign (PCSX2 default Normal).
     *  Unpacks to vu0/vu1 Overflow/ExtraOverflow/SignOverflow. */
    val vuClampMode: Int = 1,
    /** EmuCore/Speedhacks/vuThread — Multi-Threaded VU1 (MTVU).
     *  Kept on by default for the mac ARM64 backend, but persisted normally
     *  so testers can A/B games which dislike MTVU. */
    val mtvu: Boolean = true,
    /** EmuCore/Speedhacks/vu1Instant — completes VU1 in one cycle. */
    val vu1Instant: Boolean = true,
    /** EmuCore/Speedhacks/vuFlagHack — skip VU flag computation when unread. */
    val vuFlagHack: Boolean = true,
    /** EmuCore/Speedhacks/fastCDVD — skip CDVD reads. */
    val fastCDVD: Boolean = false,
    /** EmuCore/Speedhacks/IntcStat — INTC_STAT register read hack. */
    val intcStat: Boolean = true,
    /** EmuCore/Speedhacks/WaitLoop — detect EE wait loops. */
    val waitLoop: Boolean = true,
    /** EmuCore/Speedhacks/vuNeonFusions — ARMSX2-only. Gates the arm64
     *  VU1 JIT NEON peephole fusions (MAC cluster
     *  MULAx+MADDAy+MADDAz+MADDw, OPMULA+OPMSUB cross-product). Default
     *  on — toggle off to A/B whether one of those JIT fusions is
     *  responsible for a per-game regression. */
    val vuNeonFusions: Boolean = true,
    /** EmuCore/Speedhacks/vuDeferredWrites — EXPERIMENTAL. Defers
     *  per-pair VF stores via the NEON cache; flush sites commit later.
     *  Big perf win on transform-heavy code. Known to break SH2 graphics
     *  and other games with cross-pair memory coherence assumptions. */
    val vuDeferredWrites: Boolean = false,
    /** EmuCore/Speedhacks/vuSkipStallSim — AGGRESSIVE. Skips the
     *  vu1_TestPipes_VU1 BL in the JIT — was 19-32% of total CPU on
     *  Futurama/GoW2/Ape Escape 3 per profiling. Breaks any game that
     *  relies on accurate FMAC/FDIV/EFU/IALU pipeline-stall timing. */
    val vuSkipStallSim: Boolean = false,

    // ---- EmuCore/GS — frame limiter ----
    /** EmuCore/GS/FrameLimitEnable. */
    val frameLimitEnable: Boolean = true,
    /** Framerate/NominalScalar expressed as a percent of native speed
     *  (100 = full speed ≈ 60fps NTSC / 50fps PAL). Applies when the Frame
     *  Limiter is on: lower values cap the FPS (50 ≈ 30fps), higher values
     *  fast-forward. Stored as percent; written to emucore as the 0.05..10.0
     *  float scalar. */
    val nominalSpeedPercent: Int = 100,
    /** Max presented-FPS cap, independent of [nominalSpeedPercent] and the Speed
     *  Limit %. 0 = off. When > 0 the native side caps the DISPLAY frame rate by
     *  dropping presents on the GS thread while emulation keeps running full
     *  speed — it does NOT slow the game. Adaptive: a game already at/below the
     *  target is unaffected (no over-skip). */
    val fpsLimit: Int = 0,
    /** Deprecated Android-only frame skip. Kept for JSON compatibility only. */
    val frameSkip: Int = 0,

    // ---- Audio (SPU2/Output) ----
    /** SPU2/Output/StandardVolume — output volume %, 0..200 (100 = full). */
    val audioVolume: Int = 100,
    /** SPU2/Output/OutputMuted — mute audio output. */
    val audioMuted: Boolean = false,
    /** SPU2/Output/SwapChannels — swap final stereo output L<->R (flipped-speaker
     *  devices forced into reverse-landscape, e.g. the Clamp gamepad). */
    val audioSwapChannels: Boolean = false,
    /** SPU2/Output/SyncMode — TimeStretch keeps pitch stable under load; off
     *  (Disabled) is lower CPU but drifts pitch when frame-time varies. */
    val audioTimeStretch: Boolean = true,
    /** SPU2/Output/BufferMS — audio buffer size (ms). Higher = fewer dropouts,
     *  more latency. 50 = default; raise if audio stutters on low-end devices. */
    val audioBufferMs: Int = 50,
    /** SPU2/Output/OutputLatencyMS — target output latency (ms). 20 = default. */
    val audioOutputLatencyMs: Int = 20,
    /** SPU2/Output/FastForwardVolume — output volume % while fast-forwarding. */
    val audioFastForwardVolume: Int = 100,
    /** SPU2/NeonReverbSIMD — opt-in NEON reverb FIR on ARM64. Frees CPU on
     *  CPU-bound devices; default off uses the scalar reference (unchanged
     *  audio). Applied on the next game boot/reset. */
    val spu2NeonReverb: Boolean = false,
    /** SPU2/Output/AndroidOpenSLES — opt-in legacy OpenSL ES audio path (Oboe)
     *  instead of AAudio. Slightly higher latency, but Android doesn't reclaim
     *  the idle stream, so pause/resume (and fast-forward toggling through the
     *  menu) never triggers the ~1s stream rebuild. Applies live (stream
     *  reconfigures). Default off = AAudio low-latency. */
    val audioOpenSLES: Boolean = false,
    /** SPU2/Output/LightweightMode — low-end audio lever: skip the SPU2 reverb
     *  pipeline (all echo/spatial reverb) in the mixer. Frees CPU on devices that
     *  can't keep up even with NEON reverb; default off = full reverb. Applies
     *  live (read per-sample in MixCore). */
    val spu2LightweightMix: Boolean = false,

    // ---- EmuCore — patches / cheats ----
    /** EmuCore/EnablePatches — game-compatibility patches (default on). */
    val enablePatches: Boolean = true,
    /** EmuCore/EnableCheats — PNACH cheats. */
    val enableCheats: Boolean = false,
    /** EmuCore/EnableWideScreenPatches — 16:9 widescreen patches. */
    val enableWideScreenPatches: Boolean = false,
    /** EmuCore/EnableNoInterlacingPatches — no-interlacing patches. */
    val enableNoInterlacingPatches: Boolean = false,
    /** EmuCore/EnableFastBoot — skip BIOS splash and boot straight to the game. */
    val enableFastBoot: Boolean = false,
    /** EmuCore/HostFs — host: filesystem access in the VM, for ELF/homebrew and mods
     *  (e.g. modded Persona 3 FES). Per-game capable; applies on the next game boot. */
    val hostFs: Boolean = false,
    /** EmuCore/EnableGameFixes — master switch that lets the GameDB apply each game's
     *  curated compatibility gamefixes (e.g. VuAddSubHack, SkipMPEGHack). Defaults TRUE
     *  to match upstream PCSX2 (Pcsx2Config.cpp EnableGameFixes = true) and trak's Mac:
     *  Android was the outlier defaulting it false, which silently skipped every GameDB
     *  CPU gamefix — that's what broke Valkyrie Profile 2 (needs VuAddSubHack; without it
     *  the first VU0 program diverges and the EE derails to PC=0) and made Skip MPEG inert.
     *  GameDB gamefixes are per-game curated, so on-by-default only helps compatibility. */
    val enableGameFixes: Boolean = true,
    /** EmuCore/Gamefixes/SoftwareRendererFMVHack. */
    val gamefixSoftwareRendererFmv: Boolean = false,
    /** EmuCore/Gamefixes/SkipMPEGHack. */
    val gamefixSkipMpeg: Boolean = false,
    /** EmuCore/Gamefixes/EETimingHack. */
    val gamefixEETiming: Boolean = false,
    /** EmuCore/Gamefixes/InstantDMAHack. */
    val gamefixInstantDma: Boolean = false,
    /** EmuCore/Gamefixes/BlitInternalFPSHack. */
    val gamefixBlitInternalFps: Boolean = false,
    /** EmuCore/Gamefixes/FpuMulHack — Tales of Destiny. */
    val gamefixFpuMul: Boolean = false,
    /** EmuCore/Gamefixes/OPHFlagHack — Bleach Blade Battlers. */
    val gamefixOphFlag: Boolean = false,
    /** EmuCore/Gamefixes/GIFFIFOHack — emulate the GIF FIFO (Test Drive Unlimited). */
    val gamefixGifFifo: Boolean = false,
    /** EmuCore/Gamefixes/DMABusyHack — Mana Khemia 1. */
    val gamefixDmaBusy: Boolean = false,
    /** EmuCore/Gamefixes/VIF1StallHack — delay VIF1 stalls (SOCOM 2 HUD). */
    val gamefixVif1Stall: Boolean = false,
    /** EmuCore/Gamefixes/IbitHack — Scarface, Crash Twinsanity. */
    val gamefixIbit: Boolean = false,
    /** EmuCore/Gamefixes/FullVU0SyncHack — tight VU0 sync on every COP2 op. */
    val gamefixFullVu0Sync: Boolean = false,
    /** EmuCore/Gamefixes/VuAddSubHack — Tri-Ace games. */
    val gamefixVuAddSub: Boolean = false,
    /** EmuCore/Gamefixes/VUOverflowHack — Superman Returns. */
    val gamefixVuOverflow: Boolean = false,
    /** EmuCore/Gamefixes/XgKickHack — extra XGKICK delay (Erementar Gerad). */
    val gamefixXgkick: Boolean = false,
    /** EmuCore/Gamefixes/GoemonTlbHack — preload TLB for Goemon games. Restart to apply. */
    val gamefixGoemonTlb: Boolean = false,
    /** EmuCore/Gamefixes/VUSyncHack — run microVU behind the EE (M-bit games). Restart to apply. */
    val gamefixVuSync: Boolean = false,
    /** EmuCore/GS/SkipDuplicateFrames — skip presenting unchanged frames. PCSX2 default on. */
    val skipDuplicateFrames: Boolean = true,
    /** EmuCore/CPU/FPU.Roundmode — EE FPU rounding: 0 Nearest / 1 Negative / 2 Positive
     *  / 3 Chop. PS2 EE FPU default is Chop (toward zero). */
    val eeFpuRoundMode: Int = 3,
    /** EmuCore/CPU/VU0.Roundmode — VU0 rounding: 0 Nearest / 1 Neg / 2 Pos / 3 Chop. Default Chop. */
    val vu0RoundMode: Int = 3,
    /** EmuCore/CPU/VU1.Roundmode — VU1 rounding: 0 Nearest / 1 Neg / 2 Pos / 3 Chop. Default Chop. */
    val vu1RoundMode: Int = 3,

    // ---- EmuCore/GS — display / PCRTC fixes ----
    /** EmuCore/GS/pcrtc_offsets — apply PCRTC screen offsets. PCSX2 default off. */
    val screenOffsets: Boolean = false,
    /** EmuCore/GS/pcrtc_overscan — show overscan area. PCSX2 default off. */
    val showOverscan: Boolean = false,
    /** EmuCore/GS/pcrtc_antiblur — anti-blur. PCSX2 default ON. */
    val antiBlur: Boolean = true,
    /** EmuCore/GS/disable_interlace_offset — disable interlace offset. Default off. */
    val disableInterlaceOffset: Boolean = false,
    /** EmuCore/GS/SyncToHostRefreshRate — pace emulation to the host refresh. Default off. */
    val syncToHostRefresh: Boolean = false,
    /** EmuCore/GS/DisableFramebufferFetch — disable the framebuffer-fetch path. Default off. */
    val disableFramebufferFetch: Boolean = false,
    /** EmuCore/GS/HWROV — Rasterizer Order Views (accurate blending via fragment-shader
     *  interlock; Vulkan only). Default OFF on mobile: it's a perf loss on tilers and is
     *  inert on Turnip/Adreno (no VK_EXT_fragment_shader_interlock), so on-by-default just
     *  costs frames for no gain. Upstream PCSX2 defaults it true (desktop); we override to
     *  false for Android. Users can still enable it in Renderer for benchmarking. */
    val hwRov: Boolean = false,
    /** EmuCore/GS/HWAA1 — hardware PS2 AA1 edge anti-aliasing. Default off. Applies on game restart. */
    val hwAa1: Boolean = false,
    /** EmuCore/GS/HWAccurateAlphaTest — accurate alpha test for the HW renderer (pairs with ROV). Default off. */
    val hwAat: Boolean = false,
    /** EmuCore/GS/EnableAdrenoFramebufferFetch — enable the Vulkan framebuffer-fetch
     * (ROAA) accurate-blending fast path on non-Mali (Adreno) GPUs that expose the
     * extension. Default ON so accurate blending runs in-tile (fast) instead of the
     * per-primitive barrier fallback. A few proprietary Adreno drivers show stale-ROAA
     * read artifacts — turn this off in the Renderer tab if so. Applies on game restart. */
    val adrenoFbFetch: Boolean = true,
    /** EmuCore/GS/ForceMaliFramebufferFetch — re-enable the Vulkan framebuffer-fetch
     * (ROAA) path on MediaTek Mali / Mali-G57, where it is force-disabled because those
     * drivers return zero/stale destination colour through ROAA (black or missing
     * textures). Mali exposes no hardware dual-source blend, so with fetch off the HW
     * renderer SW-blends via a per-primitive texture barrier — very slow in blend-heavy
     * games (issue #339: Shadow of the Colossus on Dimensity 8350 + Mali-G615). This lets
     * such a user test whether their driver is actually affected. Default OFF, and kept
     * deliberately separate from adrenoFbFetch (which is default-ON plus a ConfigStore
     * migration — keying off it would force fetch on for EVERY MediaTek Mali user, the
     * exact breakage this works around). Inert on other GPUs/renderers. Applies on game
     * restart. */
    val forceMaliFbFetch: Boolean = false,
    /** EmuCore/GS/AndroidUseAngleOpenGL — run the OpenGL renderer through ANGLE's
     *  GLES-on-Vulkan translation (bundled libEGL_angle.so / libGLESv2_angle.so).
     *  Useful on devices with a broken native GLES driver (e.g. some MediaTek Mali).
     *  Only takes effect when the renderer is OpenGL; MainActivityRuntime.applyAngleEnv
     *  turns it into the ARMSX2_ANGLE_EGL_LIBRARY env var that GLContextEGL reads.
     *  Applies on game restart. Default off. */
    val useAngleOpenGL: Boolean = false,
    /** EmuCore/GS/OverrideTextureBarriers — -1 Auto / 0 Off / 1 On. */
    val overrideTextureBarriers: Int = -1,
    /** EmuCore/GS/GSBackThreadMode — GV7 GS front/back thread split.
     * 0 Off (single-threaded), 1 Inline, 2 Lockstep, 3 Pipelined (fastest).
     * Defaults to Off (opt-in); a per-game override can raise it. Restart-required. */
    val gsBackThreadMode: Int = 0,
    /** EmuCore/GS/DisableVertexShaderExpand — force CPU vertex expansion. Renderer-init; restart to apply. */
    val disableVertexShaderExpand: Boolean = false,
    /** EmuCore/GS/UseBlitSwapChain — blit present model instead of flip. Renderer-init; restart to apply. */
    val useBlitSwapChain: Boolean = false,
    /** EmuCore/GS/DisableShaderCache — don't cache compiled shaders to disk. Renderer-init; restart to apply. */
    val disableShaderCache: Boolean = false,
    /** EmuCore/GS/HWAccurateAlphaTest — accurate hardware alpha test. PCSX2 default off. */
    val hwAccurateAlphaTest: Boolean = false,

    // ---- EmuCore/GS — hardware / software renderer fixes ----
    /** EmuCore/GS/UserHacks_SkipDraw_Start — first draw to skip. 0 = off. */
    val skipDrawStart: Int = 0,
    /** EmuCore/GS/UserHacks_SkipDraw_End — last draw to skip. 0 = off. */
    val skipDrawEnd: Int = 0,
    /** EmuCore/GS/HWSpinGPUForReadbacks — busy-wait the GPU on readbacks. Default off. */
    val spinGpuReadbacks: Boolean = false,
    /** EmuCore/GS/HWSpinCPUForReadbacks — busy-wait the CPU on readbacks. Default off. */
    val spinCpuReadbacks: Boolean = false,
    /** EmuCore/GS/IntegerScaling — integer pixel scaling for the presented image. Default off. */
    val integerScaling: Boolean = false,
    /** EmuCore/GS/CropLeft|Top|Right|Bottom — overscan crop in native PS2 pixels, trimmed
     *  from the presented image before aspect/integer scaling. Many PS2 titles render
     *  garbage or a black band in the overscan area that a TV would have hidden; the core
     *  has always supported this (GSRenderer.cpp) but Android never exposed it (issue #293). */
    val cropLeft: Int = 0,
    val cropTop: Int = 0,
    val cropRight: Int = 0,
    val cropBottom: Int = 0,
    /** Display zoom, 100-150% (#383). An AetherSX2-style single "zoom" slider: rather than the
     *  four fiddly per-edge crops (which distort when set unevenly), this trims all four edges by
     *  the SAME fraction, so the image scales up into the frame without changing aspect. App-side
     *  only (no native key) — it's converted to symmetric CropLeft/Top/Right/Bottom in writeIni,
     *  overriding the manual crops while > 100. */
    val displayZoom: Int = 100,
    /** EmuCore/GS/dithering_ps2 — 0 Off / 1 Scaled / 2 Unscaled / 3 Force 32bit. PCSX2 default Unscaled. */
    val dithering: Int = 2,
    /** EmuCore/GS/VsyncQueueSize — frames the GS thread may queue (0-3). PCSX2 default 2. */
    val vsyncQueueSize: Int = 2,
    /** EmuCore/GS/autoflush_sw — software-renderer auto-flush. PCSX2 default on. */
    val autoFlushSw: Boolean = true,
    /** EmuCore/GS/mipmap — software-renderer mipmapping. PCSX2 default on. */
    val mipmapSw: Boolean = true,
    /** EmuCore/GS/extrathreads — extra software-renderer threads (0-10). PCSX2 default 4. */
    val swThreads: Int = 4,
    /** EmuCore/GS/extrathreads_height — SW-renderer tile height per thread (0-8). PCSX2 default 4. Restart to apply. */
    val swThreadsHeight: Int = 4,

    /** EmuCore/GS/AspectRatio:
     *  0 Stretch · 1 Auto 4:3/3:2 · 2 4:3 · 3 16:9 · 4 10:7. */
    val aspectRatio: Int = 1,
    /** EmuCore/GS/FMVAspectRatioSwitch — aspect ratio used ONLY while an FMV/MPEG is
     *  playing (restores [aspectRatio] when it ends). 0 Off (no override) · 1 Auto
     *  4:3/3:2 · 2 4:3 · 3 16:9 · 4 10:7. Default Off. */
    val fmvAspectRatio: Int = 0,
    /** Host graphics API: "auto" / "opengl" / "vulkan" / "software". Applied via
     *  the renderer JNI helpers on (re)launch; per-game so each title can pick its
     *  own backend. Seeded from the legacy global "renderer" pref on first load. */
    val renderer: String = "auto",
    /** Internal resolution multiplier (0.25..5.0; 1.0 = native). Applied live via
     *  the GS upscale helper; per-game so each title keeps its own. Seeded from the
     *  legacy global "upscaleFloat" pref on first load. */
    val upscaleFloat: Float = 1.0f,
    /** Installed custom Vulkan GPU driver id to pin (e.g. a Turnip build). "" = system
     *  driver. Applied at (re)launch via CustomDriver.applyToNative in
     *  MainActivityRuntime.applyRendererPrefs; per-game so a title can pin the driver it
     *  needs. Seeded from the legacy global "customDriverId" pref on first load. */
    val customDriverId: String = "",
    /** Android activity screen orientation: 0 Use Device Setting · 1 Landscape · 2 Portrait
     *  · 3 Auto-Rotate. Applied via MainActivityRuntime.applyEmulationOrientation, resolved
     *  per-game at game boot (global in the library/menus). Seeded from the legacy global
     *  "ui.orientation" pref on first load. */
    val orientation: Int = 0,
    /** GitHub #375: in PORTRAIT, top-align the render (true, default) instead of vertical-
     *  centering (false), so the bottom is free for touch controls. Applied live via
     *  NativeApp.setPortraitRenderTop; only affects a portrait window. */
    val portraitRenderTop: Boolean = true,
    /** EmuCore/GS FramerateNTSC — the emulated PS2 vsync rate for NTSC games
     *  (PCSX2 default 59.94). Lowering it slows the game's target rate; raising it
     *  speeds it up. Mirrors NetherSX2's "Framerate For NTSC". */
    val framerateNtsc: Float = 59.94f,
    /** EmuCore/GS FrameratePAL — emulated PS2 vsync rate for PAL games (default 50.00). */
    val frameratePal: Float = 50.00f,
    /** EmuCore/GS/deinterlace_mode — GSInterlaceMode:
     *  0 Auto · 1 Off · 2/3 Weave · 4/5 Bob · 6/7 Blend · 8/9 Adaptive. */
    val deinterlaceMode: Int = 0,

    // ---- DEV9 — PS2 HDD / Ethernet ----
    /** DEV9/Eth/EthEnable — PS2 network adapter. */
    val dev9EthEnable: Boolean = false,
    /** DEV9/Eth/EthApi — "Sockets" is the usable Android backend. */
    val dev9EthApi: String = "Sockets",
    /** DEV9/Eth/EthDevice — "Auto" lets the sockets backend choose. */
    val dev9EthDevice: String = "Auto",
    /** DEV9/Eth/EthLogDHCP — logs DHCP packets for network debugging. */
    val dev9EthLogDhcp: Boolean = false,
    /** DEV9/Eth/EthLogDNS — logs DNS packets for network debugging. */
    val dev9EthLogDns: Boolean = false,
    /** DEV9/Eth/InterceptDHCP — use PCSX2's internal DHCP replies. */
    val dev9InterceptDhcp: Boolean = false,
    val dev9Ps2Ip: String = "0.0.0.0",
    val dev9Mask: String = "0.0.0.0",
    val dev9Gateway: String = "0.0.0.0",
    val dev9Dns1: String = "0.0.0.0",
    val dev9Dns2: String = "0.0.0.0",
    val dev9AutoMask: Boolean = true,
    val dev9AutoGateway: Boolean = true,
    val dev9ModeDns1: String = "Auto",
    val dev9ModeDns2: String = "Auto",
    /** DEV9/Eth/Hosts — hostname->IP overrides consulted by the INTERNAL DNS server
     *  (DNS mode = Internal). For private/fan servers that redirect specific hostnames. */
    val dev9EthHosts: List<Dev9HostMapping> = emptyList(),
    /** DEV9/Hdd/HddEnable — virtual PS2 HDD. */
    val dev9HddEnable: Boolean = false,
    /** DEV9/Hdd/HddFile — path/name of the virtual HDD image. */
    val dev9HddFile: String = "DEV9hdd.raw",

    // ---- MemoryCards ----
    val memoryCardSlot1Enabled: Boolean = true,
    val memoryCardSlot1Filename: String = "mcd001.ps2",
    // Per-game BIOS override (e.g. an EU disc vs a US disc wanting its region's BIOS).
    // Empty = use the global BIOS picked in the BIOS manager. Just the filename; the file
    // lives in the app-private BIOS dir like every installed BIOS. Applied at boot in
    // MainActivityRuntime.applyRendererPrefs (resolved per-game, with global fallback).
    val biosFilename: String = "",
    val memoryCardSlot2Enabled: Boolean = true,
    val memoryCardSlot2Filename: String = "mcd002.ps2",

    // ---- USB ----
    /** USB1/Type = hidkbd — attach an emulated USB HID keyboard on USB port 1.
     *  Needed by games that require a real USB keyboard (EverQuest Online
     *  Adventures, Konami-keyboard titles). A physical/Bluetooth keyboard's key
     *  events are forwarded to it (see MainActivityRuntime.dispatchKeyEvent → NativeApp.usbKeyboardKey).
     *  Default off. */
    val usbKeyboard: Boolean = false,

    // ---- EmuCore/CPU/Recompiler — recompiler enables ----
    /** EmuCore/CPU/Recompiler/EnableEE — EE (R5900) recompiler. */
    val recEE: Boolean = true,
    /** EmuCore/CPU/Recompiler/EnableIOP — IOP (R3000) recompiler. */
    val recIOP: Boolean = true,
    /** EmuCore/CPU/Recompiler/EnableVU0 — VU0 recompiler. */
    val recVU0: Boolean = true,
    /** EmuCore/CPU/Recompiler/EnableVU1 — VU1 recompiler. */
    val recVU1: Boolean = true,
    /** EmuCore/CPU/Recompiler/EnableFastmem — fastmem (page-fault backpatch
     *  signal handler). Disabling falls back to the slow VTLB read/write
     *  path on every memory op. */
    val enableFastmem: Boolean = true,

    // ---- macOS/PCSX2 ARM64 backend compatibility flags ----
    // Hidden from UI and forced on. Kept only so older JSON/INI/per-game blobs
    // with UseMac* keys still parse without losing the rest of their settings.
    /** EmuCore/CPU/Recompiler/UseMacEE — legacy, forced on. */
    val useMacEE: Boolean = true,
    /** EmuCore/CPU/Recompiler/UseMacIOP — legacy, forced on. */
    val useMacIOP: Boolean = true,
    /** EmuCore/CPU/Recompiler/UseMacVU0 — legacy, forced on. */
    val useMacVU0: Boolean = true,
    /** EmuCore/CPU/Recompiler/UseMacVU1 — legacy, forced on. */
    val useMacVU1: Boolean = true,

    // ---- microVU-style compile-time pipeline-stall folding ----
    /** EmuCore/CPU/Recompiler/Vu1InlineFmacStall — replace the per-pair
     *  `vu1_TestFMACStallReg / _Reg2` BLs (formerly 17-32% of total CPU per
     *  simpleperf) with an inline `Add VU1_CYCLE_REG, #fmac_stall`. Mirrors
     *  mac's compile-time mVUincCycles + mVUstall fold. Gated by the same
     *  `fmac_carry_safe` (ct_cycle > 3) guarantee that cross-block carry-in
     *  FMAC slots have retired at runtime. */
    val vu1InlineFmacStall: Boolean = false,
    /** EmuCore/CPU/Recompiler/Vu1CrossBlockPState — propagate predecessor's
     *  exit pipeline-state to successor block compile, so CARRY_IN_GATE_*
     *  bounds can shrink (FMAC/IALU=3, FDIV=12, EFU=54). When a predecessor
     *  links to a successor, the successor variant is specialised for that
     *  predecessor's exitState. Mirrors mac's microBlockManager pState match. */
    val vu1CrossBlockPState: Boolean = false,
    /** EmuCore/CPU/Recompiler/Vu1InlineDrainTestPipes — inline-emit the
     *  vu1_TestPipes_VU1 FMAC drain at JIT sites where the pre-walk proves
     *  FDIV/EFU/IALU are empty (skip_info[i].fmacOnlyTestPipes). Saves the BL
     *  + viCacheInvalidateAll + return overhead per call. Mac doesn't need
     *  this because it has no runtime FMAC ring — flag instances are routed
     *  at compile time. */
    val vu1InlineDrainTestPipes: Boolean = false,
    /** EmuCore/CPU/Recompiler/Vu1FmacInstanceRouting — mac-style 4-slot flag-
     *  instance routing. Repurposes VU->fmac[0..3].{mac,status,clip}flag as
     *  instance slots; skips the ring metadata Strs and the FMAC stall BLs.
     *  fmaccount stays 0 so vu1_TestPipes_VU1's FMAC drain early-exits. */
    val vu1FmacInstanceRouting: Boolean = false,

    // ---- EmuCore/GS — renderer accuracy / quality ----
    /** EmuCore/GS/hw_mipmap. */
    val hwMipmap: Boolean = true,
    /** EmuCore/GS/accurate_blending_unit — AccBlendLevel:
     *  0 Min · 1 Basic · 2 Medium · 3 High · 4 Full · 5 Maximum. */
    val accurateBlendingUnit: Int = 1,
    /** EmuCore/GS/filter — BiFiltering:
     *  0 Nearest · 1 Forced (Bilinear) · 2 PS2 · 3 Forced_But_Sprite. */
    val textureFiltering: Int = 2,
    /** EmuCore/GS/linear_present_mode — GSPostBilinearMode:
     *  0 Off (nearest) · 1 Smooth · 2 Sharp. Display output (scan-out) bilinear filter. */
    val displayBilinear: Int = 1,
    /** EmuCore/GS/texture_preloading — TexturePreloadingLevel:
     *  0 Off · 1 Partial · 2 Full. */
    val texturePreloading: Int = 2,
    /** EmuCore/GS/HWDownloadMode — GSHardwareDownloadMode:
     *  0 Accurate · 1 Force Full · 2 No Readbacks · 3 Unsync · 4 Disabled. */
    val hardwareDownloadMode: Int = 0,
    /** EmuCore/GS/TVShader — CRT / TV shader preset. */
    val tvShader: Int = 0,
    /** EmuCore/GS/ShadeBoost. */
    val shadeBoost: Boolean = false,
    val shadeBoostBrightness: Int = 50,
    val shadeBoostContrast: Int = 50,
    val shadeBoostSaturation: Int = 50,
    val shadeBoostGamma: Int = 50,
    /** EmuCore/GS/fxaa — FXAA post-process anti-aliasing. */
    val fxaa: Boolean = false,
    /** EmuCore/GS/ShaderChainEnabled + /ShaderChainPreset — RetroArch (.slangp) shader
     *  chain run at present via librashader, after ShadeBoost/FXAA. An empty preset means
     *  off regardless of the flag, and it's a no-op if this build has no librashader. */
    val shaderChainEnabled: Boolean = false,
    val shaderChainPreset: String = "",
    /** Tweaked shader parameters, as `preset path -> (parameter name -> value)`.
     *
     *  Sparse: a parameter the user hasn't touched is simply absent, and the author's own
     *  initial applies. That is what keeps this from bloating — a preset can declare ~900
     *  parameters, and storing all of them per game would dwarf the rest of the config.
     *
     *  Keyed by preset rather than holding one flat map because parameter names collide
     *  freely across packs ("gamma" means something different in every one of them), and
     *  because it lets a user flip between two tweaked presets without losing either set.
     *  No EmuCore key mirrors this — see applyTo. */
    val shaderChainParams: Map<String, Map<String, Float>> = emptyMap(),
    /** EmuCore/GS/CASMode — GSCASMode: 0 Off / 1 Sharpen Only / 2 Sharpen + Resize. */
    val casMode: Int = 0,
    /** EmuCore/GS/CASSharpness — sharpening strength 0..100 (%). */
    val casSharpness: Int = 50,
    /** EmuCore/GS/LoadTextureReplacements. */
    val loadTextureReplacements: Boolean = false,
    /** EmuCore/GS/LoadTextureReplacementsAsync. */
    val loadTextureReplacementsAsync: Boolean = true,
    /** EmuCore/GS/PrecacheTextureReplacements. */
    val precacheTextureReplacements: Boolean = false,
    /** EmuCore/GS/DumpReplaceableTextures. */
    val dumpReplaceableTextures: Boolean = false,
    /** EmuCore/GS/OsdShowTextureReplacements. */
    val osdShowTextureReplacements: Boolean = false,
    // Performance Overlay element toggles. Default true to mirror native
    // initialize(), which turns every OsdShow* bit on at first boot.
    // Disabling GPU also stops the GPU timing queries (real perf win).
    /** EmuCore/GS/OsdShowFPS. */
    val osdShowFps: Boolean = false,
    /** EmuCore/GS/OsdScale — size of on-screen messages/stats, percent (25–500, 100 = PCSX2's
     *  normal). Defaults to 65: at 100 the stats block dominates a handheld screen, and 65 matches
     *  the size NetherSX2 ships. Saves still on the old 100 default are migrated once (ConfigStore). */
    val osdScale: Int = 65,
    /** EmuCore/GS/OsdColor — OSD text colour as 0xRRGGBB. 0 = default white. */
    val osdColor: Int = 0,
    /** EmuCore/GS/VsyncEnable — sync presentation to the display refresh (less
     *  tearing/smoother, slightly higher latency). Applies on game restart. */
    val vsyncEnable: Boolean = false,
    /** EmuCore/GS/OsdShowVPS. */
    val osdShowVps: Boolean = false,
    /** EmuCore/GS/OsdShowSpeed. */
    val osdShowSpeed: Boolean = false,
    /** EmuCore/GS/OsdShowCPU. */
    val osdShowCpu: Boolean = false,
    /** EmuCore/GS/OsdShowGPU. */
    val osdShowGpu: Boolean = false,
    /** EmuCore/GS/OsdShowResolution. */
    val osdShowResolution: Boolean = false,
    /** EmuCore/GS/OsdShowGSStats. */
    val osdShowGsStats: Boolean = false,
    /** EmuCore/GS/OsdShowFrameTimes. */
    val osdShowFrameTimes: Boolean = false,
    /** EmuCore/GS/OsdShowHardwareInfo — the CPU/GPU model info line. */
    val osdShowHardwareInfo: Boolean = false,
    /** EmuCore/GS/OsdMessagesPos — transient OSD notifications (shader-compile
     *  popups, "settings applied", save-state, etc.). true = shown (TopLeft),
     *  false = hidden (None). Achievement popups are separate & unaffected. */
    val osdShowMessages: Boolean = true,
    /** EmuCore/GS/OsdShowGPUStats — GPU pipeline stats (VSI/PSI). Vulkan-only
     *  (GLES has no pipeline_statistics_query); default off since it's a niche
     *  diagnostic that adds per-frame query overhead. */
    val osdShowGpuStats: Boolean = false,
    /** EmuCore/GS/OsdShowVersion — the emulator version line. */
    val osdShowVersion: Boolean = false,
    /** EmuCore/GS/OsdShowSettings — the settings summary (bottom-left). */
    val osdShowSettings: Boolean = false,
    /** EmuCore/GS/OsdShowInputs — the control inputs (bottom-right). */
    val osdShowInputs: Boolean = false,
    /** EmuCore/GS/UserHacks_AutoFlushLevel — GSHWAutoFlushLevel:
     *  0 Disabled · 1 SpritesOnly · 2 Enabled. */
    val autoFlush: Int = 0,
    /** EmuCore/GS/UserHacks_HalfPixelOffset — GSHalfPixelOffset:
     *  0 Off · 1 Normal · 2 Special · 3 SpecialAggressive · 4 Native · 5 NativeWTexOffset. */
    val halfPixelOffset: Int = 0,
    /** EmuCore/GS/UserHacks_Limit24BitDepth — 0 Off · 1 Upper · 2 Lower. */
    val limit24BitDepth: Int = 0,
    /** EmuCore/GS/UserHacks — master hardware-fixes toggle. */
    val manualUserHacks: Boolean = false,
    /** EmuCore/GS/UserHacks_TextureInsideRt — texture inside render target. */
    val textureInsideRt: Int = 0,
    /** EmuCore/GS/UserHacks_native_scaling — upscaling fixes/native scaling. */
    val nativeScaling: Int = 0,
    /** EmuCore/GS/UserHacks_round_sprite_offset. */
    val roundSprite: Int = 0,
    /** EmuCore/GS/UserHacks_BilinearHack. */
    val bilinearUpscale: Int = 0,
    /** EmuCore/GS/UserHacks_GPUTargetCLUTMode. */
    val gpuTargetClut: Int = 0,
    /** EmuCore/GS/UserHacks_CPUSpriteRenderBW. */
    val cpuSpriteRenderBw: Int = 0,
    /** EmuCore/GS/UserHacks_CPUSpriteRenderLevel. */
    val cpuSpriteRenderLevel: Int = 0,
    // ---- Additional PCSX2 hardware / upscaling fixes (full parity) ----
    // Upscaling fixes
    /** EmuCore/GS/UserHacks_align_sprite_X — Align Sprite (fixes vertical lines on some 2D upscales). */
    val alignSprite: Boolean = false,
    /** EmuCore/GS/UserHacks_merge_pp_sprite — Merge Sprite (fixes lines between post-process sprites). */
    val mergeSprite: Boolean = false,
    /** EmuCore/GS/UserHacks_ForceEvenSpritePosition — "Wild Arms" hack; forces even sprite/texture positions. */
    val forceEvenSpritePosition: Boolean = false,
    /** EmuCore/GS/UserHacks_NativePaletteDraw — Unscaled Palette Texture Draws. */
    val unscaledPaletteDraw: Boolean = false,
    /** EmuCore/GS/UserHacks_TCOffsetX — texture-coordinate X offset, 0..10000 (= 0..10 px ×1000). */
    val textureOffsetX: Int = 0,
    /** EmuCore/GS/UserHacks_TCOffsetY — texture-coordinate Y offset, 0..10000 (= 0..10 px ×1000). */
    val textureOffsetY: Int = 0,
    // Hardware fixes
    /** EmuCore/GS/paltex — GPU Palette Conversion. */
    val gpuPaletteConversion: Boolean = false,
    /** EmuCore/GS/UserHacks_CPU_FB_Conversion — CPU Framebuffer Conversion. */
    val cpuFramebufferConversion: Boolean = false,
    /** EmuCore/GS/UserHacks_ReadTCOnClose — Read Targets When Closing. */
    val readTargetsWhenClosing: Boolean = false,
    /** EmuCore/GS/UserHacks_DisableDepthSupport — Disable Depth Emulation. */
    val disableDepthEmulation: Boolean = false,
    /** EmuCore/GS/UserHacks_DisablePartialInvalidation — Disable Partial Source Invalidation. */
    val disablePartialInvalidation: Boolean = false,
    /** EmuCore/GS/UserHacks_Disable_Safe_Features — Disable Safe Features. */
    val disableSafeFeatures: Boolean = false,
    /** EmuCore/GS/UserHacks_DisableRenderFixes — Disable Render Fixes. */
    val disableRenderFixes: Boolean = false,
    /** EmuCore/GS/preload_frame_with_gs_data — Preload Frame Data. */
    val preloadFrameData: Boolean = false,
    /** EmuCore/GS/UserHacks_EstimateTextureRegion — Estimate Texture Region. */
    val estimateTextureRegion: Boolean = false,
    /** EmuCore/GS/UserHacks_DrawBuffering — buffer draws (UserHack). */
    val drawBuffering: Boolean = false,
    /** EmuCore/GS/UserHacks_CPUCLUTRender — CPU CLUT Render: 0 Off · 1 Normal · 2 Aggressive. */
    val cpuClutRender: Int = 0,
    /** EmuCore/GS/TriFilter — TriFiltering: -1 Auto · 0 Off · 1 PS2 · 2 Forced. */
    val triFilter: Int = -1,
    /** EmuCore/GS/MaxAnisotropy — 0 Off, else 2/4/8/16. */
    val maxAnisotropy: Int = 0,
    /** EmuCore/GS/AndroidGpuProfileOverride — 0 Auto · 1 Mali · 2 Adreno · 3 PowerVR · 4 Xclipse.
     *  Stringified to "auto"/"mali"/"adreno"/"powervr"/"xclipse" when written to emucore.
     *  Picked up in GSDeviceOGL::CheckFeatures at device init; requires
     *  a renderer restart to take effect. */
    val gpuProfile: Int = 0,
) {
    /** Routes a persisted-key write to the native base layer, or to
     *  [emitSink] when a per-game INI export is capturing the key set (see
     *  [writeGameSettingsIni]). Replaces the direct NativeApp.setSetting calls
     *  so applyTo/writeGsToNative can be reused as the single source of the
     *  field→EmuCore-key mapping for the export — no duplicated key list. */
    private fun put(section: String, key: String, type: String, value: String) {
        val sink = emitSink
        if (sink != null) sink(section, key, type, value)
        else NativeApp.setSetting(section, key, type, value)
    }

    /** Push every field into emucore via NativeApp.setSetting + commit. */
    fun applyTo() {
        // Speedhacks
        put("EmuCore/Speedhacks", "EECycleRate", "int", eeCycleRate.toString())
        put("EmuCore/Speedhacks", "EECycleSkip", "int", eeCycleSkip.toString())
        // EE/FPU + VU clamping (recompiler accuracy). Each mode unpacks to the
        // PCSX2 bit flags below; both VUs get the same mode. Needs a recompiler
        // reset (commitSettings / game restart) to take effect.
        put("EmuCore/CPU/Recompiler", "fpuOverflow", "bool", (eeClampMode >= 1).toString())
        put("EmuCore/CPU/Recompiler", "fpuExtraOverflow", "bool", (eeClampMode >= 2).toString())
        put("EmuCore/CPU/Recompiler", "fpuFullMode", "bool", (eeClampMode >= 3).toString())
        for (vu in arrayOf("vu0", "vu1")) {
            put("EmuCore/CPU/Recompiler", "${vu}Overflow", "bool", (vuClampMode >= 1).toString())
            put("EmuCore/CPU/Recompiler", "${vu}ExtraOverflow", "bool", (vuClampMode >= 2).toString())
            put("EmuCore/CPU/Recompiler", "${vu}SignOverflow", "bool", (vuClampMode >= 3).toString())
        }
        put("EmuCore/Speedhacks", "vuThread", "bool", mtvu.toString())
        put("EmuCore/Speedhacks", "vu1Instant", "bool", vu1Instant.toString())
        put("EmuCore/Speedhacks", "vuFlagHack", "bool", vuFlagHack.toString())
        put("EmuCore/Speedhacks", "fastCDVD", "bool", fastCDVD.toString())
        put("EmuCore/Speedhacks", "IntcStat", "bool", intcStat.toString())
        put("EmuCore/Speedhacks", "WaitLoop", "bool", waitLoop.toString())
        put("EmuCore/Speedhacks", "vuNeonFusions", "bool", vuNeonFusions.toString())
        put("EmuCore/Speedhacks", "vuDeferredWrites", "bool", vuDeferredWrites.toString())
        put("EmuCore/Speedhacks", "vuSkipStallSim", "bool", vuSkipStallSim.toString())
        // GS frame limit. The setting key is persisted (read by runVMThread
        // after Initialize so cold starts honor the preference) AND the live
        // limiter mode is poked via speedhackLimitermode so toggling in-game
        // takes effect immediately. 0 = Nominal (capped at native rate),
        // 3 = Unlimited.
        put("EmuCore/GS", "FrameLimitEnable", "bool", frameLimitEnable.toString())
        // Preserve an active fast-forward / slow-down latch, exactly as the in-game overlay's
        // own frame-limit path does (MainActivityRuntime). Forcing 0/3 unconditionally here
        // clobbered Turbo on ANY settings apply while fast-forward was engaged — and since
        // fastForwardToggleActive stayed true, the UI kept reporting "Fast Forward ON" with
        // the emulator back at nominal speed. Frame-limit-off masked the bug: that path IS
        // mode 3, so re-asserting it changed nothing, which is why users saw "frame limit off
        // fast-forwards but fast-forward doesn't".
        if (emitSink == null) {
            NativeApp.speedhackLimitermode(
                when {
                    MainActivityRuntime.fastForwardToggleActive -> MainActivityRuntime.ffLimiterMode()
                    MainActivityRuntime.slowDownToggleActive -> 2
                    else -> if (frameLimitEnable) 0 else 3
                }
            )
        }
        // Framerate/NominalScalar — custom speed / FPS cap as a fraction of
        // native. commitSettings → ApplySettings → CheckForEmulationSpeedConfigChanges
        // → UpdateTargetSpeed picks this up live. Clamp mirrors emucore's
        // EmulationSpeedOptions::SanityCheck (0.05..10.0).
        put("Framerate", "NominalScalar", "float",
            (nominalSpeedPercent.coerceIn(10, 1000) / 100f).toString())
        // Live-apply: the setSetting above only persists; the running frame
        // pacer needs a direct re-pace (mirrors speedhackLimitermode).
        if (emitSink == null) NativeApp.setNominalSpeed(nominalSpeedPercent.coerceIn(10, 1000))
        // Max presented-FPS cap — independent of the Speed Limit % above. Caps
        // the display rate by dropping presents on the GS thread (emulation keeps
        // full speed, NominalScalar untouched); 0 = off. See GSRenderer::VSync.
        if (emitSink == null) NativeApp.setFpsCap(fpsLimit.coerceIn(0, 1000))
        // Manual frameskip (0..5) — present 1 of every (N+1) frames. Held as a
        // GS-thread global, applied live; no persisted EmuCore key needed.
        if (emitSink == null) NativeApp.setFrameSkip(frameSkip.coerceIn(0, 5))
        if (emitSink == null) NativeApp.setPortraitRenderTop(portraitRenderTop)
        // Audio (SPU2). Volume/mute are live native setters; the rest are written
        // to the base layer and applied on commit (SPU2 stream reconfigure).
        if (emitSink == null) NativeApp.setAudioVolume(audioVolume.coerceIn(0, 200))
        if (emitSink == null) NativeApp.setAudioMuted(audioMuted)
        if (emitSink == null) NativeApp.setAudioSwapChannels(audioSwapChannels)
        put("SPU2/Output", "SyncMode", "string", if (audioTimeStretch) "TimeStretch" else "Disabled")
        put("SPU2/Output", "BufferMS", "int", audioBufferMs.coerceIn(10, 200).toString())
        put("SPU2/Output", "OutputLatencyMS", "int", audioOutputLatencyMs.coerceIn(5, 200).toString())
        put("SPU2/Output", "FastForwardVolume", "int", audioFastForwardVolume.coerceIn(0, 200).toString())
        // Opt-in NEON reverb FIR (ARM64). Read by SPU2::InternalReset on the
        // next game boot; default off = scalar reference (unchanged audio).
        put("SPU2", "NeonReverbSIMD", "bool", spu2NeonReverb.toString())
        // Opt-in OpenSL ES output (Oboe). Lives in the SPU2/Output StreamParameters,
        // so ApplySettings → CheckForConfigChanges recreates the stream on toggle.
        put("SPU2/Output", "AndroidOpenSLES", "bool", audioOpenSLES.toString())
        // Lightweight mix (skip reverb) — read live in MixCore via EmuConfig.SPU2.
        put("SPU2/Output", "LightweightMode", "bool", spu2LightweightMix.toString())
        // Patches / cheats (EmuCore). Reloaded by ApplySettings →
        // CheckForPatchConfigChanges; widescreen/no-interlacing take effect on
        // the next boot for most games.
        put("EmuCore", "EnablePatches", "bool", enablePatches.toString())
        put("EmuCore", "EnableCheats", "bool", enableCheats.toString())
        put("EmuCore", "EnableWideScreenPatches", "bool", enableWideScreenPatches.toString())
        put("EmuCore", "EnableNoInterlacingPatches", "bool", enableNoInterlacingPatches.toString())
        put("EmuCore", "EnableFastBoot", "bool", enableFastBoot.toString())
        put("EmuCore", "HostFs", "bool", hostFs.toString())
        put("EmuCore", "EnableGameFixes", "bool", enableGameFixes.toString())
        put("EmuCore/Gamefixes", "SoftwareRendererFMVHack", "bool", gamefixSoftwareRendererFmv.toString())
        put("EmuCore/Gamefixes", "SkipMPEGHack", "bool", gamefixSkipMpeg.toString())
        put("EmuCore/Gamefixes", "EETimingHack", "bool", gamefixEETiming.toString())
        put("EmuCore/Gamefixes", "InstantDMAHack", "bool", gamefixInstantDma.toString())
        put("EmuCore/Gamefixes", "BlitInternalFPSHack", "bool", gamefixBlitInternalFps.toString())
        put("EmuCore/Gamefixes", "FpuMulHack", "bool", gamefixFpuMul.toString())
        put("EmuCore/Gamefixes", "OPHFlagHack", "bool", gamefixOphFlag.toString())
        put("EmuCore/Gamefixes", "GIFFIFOHack", "bool", gamefixGifFifo.toString())
        put("EmuCore/Gamefixes", "DMABusyHack", "bool", gamefixDmaBusy.toString())
        put("EmuCore/Gamefixes", "VIF1StallHack", "bool", gamefixVif1Stall.toString())
        put("EmuCore/Gamefixes", "IbitHack", "bool", gamefixIbit.toString())
        put("EmuCore/Gamefixes", "FullVU0SyncHack", "bool", gamefixFullVu0Sync.toString())
        put("EmuCore/Gamefixes", "VuAddSubHack", "bool", gamefixVuAddSub.toString())
        put("EmuCore/Gamefixes", "VUOverflowHack", "bool", gamefixVuOverflow.toString())
        put("EmuCore/Gamefixes", "XgKickHack", "bool", gamefixXgkick.toString())
        put("EmuCore/Gamefixes", "GoemonTlbHack", "bool", gamefixGoemonTlb.toString())
        put("EmuCore/Gamefixes", "VUSyncHack", "bool", gamefixVuSync.toString())
        put("EmuCore/GS", "SkipDuplicateFrames", "bool", skipDuplicateFrames.toString())
        put("EmuCore/CPU", "FPU.Roundmode", "int", eeFpuRoundMode.coerceIn(0, 3).toString())
        put("EmuCore/CPU", "VU0.Roundmode", "int", vu0RoundMode.coerceIn(0, 3).toString())
        put("EmuCore/CPU", "VU1.Roundmode", "int", vu1RoundMode.coerceIn(0, 3).toString())
        // Display + GS renderer + hardware/upscaling-fix keys are all written
        // together in writeGsToNative() below (shared with applyGsLive()).
        // DEV9. Networking/HDD are initialized with the VM, so changes
        // made from the in-game overlay are persisted for the next boot.
        put("DEV9/Eth", "EthEnable", "bool", dev9EthEnable.toString())
        put("DEV9/Eth", "EthApi", "string", dev9EthApi)
        put("DEV9/Eth", "EthDevice", "string", dev9EthDevice.ifEmpty { "Auto" })
        put("DEV9/Eth", "EthLogDHCP", "bool", dev9EthLogDhcp.toString())
        put("DEV9/Eth", "EthLogDNS", "bool", dev9EthLogDns.toString())
        put("DEV9/Eth", "InterceptDHCP", "bool", dev9InterceptDhcp.toString())
        put("DEV9/Eth", "PS2IP", "string", dev9Ps2Ip.ifEmpty { "0.0.0.0" })
        put("DEV9/Eth", "Mask", "string", dev9Mask.ifEmpty { "0.0.0.0" })
        put("DEV9/Eth", "Gateway", "string", dev9Gateway.ifEmpty { "0.0.0.0" })
        put("DEV9/Eth", "DNS1", "string", dev9Dns1.ifEmpty { "0.0.0.0" })
        put("DEV9/Eth", "DNS2", "string", dev9Dns2.ifEmpty { "0.0.0.0" })
        put("DEV9/Eth", "AutoMask", "bool", dev9AutoMask.toString())
        put("DEV9/Eth", "AutoGateway", "bool", dev9AutoGateway.toString())
        put("DEV9/Eth", "ModeDNS1", "string", dev9ModeDns1.ifEmpty { "Auto" })
        put("DEV9/Eth", "ModeDNS2", "string", dev9ModeDns2.ifEmpty { "Auto" })
        // Internal-DNS host overrides. Count gates how many Host{i} sections the core reads.
        put("DEV9/Eth/Hosts", "Count", "int", dev9EthHosts.size.toString())
        dev9EthHosts.forEachIndexed { i, h ->
            put("DEV9/Eth/Hosts/Host$i", "Url", "string", h.url)
            put("DEV9/Eth/Hosts/Host$i", "Desc", "string", "ARMSX2")
            put("DEV9/Eth/Hosts/Host$i", "Address", "string", h.ip.ifEmpty { "0.0.0.0" })
            put("DEV9/Eth/Hosts/Host$i", "Enabled", "bool", h.enabled.toString())
        }
        put("DEV9/Hdd", "HddEnable", "bool", dev9HddEnable.toString())
        put("DEV9/Hdd", "HddFile", "string", dev9HddFile.ifEmpty { "DEV9hdd.raw" })
        put("MemoryCards", "Slot1_Enable", "bool", memoryCardSlot1Enabled.toString())
        put("MemoryCards", "Slot1_Filename", "string", memoryCardSlot1Filename.ifEmpty { "mcd001.ps2" })
        put("MemoryCards", "Slot2_Enable", "bool", memoryCardSlot2Enabled.toString())
        put("MemoryCards", "Slot2_Filename", "string", memoryCardSlot2Filename.ifEmpty { "mcd002.ps2" })
        // USB keyboard (#254). Persist [USB1] Type so USBOptions::LoadSave attaches
        // the emulated HID keyboard on the next boot (or ApplySettings). The live
        // attach/detach on a running VM is done via NativeApp.usbSetKeyboardEnabled
        // below (CheckForConfigChanges recreates the device), since a plain
        // setSetting write doesn't reattach USB devices on its own.
        put("USB1", "Type", "string", if (usbKeyboard) "hidkbd" else "None")
        // Recompiler enables. Picked up by VMManager::ApplySettings →
        // SysCpuProviderPack rebind. Toggling these on a running VM swaps
        // the dispatch pointer; existing JIT block caches are flushed by
        // ApplySettings's CpusChanged path.
        put("EmuCore/CPU/Recompiler", "EnableEE", "bool", recEE.toString())
        put("EmuCore/CPU/Recompiler", "EnableIOP", "bool", recIOP.toString())
        put("EmuCore/CPU/Recompiler", "EnableVU0", "bool", recVU0.toString())
        put("EmuCore/CPU/Recompiler", "EnableVU1", "bool", recVU1.toString())
        put("EmuCore/CPU/Recompiler", "EnableFastmem", "bool", enableFastmem.toString())
        // Force the single macOS/PCSX2 ARM64 backend. VMManager also ignores
        // stale UseMac* values, but writing true cleans old persisted settings.
        put("EmuCore/CPU/Recompiler", "UseMacEE", "bool", "true")
        put("EmuCore/CPU/Recompiler", "UseMacIOP", "bool", "true")
        put("EmuCore/CPU/Recompiler", "UseMacVU0", "bool", "true")
        put("EmuCore/CPU/Recompiler", "UseMacVU1", "bool", "true")
        put("EmuCore/CPU/Recompiler", "Vu1InlineFmacStall", "bool", vu1InlineFmacStall.toString())
        put("EmuCore/CPU/Recompiler", "Vu1CrossBlockPState", "bool", vu1CrossBlockPState.toString())
        put("EmuCore/CPU/Recompiler", "Vu1InlineDrainTestPipes", "bool", vu1InlineDrainTestPipes.toString())
        put("EmuCore/CPU/Recompiler", "Vu1FmacInstanceRouting", "bool", vu1FmacInstanceRouting.toString())
        writeGsToNative()
        // Per-game INI export is capturing the key set only — writeGsToNative()
        // above was the last persisted emit, so stop before the live pokes /
        // commit (they'd re-poke the VM and double-park it; the export must not).
        if (emitSink != null) return
        // Live convenience pokes. Harmless when the GS is closed; commitSettings()
        // below performs the authoritative apply for a cold start / restart.
        NativeApp.setAspectRatio(aspectRatio.coerceIn(0, 4))
        NativeApp.setFmvAspectRatio(fmvAspectRatio.coerceIn(0, 4))
        NativeApp.renderTvShader(tvShader.coerceIn(0, 7))
        NativeApp.renderShadeBoost(
            shadeBoost,
            shadeBoostBrightness.coerceIn(1, 100),
            shadeBoostContrast.coerceIn(1, 100),
            shadeBoostSaturation.coerceIn(1, 100),
            shadeBoostGamma.coerceIn(1, 100),
        )
        NativeApp.osdShowFPS(osdShowFps)
        NativeApp.osdSetScale(osdScale.toFloat())
        NativeApp.osdSetColor(osdColor)
        NativeApp.osdShowVPS(osdShowVps)
        NativeApp.osdShowSpeed(osdShowSpeed)
        NativeApp.osdShowCPU(osdShowCpu)
        NativeApp.osdShowGPU(osdShowGpu)
        NativeApp.osdShowResolution(osdShowResolution)
        NativeApp.osdShowGSStats(osdShowGsStats)
        NativeApp.osdShowFrameTimes(osdShowFrameTimes)
        NativeApp.osdShowHardwareInfo(osdShowHardwareInfo)
        NativeApp.osdShowMessages(osdShowMessages)
        NativeApp.osdShowGpuStats(osdShowGpuStats)
        NativeApp.osdShowVersion(osdShowVersion)
        NativeApp.osdShowSettings(osdShowSettings)
        NativeApp.osdShowInputs(osdShowInputs)
        // USB keyboard (#254): live attach/detach on the running VM. A plain
        // setSetting("USB1","Type",...) write is persisted but doesn't reattach
        // USB devices, so drive the device (re)creation explicitly. No-op before
        // the VM exists — the persisted Type above handles the cold boot.
        NativeApp.usbSetKeyboardEnabled(0, usbKeyboard)
        NativeApp.commitSettings()
    }

    /** Reverse of [applyTo]: rebuild a Settings from a parsed PCSX2-Android.ini map
     *  (keys "Section/Key" -> raw string value). Any key absent from the map keeps this
     *  Settings' current value (call on Settings() to default-fill). Used to recover an
     *  existing native config when the new UI has no stored config.global (fresh install
     *  over a reused data folder). Mirror applyTo's field->(section,key) mapping EXACTLY.
     *
     *  Only keys applyTo/writeGsToNative actually persist via [put] are inverted here.
     *  Live-only pokes (fpsLimit, frameSkip, audioVolume/Muted/SwapChannels) and the
     *  launch-time renderer/upscaleFloat helpers write no base-layer key, so those fields
     *  keep their current value. UseMac* are legacy/forced-on (applyTo always writes
     *  "true"), so they are forced true here to match fromJson. */
    fun readFromIni(ini: Map<String, String>): Settings {
        // Typed lookups: null when the key is absent (or unparseable) so callers
        // fall back to `this.<field>` via ?:.
        fun boolAt(key: String): Boolean? = ini[key]?.let { it == "true" || it == "1" }
        fun intAt(key: String): Int? = ini[key]?.toIntOrNull()
        fun floatAt(key: String): Float? = ini[key]?.toFloatOrNull()
        fun strAt(key: String): String? = ini[key]

        // EE/FPU clamp (0 None / 1 Normal / 2 Extra / 3 Full) is packed by applyTo into
        // three cumulative bool keys (fpuOverflow>=1, fpuExtraOverflow>=2, fpuFullMode>=3).
        val eeClamp = run {
            val fo = boolAt("EmuCore/CPU/Recompiler/fpuOverflow")
            val fe = boolAt("EmuCore/CPU/Recompiler/fpuExtraOverflow")
            val ff = boolAt("EmuCore/CPU/Recompiler/fpuFullMode")
            if (fo == null && fe == null && ff == null) this.eeClampMode
            else if (ff == true) 3 else if (fe == true) 2 else if (fo == true) 1 else 0
        }
        // VU clamp (0 None / 1 Normal / 2 Extra / 3 Extra+Sign) — same packing on vu0*
        // (applyTo writes vu0 and vu1 identically, so reading vu0 recovers the mode).
        val vuClamp = run {
            val o = boolAt("EmuCore/CPU/Recompiler/vu0Overflow")
            val e = boolAt("EmuCore/CPU/Recompiler/vu0ExtraOverflow")
            val sgn = boolAt("EmuCore/CPU/Recompiler/vu0SignOverflow")
            if (o == null && e == null && sgn == null) this.vuClampMode
            else if (sgn == true) 3 else if (e == true) 2 else if (o == true) 1 else 0
        }

        // renderer + upscale aren't written by applyTo's put() — the core / renderUpscalemultiplier
        // persist them to the base layer directly — so recover them from the native keys.
        // GSRendererType: Auto=-1, OGL=12, SW=13, VK=14.
        val recoveredRenderer = when (intAt("EmuCore/GS/Renderer")) {
            -1 -> "auto"
            12 -> "opengl"
            13 -> "software"
            14 -> "vulkan"
            else -> this.renderer
        }

        return this.copy(
            // ---- Renderer + upscale (base-layer keys, not applyTo put()) ----
            renderer = recoveredRenderer,
            upscaleFloat = floatAt("EmuCore/GS/upscale_multiplier") ?: this.upscaleFloat,
            // ---- EmuCore/Speedhacks ----
            eeCycleRate = intAt("EmuCore/Speedhacks/EECycleRate") ?: this.eeCycleRate,
            eeCycleSkip = intAt("EmuCore/Speedhacks/EECycleSkip") ?: this.eeCycleSkip,
            eeClampMode = eeClamp,
            vuClampMode = vuClamp,
            mtvu = boolAt("EmuCore/Speedhacks/vuThread") ?: this.mtvu,
            vu1Instant = boolAt("EmuCore/Speedhacks/vu1Instant") ?: this.vu1Instant,
            vuFlagHack = boolAt("EmuCore/Speedhacks/vuFlagHack") ?: this.vuFlagHack,
            fastCDVD = boolAt("EmuCore/Speedhacks/fastCDVD") ?: this.fastCDVD,
            intcStat = boolAt("EmuCore/Speedhacks/IntcStat") ?: this.intcStat,
            waitLoop = boolAt("EmuCore/Speedhacks/WaitLoop") ?: this.waitLoop,
            vuNeonFusions = boolAt("EmuCore/Speedhacks/vuNeonFusions") ?: this.vuNeonFusions,
            vuDeferredWrites = boolAt("EmuCore/Speedhacks/vuDeferredWrites") ?: this.vuDeferredWrites,
            vuSkipStallSim = boolAt("EmuCore/Speedhacks/vuSkipStallSim") ?: this.vuSkipStallSim,
            // ---- Frame limiter (nominalSpeedPercent stored as the 0.10..10.0 scalar) ----
            frameLimitEnable = boolAt("EmuCore/GS/FrameLimitEnable") ?: this.frameLimitEnable,
            nominalSpeedPercent = floatAt("Framerate/NominalScalar")?.let { Math.round(it * 100f) }
                ?: this.nominalSpeedPercent,
            // ---- Audio (SPU2/Output) — SyncMode is TimeStretch/Disabled ----
            audioTimeStretch = strAt("SPU2/Output/SyncMode")?.let { it == "TimeStretch" } ?: this.audioTimeStretch,
            audioBufferMs = intAt("SPU2/Output/BufferMS") ?: this.audioBufferMs,
            audioOutputLatencyMs = intAt("SPU2/Output/OutputLatencyMS") ?: this.audioOutputLatencyMs,
            audioFastForwardVolume = intAt("SPU2/Output/FastForwardVolume") ?: this.audioFastForwardVolume,
            spu2NeonReverb = boolAt("SPU2/NeonReverbSIMD") ?: this.spu2NeonReverb,
            audioOpenSLES = boolAt("SPU2/Output/AndroidOpenSLES") ?: this.audioOpenSLES,
            spu2LightweightMix = boolAt("SPU2/Output/LightweightMode") ?: this.spu2LightweightMix,
            // ---- EmuCore patches / cheats ----
            enablePatches = boolAt("EmuCore/EnablePatches") ?: this.enablePatches,
            enableCheats = boolAt("EmuCore/EnableCheats") ?: this.enableCheats,
            enableWideScreenPatches = boolAt("EmuCore/EnableWideScreenPatches") ?: this.enableWideScreenPatches,
            enableNoInterlacingPatches = boolAt("EmuCore/EnableNoInterlacingPatches") ?: this.enableNoInterlacingPatches,
            enableFastBoot = boolAt("EmuCore/EnableFastBoot") ?: this.enableFastBoot,
            hostFs = boolAt("EmuCore/HostFs") ?: this.hostFs,
            enableGameFixes = boolAt("EmuCore/EnableGameFixes") ?: this.enableGameFixes,
            // ---- EmuCore/Gamefixes ----
            gamefixSoftwareRendererFmv = boolAt("EmuCore/Gamefixes/SoftwareRendererFMVHack") ?: this.gamefixSoftwareRendererFmv,
            gamefixSkipMpeg = boolAt("EmuCore/Gamefixes/SkipMPEGHack") ?: this.gamefixSkipMpeg,
            gamefixEETiming = boolAt("EmuCore/Gamefixes/EETimingHack") ?: this.gamefixEETiming,
            gamefixInstantDma = boolAt("EmuCore/Gamefixes/InstantDMAHack") ?: this.gamefixInstantDma,
            gamefixBlitInternalFps = boolAt("EmuCore/Gamefixes/BlitInternalFPSHack") ?: this.gamefixBlitInternalFps,
            gamefixFpuMul = boolAt("EmuCore/Gamefixes/FpuMulHack") ?: this.gamefixFpuMul,
            gamefixOphFlag = boolAt("EmuCore/Gamefixes/OPHFlagHack") ?: this.gamefixOphFlag,
            gamefixGifFifo = boolAt("EmuCore/Gamefixes/GIFFIFOHack") ?: this.gamefixGifFifo,
            gamefixDmaBusy = boolAt("EmuCore/Gamefixes/DMABusyHack") ?: this.gamefixDmaBusy,
            gamefixVif1Stall = boolAt("EmuCore/Gamefixes/VIF1StallHack") ?: this.gamefixVif1Stall,
            gamefixIbit = boolAt("EmuCore/Gamefixes/IbitHack") ?: this.gamefixIbit,
            gamefixFullVu0Sync = boolAt("EmuCore/Gamefixes/FullVU0SyncHack") ?: this.gamefixFullVu0Sync,
            gamefixVuAddSub = boolAt("EmuCore/Gamefixes/VuAddSubHack") ?: this.gamefixVuAddSub,
            gamefixVuOverflow = boolAt("EmuCore/Gamefixes/VUOverflowHack") ?: this.gamefixVuOverflow,
            gamefixXgkick = boolAt("EmuCore/Gamefixes/XgKickHack") ?: this.gamefixXgkick,
            gamefixGoemonTlb = boolAt("EmuCore/Gamefixes/GoemonTlbHack") ?: this.gamefixGoemonTlb,
            gamefixVuSync = boolAt("EmuCore/Gamefixes/VUSyncHack") ?: this.gamefixVuSync,
            skipDuplicateFrames = boolAt("EmuCore/GS/SkipDuplicateFrames") ?: this.skipDuplicateFrames,
            eeFpuRoundMode = intAt("EmuCore/CPU/FPU.Roundmode") ?: this.eeFpuRoundMode,
            vu0RoundMode = intAt("EmuCore/CPU/VU0.Roundmode") ?: this.vu0RoundMode,
            vu1RoundMode = intAt("EmuCore/CPU/VU1.Roundmode") ?: this.vu1RoundMode,
            // ---- DEV9 — Ethernet / HDD ----
            dev9EthEnable = boolAt("DEV9/Eth/EthEnable") ?: this.dev9EthEnable,
            dev9EthApi = strAt("DEV9/Eth/EthApi") ?: this.dev9EthApi,
            dev9EthDevice = strAt("DEV9/Eth/EthDevice") ?: this.dev9EthDevice,
            dev9EthLogDhcp = boolAt("DEV9/Eth/EthLogDHCP") ?: this.dev9EthLogDhcp,
            dev9EthLogDns = boolAt("DEV9/Eth/EthLogDNS") ?: this.dev9EthLogDns,
            dev9InterceptDhcp = boolAt("DEV9/Eth/InterceptDHCP") ?: this.dev9InterceptDhcp,
            dev9Ps2Ip = strAt("DEV9/Eth/PS2IP") ?: this.dev9Ps2Ip,
            dev9Mask = strAt("DEV9/Eth/Mask") ?: this.dev9Mask,
            dev9Gateway = strAt("DEV9/Eth/Gateway") ?: this.dev9Gateway,
            dev9Dns1 = strAt("DEV9/Eth/DNS1") ?: this.dev9Dns1,
            dev9Dns2 = strAt("DEV9/Eth/DNS2") ?: this.dev9Dns2,
            dev9AutoMask = boolAt("DEV9/Eth/AutoMask") ?: this.dev9AutoMask,
            dev9AutoGateway = boolAt("DEV9/Eth/AutoGateway") ?: this.dev9AutoGateway,
            dev9ModeDns1 = strAt("DEV9/Eth/ModeDNS1") ?: this.dev9ModeDns1,
            dev9ModeDns2 = strAt("DEV9/Eth/ModeDNS2") ?: this.dev9ModeDns2,
            // Internal-DNS host overrides — Count gates Host{i} sections (Desc is ignored).
            dev9EthHosts = run {
                val count = intAt("DEV9/Eth/Hosts/Count") ?: return@run this.dev9EthHosts
                (0 until count).mapNotNull { idx ->
                    val url = ini["DEV9/Eth/Hosts/Host$idx/Url"] ?: return@mapNotNull null
                    Dev9HostMapping(
                        url = url,
                        ip = (ini["DEV9/Eth/Hosts/Host$idx/Address"] ?: "0.0.0.0").ifEmpty { "0.0.0.0" },
                        enabled = boolAt("DEV9/Eth/Hosts/Host$idx/Enabled") ?: true,
                    )
                }.filter { it.url.isNotBlank() }
            },
            dev9HddEnable = boolAt("DEV9/Hdd/HddEnable") ?: this.dev9HddEnable,
            dev9HddFile = strAt("DEV9/Hdd/HddFile") ?: this.dev9HddFile,
            // ---- MemoryCards ----
            memoryCardSlot1Enabled = boolAt("MemoryCards/Slot1_Enable") ?: this.memoryCardSlot1Enabled,
            memoryCardSlot1Filename = strAt("MemoryCards/Slot1_Filename") ?: this.memoryCardSlot1Filename,
            memoryCardSlot2Enabled = boolAt("MemoryCards/Slot2_Enable") ?: this.memoryCardSlot2Enabled,
            memoryCardSlot2Filename = strAt("MemoryCards/Slot2_Filename") ?: this.memoryCardSlot2Filename,
            // ---- USB keyboard (USB1/Type = hidkbd/None) ----
            usbKeyboard = strAt("USB1/Type")?.let { it == "hidkbd" } ?: this.usbKeyboard,
            // ---- EmuCore/CPU/Recompiler enables ----
            recEE = boolAt("EmuCore/CPU/Recompiler/EnableEE") ?: this.recEE,
            recIOP = boolAt("EmuCore/CPU/Recompiler/EnableIOP") ?: this.recIOP,
            recVU0 = boolAt("EmuCore/CPU/Recompiler/EnableVU0") ?: this.recVU0,
            recVU1 = boolAt("EmuCore/CPU/Recompiler/EnableVU1") ?: this.recVU1,
            enableFastmem = boolAt("EmuCore/CPU/Recompiler/EnableFastmem") ?: this.enableFastmem,
            // Legacy/forced-on ARM64 backend flags — always "true" in the INI (mirror fromJson).
            useMacEE = true,
            useMacIOP = true,
            useMacVU0 = true,
            useMacVU1 = true,
            vu1InlineFmacStall = boolAt("EmuCore/CPU/Recompiler/Vu1InlineFmacStall") ?: this.vu1InlineFmacStall,
            vu1CrossBlockPState = boolAt("EmuCore/CPU/Recompiler/Vu1CrossBlockPState") ?: this.vu1CrossBlockPState,
            vu1InlineDrainTestPipes = boolAt("EmuCore/CPU/Recompiler/Vu1InlineDrainTestPipes") ?: this.vu1InlineDrainTestPipes,
            vu1FmacInstanceRouting = boolAt("EmuCore/CPU/Recompiler/Vu1FmacInstanceRouting") ?: this.vu1FmacInstanceRouting,
            // ---- EmuCore/GS (writeGsToNative). Aspect/FMV/gpuProfile stored as names. ----
            aspectRatio = when (strAt("EmuCore/GS/AspectRatio")) {
                "Stretch" -> 0
                "Auto 4:3/3:2" -> 1
                "4:3" -> 2
                "16:9" -> 3
                "10:7" -> 4
                else -> this.aspectRatio
            },
            fmvAspectRatio = when (strAt("EmuCore/GS/FMVAspectRatioSwitch")) {
                "Off" -> 0
                "Auto 4:3/3:2" -> 1
                "4:3" -> 2
                "16:9" -> 3
                "10:7" -> 4
                else -> this.fmvAspectRatio
            },
            deinterlaceMode = intAt("EmuCore/GS/deinterlace_mode") ?: this.deinterlaceMode,
            framerateNtsc = floatAt("EmuCore/GS/FramerateNTSC") ?: this.framerateNtsc,
            frameratePal = floatAt("EmuCore/GS/FrameratePAL") ?: this.frameratePal,
            hwMipmap = boolAt("EmuCore/GS/hw_mipmap") ?: this.hwMipmap,
            accurateBlendingUnit = intAt("EmuCore/GS/accurate_blending_unit") ?: this.accurateBlendingUnit,
            textureFiltering = intAt("EmuCore/GS/filter") ?: this.textureFiltering,
            displayBilinear = intAt("EmuCore/GS/linear_present_mode") ?: this.displayBilinear,
            texturePreloading = intAt("EmuCore/GS/texture_preloading") ?: this.texturePreloading,
            hardwareDownloadMode = intAt("EmuCore/GS/HWDownloadMode") ?: this.hardwareDownloadMode,
            tvShader = intAt("EmuCore/GS/TVShader") ?: this.tvShader,
            shadeBoost = boolAt("EmuCore/GS/ShadeBoost") ?: this.shadeBoost,
            shadeBoostBrightness = intAt("EmuCore/GS/ShadeBoost_Brightness") ?: this.shadeBoostBrightness,
            shadeBoostContrast = intAt("EmuCore/GS/ShadeBoost_Contrast") ?: this.shadeBoostContrast,
            shadeBoostSaturation = intAt("EmuCore/GS/ShadeBoost_Saturation") ?: this.shadeBoostSaturation,
            shadeBoostGamma = intAt("EmuCore/GS/ShadeBoost_Gamma") ?: this.shadeBoostGamma,
            fxaa = boolAt("EmuCore/GS/fxaa") ?: this.fxaa,
            shaderChainEnabled = boolAt("EmuCore/GS/ShaderChainEnabled") ?: this.shaderChainEnabled,
            shaderChainPreset = strAt("EmuCore/GS/ShaderChainPreset") ?: this.shaderChainPreset,
            shaderChainParams = strAt("EmuCore/GS/ShaderChainParams")?.let { raw ->
                // Hand-editable file, so a malformed blob is a real possibility: keep the
                // rest of the recovered settings rather than throwing the lot away.
                runCatching { shaderChainParamsFromJson(JSONObject(raw)) }.getOrNull()
            } ?: this.shaderChainParams,
            casMode = intAt("EmuCore/GS/CASMode") ?: this.casMode,
            casSharpness = intAt("EmuCore/GS/CASSharpness") ?: this.casSharpness,
            loadTextureReplacements = boolAt("EmuCore/GS/LoadTextureReplacements") ?: this.loadTextureReplacements,
            loadTextureReplacementsAsync = boolAt("EmuCore/GS/LoadTextureReplacementsAsync") ?: this.loadTextureReplacementsAsync,
            precacheTextureReplacements = boolAt("EmuCore/GS/PrecacheTextureReplacements") ?: this.precacheTextureReplacements,
            dumpReplaceableTextures = boolAt("EmuCore/GS/DumpReplaceableTextures") ?: this.dumpReplaceableTextures,
            osdShowTextureReplacements = boolAt("EmuCore/GS/OsdShowTextureReplacements") ?: this.osdShowTextureReplacements,
            osdShowFps = boolAt("EmuCore/GS/OsdShowFPS") ?: this.osdShowFps,
            osdScale = intAt("EmuCore/GS/OsdScale") ?: this.osdScale,
            osdColor = intAt("EmuCore/GS/OsdColor") ?: this.osdColor,
            vsyncEnable = boolAt("EmuCore/GS/VsyncEnable") ?: this.vsyncEnable,
            osdShowVps = boolAt("EmuCore/GS/OsdShowVPS") ?: this.osdShowVps,
            osdShowSpeed = boolAt("EmuCore/GS/OsdShowSpeed") ?: this.osdShowSpeed,
            osdShowCpu = boolAt("EmuCore/GS/OsdShowCPU") ?: this.osdShowCpu,
            osdShowGpu = boolAt("EmuCore/GS/OsdShowGPU") ?: this.osdShowGpu,
            osdShowResolution = boolAt("EmuCore/GS/OsdShowResolution") ?: this.osdShowResolution,
            osdShowGsStats = boolAt("EmuCore/GS/OsdShowGSStats") ?: this.osdShowGsStats,
            osdShowFrameTimes = boolAt("EmuCore/GS/OsdShowFrameTimes") ?: this.osdShowFrameTimes,
            osdShowHardwareInfo = boolAt("EmuCore/GS/OsdShowHardwareInfo") ?: this.osdShowHardwareInfo,
            // OsdMessagesPos is an enum int (0 None / 1 TopLeft); applyTo writes 1 when shown.
            osdShowMessages = intAt("EmuCore/GS/OsdMessagesPos")?.let { it != 0 } ?: this.osdShowMessages,
            osdShowGpuStats = boolAt("EmuCore/GS/OsdShowGPUStats") ?: this.osdShowGpuStats,
            osdShowVersion = boolAt("EmuCore/GS/OsdShowVersion") ?: this.osdShowVersion,
            osdShowSettings = boolAt("EmuCore/GS/OsdShowSettings") ?: this.osdShowSettings,
            osdShowInputs = boolAt("EmuCore/GS/OsdShowInputs") ?: this.osdShowInputs,
            screenOffsets = boolAt("EmuCore/GS/pcrtc_offsets") ?: this.screenOffsets,
            showOverscan = boolAt("EmuCore/GS/pcrtc_overscan") ?: this.showOverscan,
            antiBlur = boolAt("EmuCore/GS/pcrtc_antiblur") ?: this.antiBlur,
            disableInterlaceOffset = boolAt("EmuCore/GS/disable_interlace_offset") ?: this.disableInterlaceOffset,
            syncToHostRefresh = boolAt("EmuCore/GS/SyncToHostRefreshRate") ?: this.syncToHostRefresh,
            disableFramebufferFetch = boolAt("EmuCore/GS/DisableFramebufferFetch") ?: this.disableFramebufferFetch,
            hwRov = boolAt("EmuCore/GS/HWROV") ?: this.hwRov,
            hwAa1 = boolAt("EmuCore/GS/HWAA1") ?: this.hwAa1,
            adrenoFbFetch = boolAt("EmuCore/GS/EnableAdrenoFramebufferFetch") ?: this.adrenoFbFetch,
            forceMaliFbFetch = boolAt("EmuCore/GS/ForceMaliFramebufferFetch") ?: this.forceMaliFbFetch,
            useAngleOpenGL = boolAt("EmuCore/GS/AndroidUseAngleOpenGL") ?: this.useAngleOpenGL,
            overrideTextureBarriers = intAt("EmuCore/GS/OverrideTextureBarriers") ?: this.overrideTextureBarriers,
            gsBackThreadMode = intAt("EmuCore/GS/GSBackThreadMode") ?: this.gsBackThreadMode,
            disableVertexShaderExpand = boolAt("EmuCore/GS/DisableVertexShaderExpand") ?: this.disableVertexShaderExpand,
            useBlitSwapChain = boolAt("EmuCore/GS/UseBlitSwapChain") ?: this.useBlitSwapChain,
            disableShaderCache = boolAt("EmuCore/GS/DisableShaderCache") ?: this.disableShaderCache,
            hwAccurateAlphaTest = boolAt("EmuCore/GS/HWAccurateAlphaTest") ?: this.hwAccurateAlphaTest,
            drawBuffering = boolAt("EmuCore/GS/UserHacks_DrawBuffering") ?: this.drawBuffering,
            spinGpuReadbacks = boolAt("EmuCore/GS/HWSpinGPUForReadbacks") ?: this.spinGpuReadbacks,
            spinCpuReadbacks = boolAt("EmuCore/GS/HWSpinCPUForReadbacks") ?: this.spinCpuReadbacks,
            integerScaling = boolAt("EmuCore/GS/IntegerScaling") ?: this.integerScaling,
            cropLeft = intAt("EmuCore/GS/CropLeft") ?: this.cropLeft,
            cropTop = intAt("EmuCore/GS/CropTop") ?: this.cropTop,
            cropRight = intAt("EmuCore/GS/CropRight") ?: this.cropRight,
            cropBottom = intAt("EmuCore/GS/CropBottom") ?: this.cropBottom,
            dithering = intAt("EmuCore/GS/dithering_ps2") ?: this.dithering,
            vsyncQueueSize = intAt("EmuCore/GS/VsyncQueueSize") ?: this.vsyncQueueSize,
            autoFlushSw = boolAt("EmuCore/GS/autoflush_sw") ?: this.autoFlushSw,
            mipmapSw = boolAt("EmuCore/GS/mipmap") ?: this.mipmapSw,
            swThreads = intAt("EmuCore/GS/extrathreads") ?: this.swThreads,
            swThreadsHeight = intAt("EmuCore/GS/extrathreads_height") ?: this.swThreadsHeight,
            skipDrawStart = intAt("EmuCore/GS/UserHacks_SkipDraw_Start") ?: this.skipDrawStart,
            skipDrawEnd = intAt("EmuCore/GS/UserHacks_SkipDraw_End") ?: this.skipDrawEnd,
            // "UserHacks" is applyTo's derived master (manualUserHacks OR any hack set);
            // recovering it into manualUserHacks is idempotent — the individual hacks below
            // re-derive it when re-applied, and it preserves a master-on-with-no-hacks state.
            manualUserHacks = boolAt("EmuCore/GS/UserHacks") ?: this.manualUserHacks,
            autoFlush = intAt("EmuCore/GS/UserHacks_AutoFlushLevel") ?: this.autoFlush,
            halfPixelOffset = intAt("EmuCore/GS/UserHacks_HalfPixelOffset") ?: this.halfPixelOffset,
            limit24BitDepth = intAt("EmuCore/GS/UserHacks_Limit24BitDepth") ?: this.limit24BitDepth,
            textureInsideRt = intAt("EmuCore/GS/UserHacks_TextureInsideRt") ?: this.textureInsideRt,
            nativeScaling = intAt("EmuCore/GS/UserHacks_native_scaling") ?: this.nativeScaling,
            roundSprite = intAt("EmuCore/GS/UserHacks_round_sprite_offset") ?: this.roundSprite,
            bilinearUpscale = intAt("EmuCore/GS/UserHacks_BilinearHack") ?: this.bilinearUpscale,
            gpuTargetClut = intAt("EmuCore/GS/UserHacks_GPUTargetCLUTMode") ?: this.gpuTargetClut,
            cpuSpriteRenderBw = intAt("EmuCore/GS/UserHacks_CPUSpriteRenderBW") ?: this.cpuSpriteRenderBw,
            cpuSpriteRenderLevel = intAt("EmuCore/GS/UserHacks_CPUSpriteRenderLevel") ?: this.cpuSpriteRenderLevel,
            cpuClutRender = intAt("EmuCore/GS/UserHacks_CPUCLUTRender") ?: this.cpuClutRender,
            alignSprite = boolAt("EmuCore/GS/UserHacks_align_sprite_X") ?: this.alignSprite,
            mergeSprite = boolAt("EmuCore/GS/UserHacks_merge_pp_sprite") ?: this.mergeSprite,
            forceEvenSpritePosition = boolAt("EmuCore/GS/UserHacks_ForceEvenSpritePosition") ?: this.forceEvenSpritePosition,
            unscaledPaletteDraw = boolAt("EmuCore/GS/UserHacks_NativePaletteDraw") ?: this.unscaledPaletteDraw,
            textureOffsetX = intAt("EmuCore/GS/UserHacks_TCOffsetX") ?: this.textureOffsetX,
            textureOffsetY = intAt("EmuCore/GS/UserHacks_TCOffsetY") ?: this.textureOffsetY,
            gpuPaletteConversion = boolAt("EmuCore/GS/paltex") ?: this.gpuPaletteConversion,
            cpuFramebufferConversion = boolAt("EmuCore/GS/UserHacks_CPU_FB_Conversion") ?: this.cpuFramebufferConversion,
            readTargetsWhenClosing = boolAt("EmuCore/GS/UserHacks_ReadTCOnClose") ?: this.readTargetsWhenClosing,
            disableDepthEmulation = boolAt("EmuCore/GS/UserHacks_DisableDepthSupport") ?: this.disableDepthEmulation,
            disablePartialInvalidation = boolAt("EmuCore/GS/UserHacks_DisablePartialInvalidation") ?: this.disablePartialInvalidation,
            disableSafeFeatures = boolAt("EmuCore/GS/UserHacks_Disable_Safe_Features") ?: this.disableSafeFeatures,
            disableRenderFixes = boolAt("EmuCore/GS/UserHacks_DisableRenderFixes") ?: this.disableRenderFixes,
            preloadFrameData = boolAt("EmuCore/GS/preload_frame_with_gs_data") ?: this.preloadFrameData,
            estimateTextureRegion = boolAt("EmuCore/GS/UserHacks_EstimateTextureRegion") ?: this.estimateTextureRegion,
            triFilter = intAt("EmuCore/GS/TriFilter") ?: this.triFilter,
            maxAnisotropy = intAt("EmuCore/GS/MaxAnisotropy") ?: this.maxAnisotropy,
            gpuProfile = when (strAt("EmuCore/GS/AndroidGpuProfileOverride")) {
                "mali" -> 1
                "adreno" -> 2
                "powervr" -> 3
                "xclipse" -> 4
                "auto" -> 0
                else -> this.gpuProfile
            },
        )
    }

    /** Upstream-style per-game export (mirrors PCSX2's FullscreenUI): write only
     *  the keys that differ from [global] into the running game's
     *  gamesettings/<serial>_<CRC>.ini, so the on-disk layer is sparse and
     *  portable (a later global tweak still reaches the game for keys it didn't
     *  override). Reuses applyTo's exact field→key mapping via [emitSink]: the
     *  global pass captures a baseline, the effective pass writes the diff. The
     *  running game already reflects the change live, so the native commit does
     *  not reload — the INI applies as the game layer on the next boot. No-op
     *  when no VM is running. */
    fun writeGameSettingsIni(global: Settings, serial: String? = null) {
        // Baseline: global's persisted keys. applyTo early-returns before the
        // live pokes/commit while emitSink is set, so nothing touches the VM.
        val baseline = HashMap<String, String>()
        emitSink = { section, key, _, value -> baseline["$section$key"] = value }
        try {
            global.applyTo()
        } finally {
            emitSink = null
        }
        // With a running VM the target is the current game (gameIniBeginWrite). With no VM — a
        // per-game Reset done from the library — pass [serial] to locate the file directly; false
        // there means no stale override file exists, so there is nothing to rewrite.
        val began = if (serial == null) NativeApp.gameIniBeginWrite()
                    else NativeApp.gameIniBeginWriteForSerial(serial)
        if (!began) return
        // Effective pass: stream only the keys that differ from the baseline.
        emitSink = { section, key, _, value ->
            if (baseline["$section$key"] != value)
                NativeApp.gameIniPut(section, key, value)
        }
        try {
            applyTo()
        } finally {
            emitSink = null
        }
        NativeApp.gameIniCommitWrite()
    }

    /** Writes every EmuCore/GS key (display + renderer + hardware/upscaling
     *  fixes) into the native BASE settings layer. Pure persistence — no live
     *  pokes, no commit. Shared by [applyTo] (cold start / restart) and
     *  [applyGsLive] (running VM). Keep the key list in sync with
     *  Pcsx2Config::GSOptions::LoadSave. */
    private fun writeGsToNative() {
        val aspectRatioName = when (aspectRatio.coerceIn(0, 4)) {
            0 -> "Stretch"
            2 -> "4:3"
            3 -> "16:9"
            4 -> "10:7"
            else -> "Auto 4:3/3:2"
        }
        put("EmuCore/GS", "AspectRatio", "string", aspectRatioName)
        val fmvAspectRatioName = when (fmvAspectRatio.coerceIn(0, 4)) {
            1 -> "Auto 4:3/3:2"
            2 -> "4:3"
            3 -> "16:9"
            4 -> "10:7"
            else -> "Off"
        }
        put("EmuCore/GS", "FMVAspectRatioSwitch", "string", fmvAspectRatioName)
        put("EmuCore/GS", "deinterlace_mode", "int", deinterlaceMode.coerceIn(0, 9).toString())
        put("EmuCore/GS", "FramerateNTSC", "float", framerateNtsc.toString())
        put("EmuCore/GS", "FrameratePAL", "float", frameratePal.toString())
        put("EmuCore/GS", "hw_mipmap", "bool", hwMipmap.toString())
        put("EmuCore/GS", "accurate_blending_unit", "int", accurateBlendingUnit.toString())
        put("EmuCore/GS", "filter", "int", textureFiltering.toString())
        put("EmuCore/GS", "linear_present_mode", "int", displayBilinear.coerceIn(0, 2).toString())
        put("EmuCore/GS", "texture_preloading", "int", texturePreloading.toString())
        put("EmuCore/GS", "HWDownloadMode", "int", hardwareDownloadMode.coerceIn(0, 4).toString())
        put("EmuCore/GS", "TVShader", "int", tvShader.coerceIn(0, 7).toString())
        put("EmuCore/GS", "ShadeBoost", "bool", shadeBoost.toString())
        put("EmuCore/GS", "ShadeBoost_Brightness", "int", shadeBoostBrightness.coerceIn(1, 100).toString())
        put("EmuCore/GS", "ShadeBoost_Contrast", "int", shadeBoostContrast.coerceIn(1, 100).toString())
        put("EmuCore/GS", "ShadeBoost_Saturation", "int", shadeBoostSaturation.coerceIn(1, 100).toString())
        put("EmuCore/GS", "ShadeBoost_Gamma", "int", shadeBoostGamma.coerceIn(1, 100).toString())
        put("EmuCore/GS", "fxaa", "bool", fxaa.toString())
        put("EmuCore/GS", "ShaderChainEnabled", "bool", shaderChainEnabled.toString())
        put("EmuCore/GS", "ShaderChainPreset", "string", shaderChainPreset)
        // Parameter overrides, as one opaque JSON blob. Nothing in emucore reads this key —
        // there is no GSConfig field behind it, and the live values reach the renderer via
        // the push below, not through here. It is written so the map survives the same
        // round-trips every other field gets: settings export/import, and the reused-folder
        // recovery that rebuilds prefs from the INI after a fresh install.
        put("EmuCore/GS", "ShaderChainParams", "string", shaderChainParamsToJson(shaderChainParams).toString())
        // The actual live apply. Only the CURRENT preset's values are pushed (the rest are
        // kept for when the user picks those presets again), and only the overrides — a
        // parameter left out keeps what the chain has, which for the freshly built or
        // rebuilt chain this runs against is the author's initial. Resets are handled by
        // the UI's own pushEffective, which sends initials explicitly to a live chain.
        // Skipped under emitSink: an export has no renderer to push to.
        if (emitSink == null)
            ShaderParams.push(shaderChainPreset, shaderChainParams[shaderChainPreset].orEmpty())
        put("EmuCore/GS", "CASMode", "int", casMode.coerceIn(0, 2).toString())
        put("EmuCore/GS", "CASSharpness", "int", casSharpness.coerceIn(0, 100).toString())
        put("EmuCore/GS", "LoadTextureReplacements", "bool", loadTextureReplacements.toString())
        put("EmuCore/GS", "LoadTextureReplacementsAsync", "bool", loadTextureReplacementsAsync.toString())
        put("EmuCore/GS", "PrecacheTextureReplacements", "bool", precacheTextureReplacements.toString())
        put("EmuCore/GS", "DumpReplaceableTextures", "bool", dumpReplaceableTextures.toString())
        put("EmuCore/GS", "OsdShowTextureReplacements", "bool", osdShowTextureReplacements.toString())
        put("EmuCore/GS", "OsdShowFPS", "bool", osdShowFps.toString())
        put("EmuCore/GS", "OsdScale", "int", osdScale.coerceIn(25, 500).toString())
        put("EmuCore/GS", "OsdColor", "int", (osdColor and 0xFFFFFF).toString())
        put("EmuCore/GS", "VsyncEnable", "bool", vsyncEnable.toString())
        put("EmuCore/GS", "OsdShowVPS", "bool", osdShowVps.toString())
        put("EmuCore/GS", "OsdShowSpeed", "bool", osdShowSpeed.toString())
        put("EmuCore/GS", "OsdShowCPU", "bool", osdShowCpu.toString())
        put("EmuCore/GS", "OsdShowGPU", "bool", osdShowGpu.toString())
        put("EmuCore/GS", "OsdShowResolution", "bool", osdShowResolution.toString())
        put("EmuCore/GS", "OsdShowGSStats", "bool", osdShowGsStats.toString())
        put("EmuCore/GS", "OsdShowFrameTimes", "bool", osdShowFrameTimes.toString())
        put("EmuCore/GS", "OsdShowHardwareInfo", "bool", osdShowHardwareInfo.toString())
        put("EmuCore/GS", "OsdMessagesPos", "int", if (osdShowMessages) "1" else "0")
        put("EmuCore/GS", "OsdShowGPUStats", "bool", osdShowGpuStats.toString())
        put("EmuCore/GS", "OsdShowVersion", "bool", osdShowVersion.toString())
        put("EmuCore/GS", "OsdShowSettings", "bool", osdShowSettings.toString())
        put("EmuCore/GS", "OsdShowInputs", "bool", osdShowInputs.toString())
        // Display / PCRTC fixes (not gated by the UserHacks master).
        put("EmuCore/GS", "pcrtc_offsets", "bool", screenOffsets.toString())
        put("EmuCore/GS", "pcrtc_overscan", "bool", showOverscan.toString())
        put("EmuCore/GS", "pcrtc_antiblur", "bool", antiBlur.toString())
        put("EmuCore/GS", "disable_interlace_offset", "bool", disableInterlaceOffset.toString())
        put("EmuCore/GS", "SyncToHostRefreshRate", "bool", syncToHostRefresh.toString())
        put("EmuCore/GS", "DisableFramebufferFetch", "bool", disableFramebufferFetch.toString())
        put("EmuCore/GS", "HWROV", "bool", hwRov.toString())
        put("EmuCore/GS", "HWAA1", "bool", hwAa1.toString())
        put("EmuCore/GS", "EnableAdrenoFramebufferFetch", "bool", adrenoFbFetch.toString())
        put("EmuCore/GS", "ForceMaliFramebufferFetch", "bool", forceMaliFbFetch.toString())
        // Parity write (native reads the ARMSX2_ANGLE_EGL_LIBRARY env var set by
        // MainActivityRuntime.applyAngleEnv, not this key) — kept so the config file
        // reflects the toggle.
        put("EmuCore/GS", "AndroidUseAngleOpenGL", "bool", useAngleOpenGL.toString())
        put("EmuCore/GS", "OverrideTextureBarriers", "int", overrideTextureBarriers.coerceIn(-1, 1).toString())
        put("EmuCore/GS", "GSBackThreadMode", "int", gsBackThreadMode.coerceIn(0, 3).toString())
        put("EmuCore/GS", "DisableVertexShaderExpand", "bool", disableVertexShaderExpand.toString())
        put("EmuCore/GS", "UseBlitSwapChain", "bool", useBlitSwapChain.toString())
        put("EmuCore/GS", "DisableShaderCache", "bool", disableShaderCache.toString())
        put("EmuCore/GS", "HWAccurateAlphaTest", "bool", hwAccurateAlphaTest.toString())
        put("EmuCore/GS", "UserHacks_DrawBuffering", "bool", drawBuffering.toString())
        put("EmuCore/GS", "HWSpinGPUForReadbacks", "bool", spinGpuReadbacks.toString())
        put("EmuCore/GS", "HWSpinCPUForReadbacks", "bool", spinCpuReadbacks.toString())
        put("EmuCore/GS", "IntegerScaling", "bool", integerScaling.toString())
        // Display zoom (#383) overrides the manual crops while active: trim every edge by the
        // same fraction so the picture scales up without distortion. Nominal 640x448 native
        // frame; the zoom factor is what matters visually, so an approximate frame size is fine.
        // (1 - 100/Z)/2 is the per-edge fraction that leaves 1/Z of the image visible, centred.
        val zoom = displayZoom.coerceIn(100, 150)
        val zCropX = if (zoom > 100) ((640.0 * (1.0 - 100.0 / zoom)) / 2.0).toInt() else -1
        val zCropY = if (zoom > 100) ((448.0 * (1.0 - 100.0 / zoom)) / 2.0).toInt() else -1
        val effLeft = if (zCropX >= 0) zCropX else cropLeft
        val effRight = if (zCropX >= 0) zCropX else cropRight
        val effTop = if (zCropY >= 0) zCropY else cropTop
        val effBottom = if (zCropY >= 0) zCropY else cropBottom
        put("EmuCore/GS", "CropLeft", "int", effLeft.coerceIn(0, 640).toString())
        put("EmuCore/GS", "CropTop", "int", effTop.coerceIn(0, 640).toString())
        put("EmuCore/GS", "CropRight", "int", effRight.coerceIn(0, 640).toString())
        put("EmuCore/GS", "CropBottom", "int", effBottom.coerceIn(0, 640).toString())
        put("EmuCore/GS", "dithering_ps2", "int", dithering.coerceIn(0, 3).toString())
        put("EmuCore/GS", "VsyncQueueSize", "int", vsyncQueueSize.coerceIn(0, 3).toString())
        put("EmuCore/GS", "autoflush_sw", "bool", autoFlushSw.toString())
        put("EmuCore/GS", "mipmap", "bool", mipmapSw.toString())
        put("EmuCore/GS", "extrathreads", "int", swThreads.coerceIn(0, 10).toString())
        put("EmuCore/GS", "extrathreads_height", "int", swThreadsHeight.coerceIn(0, 8).toString())
        // Skip-draw is a UserHack (gated by the master toggle below).
        put("EmuCore/GS", "UserHacks_SkipDraw_Start", "int", skipDrawStart.coerceAtLeast(0).toString())
        put("EmuCore/GS", "UserHacks_SkipDraw_End", "int", skipDrawEnd.coerceAtLeast(0).toString())
        // Master hardware-fixes toggle. Auto-enables when ANY individual hack is
        // non-default so the user doesn't have to flip it; PCSX2 masks every
        // UserHacks_* key when this is off (GSOptions::MaskUserHacks).
        put("EmuCore/GS", "UserHacks", "bool", anyUserHackEnabled().toString())
        put("EmuCore/GS", "UserHacks_AutoFlushLevel", "int", autoFlush.coerceIn(0, 2).toString())
        put("EmuCore/GS", "UserHacks_HalfPixelOffset", "int", halfPixelOffset.coerceIn(0, 5).toString())
        put("EmuCore/GS", "UserHacks_Limit24BitDepth", "int", limit24BitDepth.coerceIn(0, 2).toString())
        put("EmuCore/GS", "UserHacks_TextureInsideRt", "int", textureInsideRt.coerceIn(0, 2).toString())
        put("EmuCore/GS", "UserHacks_native_scaling", "int", nativeScaling.coerceIn(0, 4).toString())
        put("EmuCore/GS", "UserHacks_round_sprite_offset", "int", roundSprite.coerceIn(0, 2).toString())
        put("EmuCore/GS", "UserHacks_BilinearHack", "int", bilinearUpscale.coerceIn(0, 3).toString())
        put("EmuCore/GS", "UserHacks_GPUTargetCLUTMode", "int", gpuTargetClut.coerceIn(0, 2).toString())
        put("EmuCore/GS", "UserHacks_CPUSpriteRenderBW", "int", cpuSpriteRenderBw.coerceIn(0, 3).toString())
        put("EmuCore/GS", "UserHacks_CPUSpriteRenderLevel", "int", cpuSpriteRenderLevel.coerceIn(0, 5).toString())
        put("EmuCore/GS", "UserHacks_CPUCLUTRender", "int", cpuClutRender.coerceIn(0, 2).toString())
        // Upscaling fixes (parity additions)
        put("EmuCore/GS", "UserHacks_align_sprite_X", "bool", alignSprite.toString())
        put("EmuCore/GS", "UserHacks_merge_pp_sprite", "bool", mergeSprite.toString())
        put("EmuCore/GS", "UserHacks_ForceEvenSpritePosition", "bool", forceEvenSpritePosition.toString())
        put("EmuCore/GS", "UserHacks_NativePaletteDraw", "bool", unscaledPaletteDraw.toString())
        put("EmuCore/GS", "UserHacks_TCOffsetX", "int", textureOffsetX.coerceIn(0, 10000).toString())
        put("EmuCore/GS", "UserHacks_TCOffsetY", "int", textureOffsetY.coerceIn(0, 10000).toString())
        // Hardware fixes (parity additions)
        put("EmuCore/GS", "paltex", "bool", gpuPaletteConversion.toString())
        put("EmuCore/GS", "UserHacks_CPU_FB_Conversion", "bool", cpuFramebufferConversion.toString())
        put("EmuCore/GS", "UserHacks_ReadTCOnClose", "bool", readTargetsWhenClosing.toString())
        put("EmuCore/GS", "UserHacks_DisableDepthSupport", "bool", disableDepthEmulation.toString())
        put("EmuCore/GS", "UserHacks_DisablePartialInvalidation", "bool", disablePartialInvalidation.toString())
        put("EmuCore/GS", "UserHacks_Disable_Safe_Features", "bool", disableSafeFeatures.toString())
        put("EmuCore/GS", "UserHacks_DisableRenderFixes", "bool", disableRenderFixes.toString())
        put("EmuCore/GS", "preload_frame_with_gs_data", "bool", preloadFrameData.toString())
        put("EmuCore/GS", "UserHacks_EstimateTextureRegion", "bool", estimateTextureRegion.toString())
        put("EmuCore/GS", "TriFilter", "int", triFilter.toString())
        put("EmuCore/GS", "MaxAnisotropy", "int", maxAnisotropy.toString())
        val gpuProfileStr = when (gpuProfile) {
            1 -> "mali"
            2 -> "adreno"
            3 -> "powervr"
            4 -> "xclipse"
            else -> "auto"
        }
        put("EmuCore/GS", "AndroidGpuProfileOverride", "string", gpuProfileStr)
    }

    /** True when any hardware/upscaling fix is non-default — used to auto-enable
     *  the UserHacks master so individual hacks aren't silently masked off. */
    private fun anyUserHackEnabled(): Boolean =
        manualUserHacks ||
            autoFlush != 0 || halfPixelOffset != 0 || limit24BitDepth != 0 ||
            textureInsideRt != 0 || nativeScaling != 0 || roundSprite != 0 ||
            bilinearUpscale != 0 || gpuTargetClut != 0 || cpuSpriteRenderBw != 0 ||
            cpuSpriteRenderLevel != 0 || cpuClutRender != 0 ||
            textureOffsetX != 0 || textureOffsetY != 0 ||
            alignSprite || mergeSprite || forceEvenSpritePosition || unscaledPaletteDraw ||
            gpuPaletteConversion || cpuFramebufferConversion || readTargetsWhenClosing ||
            disableDepthEmulation || disablePartialInvalidation || disableSafeFeatures ||
            disableRenderFixes || preloadFrameData || estimateTextureRegion || drawBuffering ||
            skipDrawStart != 0 || skipDrawEnd != 0

    /** Live GS-only apply for a running VM: persist all EmuCore/GS keys, then
     *  reconfigure the GS thread without the heavy CPU/JIT rebuild commitSettings()
     *  does. Lets renderer / hardware-fix / upscaling-fix changes apply instantly
     *  mid-game. */
    fun applyGsLive(): Boolean {
        writeGsToNative()
        return NativeApp.applyGSSettingsLive()
    }

    /** True when any field a live GS reconfigure ([applyGsLive]) can pick up
     *  differs from [other]. Lets the in-game delta path skip the GS thread
     *  park when only non-GS settings (audio, frame limit, …) changed.
     *  Excludes display aspect (its own live setter) and gpuProfile (device-init
     *  only — needs a renderer restart). */
    // NOTE: FramerateNTSC/PAL are intentionally NOT here — the generic GS live
    // reconfigure (applyGSSettingsLive) doesn't recompute the vsync target. They
    // get their OWN live path instead: applySafeLiveDelta routes a framerate change
    // to LiveGsApplyQueue.applyFramerate → NativeApp.applyFramerateLive, which parks
    // the VM and recomputes vsync. Keeping them out of here avoids a redundant
    // (and park-free, thus ineffective) GS reconfigure for a framerate-only edit.
    fun gsDiffersFrom(other: Settings): Boolean =
        deinterlaceMode != other.deinterlaceMode ||
            textureFiltering != other.textureFiltering ||
            displayBilinear != other.displayBilinear ||
            texturePreloading != other.texturePreloading ||
            hardwareDownloadMode != other.hardwareDownloadMode ||
            tvShader != other.tvShader ||
            shadeBoost != other.shadeBoost ||
            shadeBoostBrightness != other.shadeBoostBrightness ||
            shadeBoostContrast != other.shadeBoostContrast ||
            shadeBoostSaturation != other.shadeBoostSaturation ||
            shadeBoostGamma != other.shadeBoostGamma ||
            fxaa != other.fxaa ||
            casMode != other.casMode ||
            casSharpness != other.casSharpness ||
            accurateBlendingUnit != other.accurateBlendingUnit ||
            hwMipmap != other.hwMipmap ||
            triFilter != other.triFilter ||
            maxAnisotropy != other.maxAnisotropy ||
            manualUserHacks != other.manualUserHacks ||
            autoFlush != other.autoFlush ||
            halfPixelOffset != other.halfPixelOffset ||
            limit24BitDepth != other.limit24BitDepth ||
            textureInsideRt != other.textureInsideRt ||
            nativeScaling != other.nativeScaling ||
            roundSprite != other.roundSprite ||
            bilinearUpscale != other.bilinearUpscale ||
            gpuTargetClut != other.gpuTargetClut ||
            cpuSpriteRenderBw != other.cpuSpriteRenderBw ||
            cpuSpriteRenderLevel != other.cpuSpriteRenderLevel ||
            cpuClutRender != other.cpuClutRender ||
            alignSprite != other.alignSprite ||
            mergeSprite != other.mergeSprite ||
            forceEvenSpritePosition != other.forceEvenSpritePosition ||
            unscaledPaletteDraw != other.unscaledPaletteDraw ||
            textureOffsetX != other.textureOffsetX ||
            textureOffsetY != other.textureOffsetY ||
            gpuPaletteConversion != other.gpuPaletteConversion ||
            cpuFramebufferConversion != other.cpuFramebufferConversion ||
            readTargetsWhenClosing != other.readTargetsWhenClosing ||
            disableDepthEmulation != other.disableDepthEmulation ||
            disablePartialInvalidation != other.disablePartialInvalidation ||
            disableSafeFeatures != other.disableSafeFeatures ||
            disableRenderFixes != other.disableRenderFixes ||
            preloadFrameData != other.preloadFrameData ||
            estimateTextureRegion != other.estimateTextureRegion ||
            hwAccurateAlphaTest != other.hwAccurateAlphaTest ||
            drawBuffering != other.drawBuffering ||
            // Texture-replacement toggles: without these here the in-game "Load Texture
            // Packs" switch only wrote the base layer (setSetting) and never fired the
            // live GS reconfigure, so a just-imported pack didn't appear until the next
            // game boot. Including them routes through applyGSSettingsLive → GSUpdateConfig
            // → GSTextureReplacements reload/purge, so the pack loads immediately.
            loadTextureReplacements != other.loadTextureReplacements ||
            loadTextureReplacementsAsync != other.loadTextureReplacementsAsync ||
            precacheTextureReplacements != other.precacheTextureReplacements ||
            dumpReplaceableTextures != other.dumpReplaceableTextures ||
            osdShowTextureReplacements != other.osdShowTextureReplacements

    fun toJson(): JSONObject = JSONObject().apply {
        put("eeCycleRate", eeCycleRate)
        put("eeCycleSkip", eeCycleSkip)
        put("eeClampMode", eeClampMode)
        put("vuClampMode", vuClampMode)
        put("mtvu", mtvu)
        put("vu1Instant", vu1Instant)
        put("vuFlagHack", vuFlagHack)
        put("fastCDVD", fastCDVD)
        put("intcStat", intcStat)
        put("waitLoop", waitLoop)
        put("vuNeonFusions", vuNeonFusions)
        put("vuDeferredWrites", vuDeferredWrites)
        put("vuSkipStallSim", vuSkipStallSim)
        put("frameLimitEnable", frameLimitEnable)
        put("nominalSpeedPercent", nominalSpeedPercent)
        put("fpsLimit", fpsLimit)
        put("frameSkip", frameSkip)
        put("audioVolume", audioVolume)
        put("audioMuted", audioMuted)
        put("audioSwapChannels", audioSwapChannels)
        put("audioTimeStretch", audioTimeStretch)
        put("audioBufferMs", audioBufferMs)
        put("audioOutputLatencyMs", audioOutputLatencyMs)
        put("audioFastForwardVolume", audioFastForwardVolume)
        put("spu2NeonReverb", spu2NeonReverb)
        put("audioOpenSLES", audioOpenSLES)
        put("spu2LightweightMix", spu2LightweightMix)
        put("renderer", renderer)
        put("upscaleFloat", upscaleFloat.toDouble())
        put("customDriverId", customDriverId)
        put("orientation", orientation)
        put("portraitRenderTop", portraitRenderTop)
        put("framerateNtsc", framerateNtsc.toDouble())
        put("frameratePal", frameratePal.toDouble())
        put("enablePatches", enablePatches)
        put("enableCheats", enableCheats)
        put("enableWideScreenPatches", enableWideScreenPatches)
        put("enableNoInterlacingPatches", enableNoInterlacingPatches)
        put("enableFastBoot", enableFastBoot)
        put("hostFs", hostFs)
        put("enableGameFixes", enableGameFixes)
        put("gamefixSoftwareRendererFmv", gamefixSoftwareRendererFmv)
        put("gamefixSkipMpeg", gamefixSkipMpeg)
        put("gamefixEETiming", gamefixEETiming)
        put("gamefixInstantDma", gamefixInstantDma)
        put("gamefixBlitInternalFps", gamefixBlitInternalFps)
        put("gamefixFpuMul", gamefixFpuMul)
        put("gamefixOphFlag", gamefixOphFlag)
        put("gamefixGifFifo", gamefixGifFifo)
        put("gamefixDmaBusy", gamefixDmaBusy)
        put("gamefixVif1Stall", gamefixVif1Stall)
        put("gamefixIbit", gamefixIbit)
        put("gamefixFullVu0Sync", gamefixFullVu0Sync)
        put("gamefixVuAddSub", gamefixVuAddSub)
        put("gamefixVuOverflow", gamefixVuOverflow)
        put("gamefixXgkick", gamefixXgkick)
        put("gamefixGoemonTlb", gamefixGoemonTlb)
        put("gamefixVuSync", gamefixVuSync)
        put("skipDuplicateFrames", skipDuplicateFrames)
        put("eeFpuRoundMode", eeFpuRoundMode)
        put("vu0RoundMode", vu0RoundMode)
        put("vu1RoundMode", vu1RoundMode)
        put("screenOffsets", screenOffsets)
        put("showOverscan", showOverscan)
        put("antiBlur", antiBlur)
        put("disableInterlaceOffset", disableInterlaceOffset)
        put("syncToHostRefresh", syncToHostRefresh)
        put("disableFramebufferFetch", disableFramebufferFetch)
        put("hwRov", hwRov)
        put("hwAa1", hwAa1)
        put("adrenoFbFetch", adrenoFbFetch)
        put("forceMaliFbFetch", forceMaliFbFetch)
        put("useAngleOpenGL", useAngleOpenGL)
        put("overrideTextureBarriers", overrideTextureBarriers)
        put("gsBackThreadMode", gsBackThreadMode)
        put("disableVertexShaderExpand", disableVertexShaderExpand)
        put("useBlitSwapChain", useBlitSwapChain)
        put("disableShaderCache", disableShaderCache)
        put("hwAccurateAlphaTest", hwAccurateAlphaTest)
        put("skipDrawStart", skipDrawStart)
        put("skipDrawEnd", skipDrawEnd)
        put("spinGpuReadbacks", spinGpuReadbacks)
        put("spinCpuReadbacks", spinCpuReadbacks)
        put("integerScaling", integerScaling)
        put("cropLeft", cropLeft)
        put("displayZoom", displayZoom)
        put("cropTop", cropTop)
        put("cropRight", cropRight)
        put("cropBottom", cropBottom)
        put("dithering", dithering)
        put("vsyncQueueSize", vsyncQueueSize)
        put("autoFlushSw", autoFlushSw)
        put("mipmapSw", mipmapSw)
        put("swThreads", swThreads)
        put("swThreadsHeight", swThreadsHeight)
        put("aspectRatio", aspectRatio)
        put("fmvAspectRatio", fmvAspectRatio)
        put("deinterlaceMode", deinterlaceMode)
        put("dev9EthEnable", dev9EthEnable)
        put("dev9EthApi", dev9EthApi)
        put("dev9EthDevice", dev9EthDevice)
        put("dev9EthLogDhcp", dev9EthLogDhcp)
        put("dev9EthLogDns", dev9EthLogDns)
        put("dev9InterceptDhcp", dev9InterceptDhcp)
        put("dev9Ps2Ip", dev9Ps2Ip)
        put("dev9Mask", dev9Mask)
        put("dev9Gateway", dev9Gateway)
        put("dev9Dns1", dev9Dns1)
        put("dev9Dns2", dev9Dns2)
        put("dev9AutoMask", dev9AutoMask)
        put("dev9AutoGateway", dev9AutoGateway)
        put("dev9ModeDns1", dev9ModeDns1)
        put("dev9ModeDns2", dev9ModeDns2)
        put("dev9EthHosts", JSONArray().apply {
            dev9EthHosts.forEach { h ->
                put(JSONObject().apply {
                    put("url", h.url)
                    put("ip", h.ip)
                    put("enabled", h.enabled)
                })
            }
        })
        put("dev9HddEnable", dev9HddEnable)
        put("dev9HddFile", dev9HddFile)
        put("memoryCardSlot1Enabled", memoryCardSlot1Enabled)
        put("memoryCardSlot1Filename", memoryCardSlot1Filename)
        put("biosFilename", biosFilename)
        put("memoryCardSlot2Enabled", memoryCardSlot2Enabled)
        put("memoryCardSlot2Filename", memoryCardSlot2Filename)
        put("usbKeyboard", usbKeyboard)
        put("recEE", recEE)
        put("recIOP", recIOP)
        put("recVU0", recVU0)
        put("recVU1", recVU1)
        put("enableFastmem", enableFastmem)
        put("vu1InlineFmacStall", vu1InlineFmacStall)
        put("vu1CrossBlockPState", vu1CrossBlockPState)
        put("vu1InlineDrainTestPipes", vu1InlineDrainTestPipes)
        put("vu1FmacInstanceRouting", vu1FmacInstanceRouting)
        put("hwMipmap", hwMipmap)
        put("accurateBlendingUnit", accurateBlendingUnit)
        put("textureFiltering", textureFiltering)
        put("displayBilinear", displayBilinear)
        put("texturePreloading", texturePreloading)
        put("hardwareDownloadMode", hardwareDownloadMode)
        put("tvShader", tvShader)
        put("shadeBoost", shadeBoost)
        put("shadeBoostBrightness", shadeBoostBrightness)
        put("shadeBoostContrast", shadeBoostContrast)
        put("shadeBoostSaturation", shadeBoostSaturation)
        put("shadeBoostGamma", shadeBoostGamma)
        put("fxaa", fxaa)
        put("shaderChainEnabled", shaderChainEnabled)
        put("shaderChainPreset", shaderChainPreset)
        put("shaderChainParams", shaderChainParamsToJson(shaderChainParams))
        put("casMode", casMode)
        put("casSharpness", casSharpness)
        put("loadTextureReplacements", loadTextureReplacements)
        put("loadTextureReplacementsAsync", loadTextureReplacementsAsync)
        put("precacheTextureReplacements", precacheTextureReplacements)
        put("dumpReplaceableTextures", dumpReplaceableTextures)
        put("osdShowTextureReplacements", osdShowTextureReplacements)
        put("osdShowFps", osdShowFps)
        put("osdScale", osdScale)
        put("osdColor", osdColor)
        put("vsyncEnable", vsyncEnable)
        put("osdShowVps", osdShowVps)
        put("osdShowSpeed", osdShowSpeed)
        put("osdShowCpu", osdShowCpu)
        put("osdShowGpu", osdShowGpu)
        put("osdShowResolution", osdShowResolution)
        put("osdShowGsStats", osdShowGsStats)
        put("osdShowFrameTimes", osdShowFrameTimes)
        put("osdShowHardwareInfo", osdShowHardwareInfo)
        put("osdShowMessages", osdShowMessages)
        put("osdShowGpuStats", osdShowGpuStats)
        put("osdShowVersion", osdShowVersion)
        put("osdShowSettings", osdShowSettings)
        put("osdShowInputs", osdShowInputs)
        put("autoFlush", autoFlush)
        put("halfPixelOffset", halfPixelOffset)
        put("limit24BitDepth", limit24BitDepth)
        put("manualUserHacks", manualUserHacks)
        put("textureInsideRt", textureInsideRt)
        put("nativeScaling", nativeScaling)
        put("roundSprite", roundSprite)
        put("bilinearUpscale", bilinearUpscale)
        put("gpuTargetClut", gpuTargetClut)
        put("cpuSpriteRenderBw", cpuSpriteRenderBw)
        put("cpuSpriteRenderLevel", cpuSpriteRenderLevel)
        put("alignSprite", alignSprite)
        put("mergeSprite", mergeSprite)
        put("forceEvenSpritePosition", forceEvenSpritePosition)
        put("unscaledPaletteDraw", unscaledPaletteDraw)
        put("textureOffsetX", textureOffsetX)
        put("textureOffsetY", textureOffsetY)
        put("gpuPaletteConversion", gpuPaletteConversion)
        put("cpuFramebufferConversion", cpuFramebufferConversion)
        put("readTargetsWhenClosing", readTargetsWhenClosing)
        put("disableDepthEmulation", disableDepthEmulation)
        put("disablePartialInvalidation", disablePartialInvalidation)
        put("disableSafeFeatures", disableSafeFeatures)
        put("disableRenderFixes", disableRenderFixes)
        put("preloadFrameData", preloadFrameData)
        put("estimateTextureRegion", estimateTextureRegion)
        put("drawBuffering", drawBuffering)
        put("cpuClutRender", cpuClutRender)
        put("triFilter", triFilter)
        put("maxAnisotropy", maxAnisotropy)
        put("gpuProfile", gpuProfile)
    }

    companion object {
        /** When non-null, [put] routes persisted-key emits here instead of the
         *  native base layer. Set transiently by [writeGameSettingsIni] to
         *  capture the key set for the sparse per-game INI export without
         *  touching the base layer or re-poking the running VM. */
        @JvmStatic
        internal var emitSink: ((String, String, String, String) -> Unit)? = null

        /** One-tap "Low-End" performance snapshot applied on top of [base].
         *  Only cheap, safe-for-most levers that already exist as fields:
         *    - accurate_blending_unit = Minimum (0)   — cheapest blend path
         *    - internal resolution   = 1x (native)     — biggest GPU win
         *    - hw mipmap off, GPU palette conversion off — drop optional GPU work
         *    - texture preloading    = Partial (1)      — lower upload stalls
         *    - HW ROV off                                — never a win on tilers
         *    - EE cycle skip         = 1                 — mild CPU headroom
         *    - MTVU                   = device-aware      — only when >= 6 cores
         *  [mtvu] is passed in (from [com.armsx2.DeviceTier.mtvuDefault]) rather
         *  than read here so config/ stays free of Android context deps.
         *  NOTE: intentionally does NOT touch CAS — there is no CAS Settings
         *  field wired in this build. */
        fun lowEndPreset(base: Settings, mtvu: Boolean): Settings = base.copy(
            accurateBlendingUnit = 0,   // Minimum
            upscaleFloat = 1.0f,        // native resolution
            hwMipmap = false,           // mipmap off
            gpuPaletteConversion = false,
            texturePreloading = 1,      // Partial
            hwRov = false,              // ROV off
            eeCycleSkip = 1,
            mtvu = mtvu,
        )

        /** Lenient parse — missing keys fall back to defaults so old saved
         *  blobs survive when new fields are added. */
        fun fromJson(json: JSONObject): Settings {
            val def = Settings()
            return Settings(
                eeCycleRate = json.optInt("eeCycleRate", def.eeCycleRate),
                eeCycleSkip = json.optInt("eeCycleSkip", def.eeCycleSkip),
                eeClampMode = json.optInt("eeClampMode", def.eeClampMode),
                vuClampMode = json.optInt("vuClampMode", def.vuClampMode),
                mtvu = json.optBoolean("mtvu", def.mtvu),
                vu1Instant = json.optBoolean("vu1Instant", def.vu1Instant),
                vuFlagHack = json.optBoolean("vuFlagHack", def.vuFlagHack),
                fastCDVD = json.optBoolean("fastCDVD", def.fastCDVD),
                intcStat = json.optBoolean("intcStat", def.intcStat),
                waitLoop = json.optBoolean("waitLoop", def.waitLoop),
                vuNeonFusions = json.optBoolean("vuNeonFusions", def.vuNeonFusions),
                vuDeferredWrites = json.optBoolean("vuDeferredWrites", def.vuDeferredWrites),
                vuSkipStallSim = json.optBoolean("vuSkipStallSim", def.vuSkipStallSim),
                frameLimitEnable = json.optBoolean("frameLimitEnable", def.frameLimitEnable),
                nominalSpeedPercent = json.optInt("nominalSpeedPercent", def.nominalSpeedPercent),
                fpsLimit = json.optInt("fpsLimit", def.fpsLimit),
                frameSkip = json.optInt("frameSkip", def.frameSkip),
                audioVolume = json.optInt("audioVolume", def.audioVolume),
                audioMuted = json.optBoolean("audioMuted", def.audioMuted),
                audioSwapChannels = json.optBoolean("audioSwapChannels", def.audioSwapChannels),
                audioTimeStretch = json.optBoolean("audioTimeStretch", def.audioTimeStretch),
                audioBufferMs = json.optInt("audioBufferMs", def.audioBufferMs),
                audioOutputLatencyMs = json.optInt("audioOutputLatencyMs", def.audioOutputLatencyMs),
                audioFastForwardVolume = json.optInt("audioFastForwardVolume", def.audioFastForwardVolume),
                spu2NeonReverb = json.optBoolean("spu2NeonReverb", def.spu2NeonReverb),
                audioOpenSLES = json.optBoolean("audioOpenSLES", def.audioOpenSLES),
                spu2LightweightMix = json.optBoolean("spu2LightweightMix", def.spu2LightweightMix),
                renderer = json.optString("renderer", def.renderer),
                upscaleFloat = json.optDouble("upscaleFloat", def.upscaleFloat.toDouble()).toFloat(),
                customDriverId = json.optString("customDriverId", def.customDriverId),
                orientation = json.optInt("orientation", def.orientation),
                portraitRenderTop = json.optBoolean("portraitRenderTop", def.portraitRenderTop),
                framerateNtsc = json.optDouble("framerateNtsc", def.framerateNtsc.toDouble()).toFloat(),
                frameratePal = json.optDouble("frameratePal", def.frameratePal.toDouble()).toFloat(),
                enablePatches = json.optBoolean("enablePatches", def.enablePatches),
                enableCheats = json.optBoolean("enableCheats", def.enableCheats),
                enableWideScreenPatches = json.optBoolean("enableWideScreenPatches", def.enableWideScreenPatches),
                enableNoInterlacingPatches = json.optBoolean("enableNoInterlacingPatches", def.enableNoInterlacingPatches),
                enableFastBoot = json.optBoolean("enableFastBoot", def.enableFastBoot),
                hostFs = json.optBoolean("hostFs", def.hostFs),
                enableGameFixes = json.optBoolean("enableGameFixes", def.enableGameFixes),
                gamefixSoftwareRendererFmv = json.optBoolean("gamefixSoftwareRendererFmv", def.gamefixSoftwareRendererFmv),
                gamefixSkipMpeg = json.optBoolean("gamefixSkipMpeg", def.gamefixSkipMpeg),
                gamefixEETiming = json.optBoolean("gamefixEETiming", def.gamefixEETiming),
                gamefixInstantDma = json.optBoolean("gamefixInstantDma", def.gamefixInstantDma),
                gamefixBlitInternalFps = json.optBoolean("gamefixBlitInternalFps", def.gamefixBlitInternalFps),
                gamefixFpuMul = json.optBoolean("gamefixFpuMul", def.gamefixFpuMul),
                gamefixOphFlag = json.optBoolean("gamefixOphFlag", def.gamefixOphFlag),
                gamefixGifFifo = json.optBoolean("gamefixGifFifo", def.gamefixGifFifo),
                gamefixDmaBusy = json.optBoolean("gamefixDmaBusy", def.gamefixDmaBusy),
                gamefixVif1Stall = json.optBoolean("gamefixVif1Stall", def.gamefixVif1Stall),
                gamefixIbit = json.optBoolean("gamefixIbit", def.gamefixIbit),
                gamefixFullVu0Sync = json.optBoolean("gamefixFullVu0Sync", def.gamefixFullVu0Sync),
                gamefixVuAddSub = json.optBoolean("gamefixVuAddSub", def.gamefixVuAddSub),
                gamefixVuOverflow = json.optBoolean("gamefixVuOverflow", def.gamefixVuOverflow),
                gamefixXgkick = json.optBoolean("gamefixXgkick", def.gamefixXgkick),
                gamefixGoemonTlb = json.optBoolean("gamefixGoemonTlb", def.gamefixGoemonTlb),
                gamefixVuSync = json.optBoolean("gamefixVuSync", def.gamefixVuSync),
                skipDuplicateFrames = json.optBoolean("skipDuplicateFrames", def.skipDuplicateFrames),
                eeFpuRoundMode = json.optInt("eeFpuRoundMode", def.eeFpuRoundMode),
                vu0RoundMode = json.optInt("vu0RoundMode", def.vu0RoundMode),
                vu1RoundMode = json.optInt("vu1RoundMode", def.vu1RoundMode),
                screenOffsets = json.optBoolean("screenOffsets", def.screenOffsets),
                showOverscan = json.optBoolean("showOverscan", def.showOverscan),
                antiBlur = json.optBoolean("antiBlur", def.antiBlur),
                disableInterlaceOffset = json.optBoolean("disableInterlaceOffset", def.disableInterlaceOffset),
                syncToHostRefresh = json.optBoolean("syncToHostRefresh", def.syncToHostRefresh),
                disableFramebufferFetch = json.optBoolean("disableFramebufferFetch", def.disableFramebufferFetch),
                hwRov = json.optBoolean("hwRov", def.hwRov),
                hwAa1 = json.optBoolean("hwAa1", def.hwAa1),
                hwAat = false,
                adrenoFbFetch = json.optBoolean("adrenoFbFetch", def.adrenoFbFetch),
                forceMaliFbFetch = json.optBoolean("forceMaliFbFetch", def.forceMaliFbFetch),
                useAngleOpenGL = json.optBoolean("useAngleOpenGL", def.useAngleOpenGL),
                overrideTextureBarriers = json.optInt("overrideTextureBarriers", def.overrideTextureBarriers),
                gsBackThreadMode = json.optInt("gsBackThreadMode", def.gsBackThreadMode),
                disableVertexShaderExpand = json.optBoolean("disableVertexShaderExpand", def.disableVertexShaderExpand),
                useBlitSwapChain = json.optBoolean("useBlitSwapChain", def.useBlitSwapChain),
                disableShaderCache = json.optBoolean("disableShaderCache", def.disableShaderCache),
                hwAccurateAlphaTest = json.optBoolean(
                    "hwAccurateAlphaTest",
                    json.optBoolean("hwAat", def.hwAccurateAlphaTest),
                ),
                skipDrawStart = json.optInt("skipDrawStart", def.skipDrawStart),
                skipDrawEnd = json.optInt("skipDrawEnd", def.skipDrawEnd),
                spinGpuReadbacks = json.optBoolean("spinGpuReadbacks", def.spinGpuReadbacks),
                spinCpuReadbacks = json.optBoolean("spinCpuReadbacks", def.spinCpuReadbacks),
                integerScaling = json.optBoolean("integerScaling", def.integerScaling),
                cropLeft = json.optInt("cropLeft", def.cropLeft),
                displayZoom = json.optInt("displayZoom", def.displayZoom),
                cropTop = json.optInt("cropTop", def.cropTop),
                cropRight = json.optInt("cropRight", def.cropRight),
                cropBottom = json.optInt("cropBottom", def.cropBottom),
                dithering = json.optInt("dithering", def.dithering),
                vsyncQueueSize = json.optInt("vsyncQueueSize", def.vsyncQueueSize),
                autoFlushSw = json.optBoolean("autoFlushSw", def.autoFlushSw),
                mipmapSw = json.optBoolean("mipmapSw", def.mipmapSw),
                swThreads = json.optInt("swThreads", def.swThreads),
                swThreadsHeight = json.optInt("swThreadsHeight", def.swThreadsHeight),
                aspectRatio = json.optInt("aspectRatio", def.aspectRatio),
                fmvAspectRatio = json.optInt("fmvAspectRatio", def.fmvAspectRatio),
                deinterlaceMode = json.optInt("deinterlaceMode", def.deinterlaceMode),
                dev9EthEnable = json.optBoolean("dev9EthEnable", def.dev9EthEnable),
                dev9EthApi = json.optString("dev9EthApi", def.dev9EthApi).ifEmpty { def.dev9EthApi },
                dev9EthDevice = json.optString("dev9EthDevice", def.dev9EthDevice).ifEmpty { def.dev9EthDevice },
                dev9EthLogDhcp = json.optBoolean("dev9EthLogDhcp", def.dev9EthLogDhcp),
                dev9EthLogDns = json.optBoolean("dev9EthLogDns", def.dev9EthLogDns),
                dev9InterceptDhcp = json.optBoolean("dev9InterceptDhcp", def.dev9InterceptDhcp),
                dev9Ps2Ip = json.optString("dev9Ps2Ip", def.dev9Ps2Ip).ifEmpty { def.dev9Ps2Ip },
                dev9Mask = json.optString("dev9Mask", def.dev9Mask).ifEmpty { def.dev9Mask },
                dev9Gateway = json.optString("dev9Gateway", def.dev9Gateway).ifEmpty { def.dev9Gateway },
                dev9Dns1 = json.optString("dev9Dns1", def.dev9Dns1).ifEmpty { def.dev9Dns1 },
                dev9Dns2 = json.optString("dev9Dns2", def.dev9Dns2).ifEmpty { def.dev9Dns2 },
                dev9AutoMask = json.optBoolean("dev9AutoMask", def.dev9AutoMask),
                dev9AutoGateway = json.optBoolean("dev9AutoGateway", def.dev9AutoGateway),
                dev9ModeDns1 = json.optString("dev9ModeDns1", def.dev9ModeDns1).ifEmpty { def.dev9ModeDns1 },
                dev9ModeDns2 = json.optString("dev9ModeDns2", def.dev9ModeDns2).ifEmpty { def.dev9ModeDns2 },
                dev9EthHosts = json.optJSONArray("dev9EthHosts")?.let { arr ->
                    (0 until arr.length()).mapNotNull { idx ->
                        arr.optJSONObject(idx)?.let { o ->
                            Dev9HostMapping(
                                url = o.optString("url", ""),
                                ip = o.optString("ip", "0.0.0.0").ifEmpty { "0.0.0.0" },
                                enabled = o.optBoolean("enabled", true),
                            )
                        }
                    }.filter { it.url.isNotBlank() }
                } ?: def.dev9EthHosts,
                dev9HddEnable = json.optBoolean("dev9HddEnable", def.dev9HddEnable),
                dev9HddFile = json.optString("dev9HddFile", def.dev9HddFile).ifEmpty { def.dev9HddFile },
                memoryCardSlot1Enabled = json.optBoolean("memoryCardSlot1Enabled", def.memoryCardSlot1Enabled),
                memoryCardSlot1Filename = json.optString("memoryCardSlot1Filename", def.memoryCardSlot1Filename).ifEmpty { def.memoryCardSlot1Filename },
                biosFilename = json.optString("biosFilename", def.biosFilename),
                memoryCardSlot2Enabled = json.optBoolean("memoryCardSlot2Enabled", def.memoryCardSlot2Enabled),
                memoryCardSlot2Filename = json.optString("memoryCardSlot2Filename", def.memoryCardSlot2Filename).ifEmpty { def.memoryCardSlot2Filename },
                usbKeyboard = json.optBoolean("usbKeyboard", def.usbKeyboard),
                recEE = json.optBoolean("recEE", def.recEE),
                recIOP = json.optBoolean("recIOP", def.recIOP),
                recVU0 = json.optBoolean("recVU0", def.recVU0),
                recVU1 = json.optBoolean("recVU1", def.recVU1),
                enableFastmem = json.optBoolean("enableFastmem", def.enableFastmem),
                useMacEE = true,
                useMacIOP = true,
                useMacVU0 = true,
                useMacVU1 = true,
                vu1InlineFmacStall = json.optBoolean("vu1InlineFmacStall", def.vu1InlineFmacStall),
                vu1CrossBlockPState = json.optBoolean("vu1CrossBlockPState", def.vu1CrossBlockPState),
                vu1InlineDrainTestPipes = json.optBoolean("vu1InlineDrainTestPipes", def.vu1InlineDrainTestPipes),
                vu1FmacInstanceRouting = json.optBoolean("vu1FmacInstanceRouting", def.vu1FmacInstanceRouting),
                hwMipmap = json.optBoolean("hwMipmap", def.hwMipmap),
                accurateBlendingUnit = json.optInt("accurateBlendingUnit", def.accurateBlendingUnit),
                textureFiltering = json.optInt("textureFiltering", def.textureFiltering),
                displayBilinear = json.optInt("displayBilinear", def.displayBilinear),
                texturePreloading = json.optInt("texturePreloading", def.texturePreloading),
                hardwareDownloadMode = json.optInt("hardwareDownloadMode", def.hardwareDownloadMode),
                tvShader = json.optInt("tvShader", def.tvShader),
                shadeBoost = json.optBoolean("shadeBoost", def.shadeBoost),
                shadeBoostBrightness = json.optInt("shadeBoostBrightness", def.shadeBoostBrightness),
                shadeBoostContrast = json.optInt("shadeBoostContrast", def.shadeBoostContrast),
                shadeBoostSaturation = json.optInt("shadeBoostSaturation", def.shadeBoostSaturation),
                shadeBoostGamma = json.optInt("shadeBoostGamma", def.shadeBoostGamma),
                fxaa = json.optBoolean("fxaa", def.fxaa),
                shaderChainEnabled = json.optBoolean("shaderChainEnabled", def.shaderChainEnabled),
                shaderChainPreset = json.optString("shaderChainPreset", def.shaderChainPreset),
                shaderChainParams = json.optJSONObject("shaderChainParams")
                    ?.let { shaderChainParamsFromJson(it) } ?: def.shaderChainParams,
                casMode = json.optInt("casMode", def.casMode),
                casSharpness = json.optInt("casSharpness", def.casSharpness),
                loadTextureReplacements = json.optBoolean("loadTextureReplacements", def.loadTextureReplacements),
                loadTextureReplacementsAsync = json.optBoolean("loadTextureReplacementsAsync", def.loadTextureReplacementsAsync),
                precacheTextureReplacements = json.optBoolean("precacheTextureReplacements", def.precacheTextureReplacements),
                dumpReplaceableTextures = json.optBoolean("dumpReplaceableTextures", def.dumpReplaceableTextures),
                osdShowTextureReplacements = json.optBoolean("osdShowTextureReplacements", def.osdShowTextureReplacements),
                osdShowFps = json.optBoolean("osdShowFps", def.osdShowFps),
                osdScale = json.optInt("osdScale", def.osdScale),
                osdColor = json.optInt("osdColor", def.osdColor),
                vsyncEnable = json.optBoolean("vsyncEnable", def.vsyncEnable),
                osdShowVps = json.optBoolean("osdShowVps", def.osdShowVps),
                osdShowSpeed = json.optBoolean("osdShowSpeed", def.osdShowSpeed),
                osdShowCpu = json.optBoolean("osdShowCpu", def.osdShowCpu),
                osdShowGpu = json.optBoolean("osdShowGpu", def.osdShowGpu),
                osdShowResolution = json.optBoolean("osdShowResolution", def.osdShowResolution),
                osdShowGsStats = json.optBoolean("osdShowGsStats", def.osdShowGsStats),
                osdShowFrameTimes = json.optBoolean("osdShowFrameTimes", def.osdShowFrameTimes),
                osdShowHardwareInfo = json.optBoolean("osdShowHardwareInfo", def.osdShowHardwareInfo),
                osdShowMessages = json.optBoolean("osdShowMessages", def.osdShowMessages),
                osdShowGpuStats = json.optBoolean("osdShowGpuStats", def.osdShowGpuStats),
                osdShowVersion = json.optBoolean("osdShowVersion", def.osdShowVersion),
                osdShowSettings = json.optBoolean("osdShowSettings", def.osdShowSettings),
                osdShowInputs = json.optBoolean("osdShowInputs", def.osdShowInputs),
                autoFlush = json.optInt("autoFlush", def.autoFlush),
                halfPixelOffset = json.optInt("halfPixelOffset", def.halfPixelOffset),
                limit24BitDepth = json.optInt("limit24BitDepth", def.limit24BitDepth),
                manualUserHacks = json.optBoolean("manualUserHacks", def.manualUserHacks),
                textureInsideRt = json.optInt("textureInsideRt", def.textureInsideRt),
                nativeScaling = json.optInt("nativeScaling", def.nativeScaling),
                roundSprite = json.optInt("roundSprite", def.roundSprite),
                bilinearUpscale = json.optInt("bilinearUpscale", def.bilinearUpscale),
                gpuTargetClut = json.optInt("gpuTargetClut", def.gpuTargetClut),
                cpuSpriteRenderBw = json.optInt("cpuSpriteRenderBw", def.cpuSpriteRenderBw),
                cpuSpriteRenderLevel = json.optInt("cpuSpriteRenderLevel", def.cpuSpriteRenderLevel),
                alignSprite = json.optBoolean("alignSprite", def.alignSprite),
                mergeSprite = json.optBoolean("mergeSprite", def.mergeSprite),
                forceEvenSpritePosition = json.optBoolean("forceEvenSpritePosition", def.forceEvenSpritePosition),
                unscaledPaletteDraw = json.optBoolean("unscaledPaletteDraw", def.unscaledPaletteDraw),
                textureOffsetX = json.optInt("textureOffsetX", def.textureOffsetX),
                textureOffsetY = json.optInt("textureOffsetY", def.textureOffsetY),
                gpuPaletteConversion = json.optBoolean("gpuPaletteConversion", def.gpuPaletteConversion),
                cpuFramebufferConversion = json.optBoolean("cpuFramebufferConversion", def.cpuFramebufferConversion),
                readTargetsWhenClosing = json.optBoolean("readTargetsWhenClosing", def.readTargetsWhenClosing),
                disableDepthEmulation = json.optBoolean("disableDepthEmulation", def.disableDepthEmulation),
                disablePartialInvalidation = json.optBoolean("disablePartialInvalidation", def.disablePartialInvalidation),
                disableSafeFeatures = json.optBoolean("disableSafeFeatures", def.disableSafeFeatures),
                disableRenderFixes = json.optBoolean("disableRenderFixes", def.disableRenderFixes),
                preloadFrameData = json.optBoolean("preloadFrameData", def.preloadFrameData),
                estimateTextureRegion = json.optBoolean("estimateTextureRegion", def.estimateTextureRegion),
                drawBuffering = json.optBoolean("drawBuffering", def.drawBuffering),
                cpuClutRender = json.optInt("cpuClutRender", def.cpuClutRender),
                triFilter = json.optInt("triFilter", def.triFilter),
                maxAnisotropy = json.optInt("maxAnisotropy", def.maxAnisotropy),
                gpuProfile = json.optInt("gpuProfile", def.gpuProfile),
            )
        }

        /** Treat any field present in [overrides] as a delta over [base]. */
        /**
         * Compute the sparse override JSON between two Settings: returns
         * only fields where `current` differs from `base`. Used by the
         * overlay's per-game save path so we only persist what the user
         * actually changed for this title — global tweaks still flow
         * through fields the user hasn't touched. Mirrors the field set
         * of [merge] above (must stay in sync).
         */
        fun diff(base: Settings, current: Settings): JSONObject {
            val j = JSONObject()
            if (current.eeCycleRate         != base.eeCycleRate)         j.put("eeCycleRate", current.eeCycleRate)
            if (current.eeCycleSkip         != base.eeCycleSkip)         j.put("eeCycleSkip", current.eeCycleSkip)
            if (current.eeClampMode         != base.eeClampMode)         j.put("eeClampMode", current.eeClampMode)
            if (current.vuClampMode         != base.vuClampMode)         j.put("vuClampMode", current.vuClampMode)
            if (current.mtvu                != base.mtvu)                j.put("mtvu", current.mtvu)
            if (current.vu1Instant          != base.vu1Instant)          j.put("vu1Instant", current.vu1Instant)
            if (current.vuFlagHack          != base.vuFlagHack)          j.put("vuFlagHack", current.vuFlagHack)
            if (current.fastCDVD            != base.fastCDVD)            j.put("fastCDVD", current.fastCDVD)
            if (current.intcStat            != base.intcStat)            j.put("intcStat", current.intcStat)
            if (current.waitLoop            != base.waitLoop)            j.put("waitLoop", current.waitLoop)
            if (current.vuNeonFusions       != base.vuNeonFusions)       j.put("vuNeonFusions", current.vuNeonFusions)
            if (current.vuDeferredWrites    != base.vuDeferredWrites)    j.put("vuDeferredWrites", current.vuDeferredWrites)
            if (current.vuSkipStallSim      != base.vuSkipStallSim)      j.put("vuSkipStallSim", current.vuSkipStallSim)
            if (current.frameLimitEnable    != base.frameLimitEnable)    j.put("frameLimitEnable", current.frameLimitEnable)
            if (current.nominalSpeedPercent != base.nominalSpeedPercent) j.put("nominalSpeedPercent", current.nominalSpeedPercent)
            if (current.fpsLimit            != base.fpsLimit)            j.put("fpsLimit", current.fpsLimit)
            if (current.frameSkip != base.frameSkip) j.put("frameSkip", current.frameSkip)
            if (current.audioVolume != base.audioVolume) j.put("audioVolume", current.audioVolume)
            if (current.audioMuted != base.audioMuted) j.put("audioMuted", current.audioMuted)
            if (current.audioSwapChannels != base.audioSwapChannels) j.put("audioSwapChannels", current.audioSwapChannels)
            if (current.audioTimeStretch != base.audioTimeStretch) j.put("audioTimeStretch", current.audioTimeStretch)
            if (current.audioBufferMs != base.audioBufferMs) j.put("audioBufferMs", current.audioBufferMs)
            if (current.audioOutputLatencyMs != base.audioOutputLatencyMs) j.put("audioOutputLatencyMs", current.audioOutputLatencyMs)
            if (current.audioFastForwardVolume != base.audioFastForwardVolume) j.put("audioFastForwardVolume", current.audioFastForwardVolume)
            if (current.spu2NeonReverb != base.spu2NeonReverb) j.put("spu2NeonReverb", current.spu2NeonReverb)
            if (current.audioOpenSLES != base.audioOpenSLES) j.put("audioOpenSLES", current.audioOpenSLES)
            if (current.spu2LightweightMix != base.spu2LightweightMix) j.put("spu2LightweightMix", current.spu2LightweightMix)
            if (current.renderer != base.renderer) j.put("renderer", current.renderer)
            if (current.upscaleFloat != base.upscaleFloat) j.put("upscaleFloat", current.upscaleFloat.toDouble())
            if (current.customDriverId != base.customDriverId) j.put("customDriverId", current.customDriverId)
            if (current.orientation != base.orientation) j.put("orientation", current.orientation)
            if (current.portraitRenderTop != base.portraitRenderTop) j.put("portraitRenderTop", current.portraitRenderTop)
            if (current.framerateNtsc != base.framerateNtsc) j.put("framerateNtsc", current.framerateNtsc.toDouble())
            if (current.frameratePal != base.frameratePal) j.put("frameratePal", current.frameratePal.toDouble())
            if (current.enablePatches != base.enablePatches) j.put("enablePatches", current.enablePatches)
            if (current.enableCheats != base.enableCheats) j.put("enableCheats", current.enableCheats)
            if (current.enableWideScreenPatches != base.enableWideScreenPatches) j.put("enableWideScreenPatches", current.enableWideScreenPatches)
            if (current.enableNoInterlacingPatches != base.enableNoInterlacingPatches) j.put("enableNoInterlacingPatches", current.enableNoInterlacingPatches)
            if (current.enableFastBoot != base.enableFastBoot) j.put("enableFastBoot", current.enableFastBoot)
            if (current.hostFs != base.hostFs) j.put("hostFs", current.hostFs)
            if (current.enableGameFixes != base.enableGameFixes) j.put("enableGameFixes", current.enableGameFixes)
            if (current.gamefixSoftwareRendererFmv != base.gamefixSoftwareRendererFmv) j.put("gamefixSoftwareRendererFmv", current.gamefixSoftwareRendererFmv)
            if (current.gamefixSkipMpeg != base.gamefixSkipMpeg) j.put("gamefixSkipMpeg", current.gamefixSkipMpeg)
            if (current.gamefixEETiming != base.gamefixEETiming) j.put("gamefixEETiming", current.gamefixEETiming)
            if (current.gamefixInstantDma != base.gamefixInstantDma) j.put("gamefixInstantDma", current.gamefixInstantDma)
            if (current.gamefixBlitInternalFps != base.gamefixBlitInternalFps) j.put("gamefixBlitInternalFps", current.gamefixBlitInternalFps)
            if (current.gamefixFpuMul        != base.gamefixFpuMul)        j.put("gamefixFpuMul", current.gamefixFpuMul)
            if (current.gamefixOphFlag       != base.gamefixOphFlag)       j.put("gamefixOphFlag", current.gamefixOphFlag)
            if (current.gamefixGifFifo       != base.gamefixGifFifo)       j.put("gamefixGifFifo", current.gamefixGifFifo)
            if (current.gamefixDmaBusy       != base.gamefixDmaBusy)       j.put("gamefixDmaBusy", current.gamefixDmaBusy)
            if (current.gamefixVif1Stall     != base.gamefixVif1Stall)     j.put("gamefixVif1Stall", current.gamefixVif1Stall)
            if (current.gamefixIbit          != base.gamefixIbit)          j.put("gamefixIbit", current.gamefixIbit)
            if (current.gamefixFullVu0Sync   != base.gamefixFullVu0Sync)   j.put("gamefixFullVu0Sync", current.gamefixFullVu0Sync)
            if (current.gamefixVuAddSub      != base.gamefixVuAddSub)      j.put("gamefixVuAddSub", current.gamefixVuAddSub)
            if (current.gamefixVuOverflow    != base.gamefixVuOverflow)    j.put("gamefixVuOverflow", current.gamefixVuOverflow)
            if (current.gamefixXgkick        != base.gamefixXgkick)        j.put("gamefixXgkick", current.gamefixXgkick)
            if (current.gamefixGoemonTlb     != base.gamefixGoemonTlb)     j.put("gamefixGoemonTlb", current.gamefixGoemonTlb)
            if (current.gamefixVuSync        != base.gamefixVuSync)        j.put("gamefixVuSync", current.gamefixVuSync)
            if (current.skipDuplicateFrames  != base.skipDuplicateFrames)  j.put("skipDuplicateFrames", current.skipDuplicateFrames)
            if (current.eeFpuRoundMode       != base.eeFpuRoundMode)       j.put("eeFpuRoundMode", current.eeFpuRoundMode)
            if (current.vu0RoundMode         != base.vu0RoundMode)         j.put("vu0RoundMode", current.vu0RoundMode)
            if (current.vu1RoundMode         != base.vu1RoundMode)         j.put("vu1RoundMode", current.vu1RoundMode)
            if (current.screenOffsets        != base.screenOffsets)        j.put("screenOffsets", current.screenOffsets)
            if (current.showOverscan         != base.showOverscan)         j.put("showOverscan", current.showOverscan)
            if (current.antiBlur             != base.antiBlur)             j.put("antiBlur", current.antiBlur)
            if (current.disableInterlaceOffset != base.disableInterlaceOffset) j.put("disableInterlaceOffset", current.disableInterlaceOffset)
            if (current.syncToHostRefresh    != base.syncToHostRefresh)    j.put("syncToHostRefresh", current.syncToHostRefresh)
            if (current.disableFramebufferFetch != base.disableFramebufferFetch) j.put("disableFramebufferFetch", current.disableFramebufferFetch)
            if (current.hwRov != base.hwRov) j.put("hwRov", current.hwRov)
            if (current.hwAa1 != base.hwAa1) j.put("hwAa1", current.hwAa1)
            if (current.adrenoFbFetch != base.adrenoFbFetch) j.put("adrenoFbFetch", current.adrenoFbFetch)
            if (current.forceMaliFbFetch != base.forceMaliFbFetch) j.put("forceMaliFbFetch", current.forceMaliFbFetch)
            if (current.useAngleOpenGL != base.useAngleOpenGL) j.put("useAngleOpenGL", current.useAngleOpenGL)
            if (current.overrideTextureBarriers != base.overrideTextureBarriers) j.put("overrideTextureBarriers", current.overrideTextureBarriers)
            if (current.gsBackThreadMode != base.gsBackThreadMode) j.put("gsBackThreadMode", current.gsBackThreadMode)
            if (current.disableVertexShaderExpand != base.disableVertexShaderExpand) j.put("disableVertexShaderExpand", current.disableVertexShaderExpand)
            if (current.useBlitSwapChain     != base.useBlitSwapChain)     j.put("useBlitSwapChain", current.useBlitSwapChain)
            if (current.disableShaderCache   != base.disableShaderCache)   j.put("disableShaderCache", current.disableShaderCache)
            if (current.hwAccurateAlphaTest  != base.hwAccurateAlphaTest)  j.put("hwAccurateAlphaTest", current.hwAccurateAlphaTest)
            if (current.skipDrawStart        != base.skipDrawStart)        j.put("skipDrawStart", current.skipDrawStart)
            if (current.skipDrawEnd          != base.skipDrawEnd)          j.put("skipDrawEnd", current.skipDrawEnd)
            if (current.spinGpuReadbacks     != base.spinGpuReadbacks)     j.put("spinGpuReadbacks", current.spinGpuReadbacks)
            if (current.spinCpuReadbacks     != base.spinCpuReadbacks)     j.put("spinCpuReadbacks", current.spinCpuReadbacks)
            if (current.integerScaling       != base.integerScaling)       j.put("integerScaling", current.integerScaling)
            if (current.cropLeft             != base.cropLeft)             j.put("cropLeft", current.cropLeft)
            if (current.displayZoom          != base.displayZoom)          j.put("displayZoom", current.displayZoom)
            if (current.cropTop              != base.cropTop)              j.put("cropTop", current.cropTop)
            if (current.cropRight            != base.cropRight)            j.put("cropRight", current.cropRight)
            if (current.cropBottom           != base.cropBottom)           j.put("cropBottom", current.cropBottom)
            if (current.dithering            != base.dithering)            j.put("dithering", current.dithering)
            if (current.vsyncQueueSize       != base.vsyncQueueSize)       j.put("vsyncQueueSize", current.vsyncQueueSize)
            if (current.autoFlushSw          != base.autoFlushSw)          j.put("autoFlushSw", current.autoFlushSw)
            if (current.mipmapSw             != base.mipmapSw)             j.put("mipmapSw", current.mipmapSw)
            if (current.swThreads            != base.swThreads)            j.put("swThreads", current.swThreads)
            if (current.swThreadsHeight      != base.swThreadsHeight)      j.put("swThreadsHeight", current.swThreadsHeight)
            if (current.aspectRatio         != base.aspectRatio)         j.put("aspectRatio", current.aspectRatio)
            if (current.fmvAspectRatio      != base.fmvAspectRatio)      j.put("fmvAspectRatio", current.fmvAspectRatio)
            if (current.deinterlaceMode     != base.deinterlaceMode)     j.put("deinterlaceMode", current.deinterlaceMode)
            if (current.dev9EthEnable       != base.dev9EthEnable)       j.put("dev9EthEnable", current.dev9EthEnable)
            if (current.dev9EthApi          != base.dev9EthApi)          j.put("dev9EthApi", current.dev9EthApi)
            if (current.dev9EthDevice       != base.dev9EthDevice)       j.put("dev9EthDevice", current.dev9EthDevice)
            if (current.dev9EthLogDhcp      != base.dev9EthLogDhcp)      j.put("dev9EthLogDhcp", current.dev9EthLogDhcp)
            if (current.dev9EthLogDns       != base.dev9EthLogDns)       j.put("dev9EthLogDns", current.dev9EthLogDns)
            if (current.dev9InterceptDhcp   != base.dev9InterceptDhcp)   j.put("dev9InterceptDhcp", current.dev9InterceptDhcp)
            if (current.dev9Ps2Ip           != base.dev9Ps2Ip)           j.put("dev9Ps2Ip", current.dev9Ps2Ip)
            if (current.dev9Mask            != base.dev9Mask)            j.put("dev9Mask", current.dev9Mask)
            if (current.dev9Gateway         != base.dev9Gateway)         j.put("dev9Gateway", current.dev9Gateway)
            if (current.dev9Dns1            != base.dev9Dns1)            j.put("dev9Dns1", current.dev9Dns1)
            if (current.dev9Dns2            != base.dev9Dns2)            j.put("dev9Dns2", current.dev9Dns2)
            if (current.dev9AutoMask        != base.dev9AutoMask)        j.put("dev9AutoMask", current.dev9AutoMask)
            if (current.dev9AutoGateway     != base.dev9AutoGateway)     j.put("dev9AutoGateway", current.dev9AutoGateway)
            if (current.dev9ModeDns1        != base.dev9ModeDns1)        j.put("dev9ModeDns1", current.dev9ModeDns1)
            if (current.dev9ModeDns2        != base.dev9ModeDns2)        j.put("dev9ModeDns2", current.dev9ModeDns2)
            if (current.dev9EthHosts        != base.dev9EthHosts) {
                j.put("dev9EthHosts", JSONArray().apply {
                    current.dev9EthHosts.forEach { host ->
                        put(JSONObject().apply {
                            put("url", host.url)
                            put("ip", host.ip)
                            put("enabled", host.enabled)
                        })
                    }
                })
            }
            if (current.dev9HddEnable       != base.dev9HddEnable)       j.put("dev9HddEnable", current.dev9HddEnable)
            if (current.dev9HddFile         != base.dev9HddFile)         j.put("dev9HddFile", current.dev9HddFile)
            if (current.memoryCardSlot1Enabled != base.memoryCardSlot1Enabled) j.put("memoryCardSlot1Enabled", current.memoryCardSlot1Enabled)
            if (current.memoryCardSlot1Filename != base.memoryCardSlot1Filename) j.put("memoryCardSlot1Filename", current.memoryCardSlot1Filename)
            if (current.biosFilename != base.biosFilename) j.put("biosFilename", current.biosFilename)
            if (current.memoryCardSlot2Enabled != base.memoryCardSlot2Enabled) j.put("memoryCardSlot2Enabled", current.memoryCardSlot2Enabled)
            if (current.memoryCardSlot2Filename != base.memoryCardSlot2Filename) j.put("memoryCardSlot2Filename", current.memoryCardSlot2Filename)
            if (current.usbKeyboard         != base.usbKeyboard)         j.put("usbKeyboard", current.usbKeyboard)
            if (current.recEE               != base.recEE)               j.put("recEE", current.recEE)
            if (current.recIOP              != base.recIOP)              j.put("recIOP", current.recIOP)
            if (current.recVU0              != base.recVU0)              j.put("recVU0", current.recVU0)
            if (current.recVU1              != base.recVU1)              j.put("recVU1", current.recVU1)
            if (current.enableFastmem       != base.enableFastmem)       j.put("enableFastmem", current.enableFastmem)
            if (current.vu1InlineFmacStall  != base.vu1InlineFmacStall)  j.put("vu1InlineFmacStall", current.vu1InlineFmacStall)
            if (current.vu1CrossBlockPState != base.vu1CrossBlockPState) j.put("vu1CrossBlockPState", current.vu1CrossBlockPState)
            if (current.vu1InlineDrainTestPipes != base.vu1InlineDrainTestPipes) j.put("vu1InlineDrainTestPipes", current.vu1InlineDrainTestPipes)
            if (current.vu1FmacInstanceRouting != base.vu1FmacInstanceRouting) j.put("vu1FmacInstanceRouting", current.vu1FmacInstanceRouting)
            if (current.hwMipmap            != base.hwMipmap)            j.put("hwMipmap", current.hwMipmap)
            if (current.accurateBlendingUnit!= base.accurateBlendingUnit)j.put("accurateBlendingUnit", current.accurateBlendingUnit)
            if (current.textureFiltering    != base.textureFiltering)    j.put("textureFiltering", current.textureFiltering)
            if (current.displayBilinear     != base.displayBilinear)     j.put("displayBilinear", current.displayBilinear)
            if (current.texturePreloading   != base.texturePreloading)   j.put("texturePreloading", current.texturePreloading)
            if (current.hardwareDownloadMode!= base.hardwareDownloadMode)j.put("hardwareDownloadMode", current.hardwareDownloadMode)
            if (current.tvShader            != base.tvShader)            j.put("tvShader", current.tvShader)
            if (current.shadeBoost          != base.shadeBoost)          j.put("shadeBoost", current.shadeBoost)
            if (current.shadeBoostBrightness != base.shadeBoostBrightness) j.put("shadeBoostBrightness", current.shadeBoostBrightness)
            if (current.shadeBoostContrast  != base.shadeBoostContrast)  j.put("shadeBoostContrast", current.shadeBoostContrast)
            if (current.shadeBoostSaturation != base.shadeBoostSaturation) j.put("shadeBoostSaturation", current.shadeBoostSaturation)
            if (current.shadeBoostGamma     != base.shadeBoostGamma)     j.put("shadeBoostGamma", current.shadeBoostGamma)
            if (current.fxaa                != base.fxaa)                j.put("fxaa", current.fxaa)
            if (current.shaderChainEnabled  != base.shaderChainEnabled)  j.put("shaderChainEnabled", current.shaderChainEnabled)
            if (current.shaderChainPreset   != base.shaderChainPreset)   j.put("shaderChainPreset", current.shaderChainPreset)
            if (current.shaderChainParams   != base.shaderChainParams)   j.put("shaderChainParams", shaderChainParamsToJson(current.shaderChainParams))
            if (current.casMode             != base.casMode)             j.put("casMode", current.casMode)
            if (current.casSharpness        != base.casSharpness)        j.put("casSharpness", current.casSharpness)
            if (current.loadTextureReplacements != base.loadTextureReplacements) j.put("loadTextureReplacements", current.loadTextureReplacements)
            if (current.loadTextureReplacementsAsync != base.loadTextureReplacementsAsync) j.put("loadTextureReplacementsAsync", current.loadTextureReplacementsAsync)
            if (current.precacheTextureReplacements != base.precacheTextureReplacements) j.put("precacheTextureReplacements", current.precacheTextureReplacements)
            if (current.dumpReplaceableTextures != base.dumpReplaceableTextures) j.put("dumpReplaceableTextures", current.dumpReplaceableTextures)
            if (current.osdShowTextureReplacements != base.osdShowTextureReplacements) j.put("osdShowTextureReplacements", current.osdShowTextureReplacements)
            if (current.osdShowFps != base.osdShowFps) j.put("osdShowFps", current.osdShowFps)
            if (current.osdScale != base.osdScale) j.put("osdScale", current.osdScale)
            if (current.osdColor != base.osdColor) j.put("osdColor", current.osdColor)
            if (current.vsyncEnable != base.vsyncEnable) j.put("vsyncEnable", current.vsyncEnable)
            if (current.osdShowVps != base.osdShowVps) j.put("osdShowVps", current.osdShowVps)
            if (current.osdShowSpeed != base.osdShowSpeed) j.put("osdShowSpeed", current.osdShowSpeed)
            if (current.osdShowCpu != base.osdShowCpu) j.put("osdShowCpu", current.osdShowCpu)
            if (current.osdShowGpu != base.osdShowGpu) j.put("osdShowGpu", current.osdShowGpu)
            if (current.osdShowResolution != base.osdShowResolution) j.put("osdShowResolution", current.osdShowResolution)
            if (current.osdShowGsStats != base.osdShowGsStats) j.put("osdShowGsStats", current.osdShowGsStats)
            if (current.osdShowFrameTimes != base.osdShowFrameTimes) j.put("osdShowFrameTimes", current.osdShowFrameTimes)
            if (current.osdShowHardwareInfo != base.osdShowHardwareInfo) j.put("osdShowHardwareInfo", current.osdShowHardwareInfo)
            if (current.osdShowMessages != base.osdShowMessages) j.put("osdShowMessages", current.osdShowMessages)
            if (current.osdShowGpuStats != base.osdShowGpuStats) j.put("osdShowGpuStats", current.osdShowGpuStats)
            if (current.osdShowVersion != base.osdShowVersion) j.put("osdShowVersion", current.osdShowVersion)
            if (current.osdShowSettings != base.osdShowSettings) j.put("osdShowSettings", current.osdShowSettings)
            if (current.osdShowInputs != base.osdShowInputs) j.put("osdShowInputs", current.osdShowInputs)
            if (current.autoFlush           != base.autoFlush)           j.put("autoFlush", current.autoFlush)
            if (current.halfPixelOffset     != base.halfPixelOffset)     j.put("halfPixelOffset", current.halfPixelOffset)
            if (current.limit24BitDepth     != base.limit24BitDepth)     j.put("limit24BitDepth", current.limit24BitDepth)
            if (current.manualUserHacks     != base.manualUserHacks)     j.put("manualUserHacks", current.manualUserHacks)
            if (current.textureInsideRt     != base.textureInsideRt)     j.put("textureInsideRt", current.textureInsideRt)
            if (current.nativeScaling       != base.nativeScaling)       j.put("nativeScaling", current.nativeScaling)
            if (current.roundSprite         != base.roundSprite)         j.put("roundSprite", current.roundSprite)
            if (current.bilinearUpscale     != base.bilinearUpscale)     j.put("bilinearUpscale", current.bilinearUpscale)
            if (current.gpuTargetClut       != base.gpuTargetClut)       j.put("gpuTargetClut", current.gpuTargetClut)
            if (current.cpuSpriteRenderBw   != base.cpuSpriteRenderBw)   j.put("cpuSpriteRenderBw", current.cpuSpriteRenderBw)
            if (current.cpuSpriteRenderLevel != base.cpuSpriteRenderLevel) j.put("cpuSpriteRenderLevel", current.cpuSpriteRenderLevel)
            if (current.alignSprite         != base.alignSprite)         j.put("alignSprite", current.alignSprite)
            if (current.mergeSprite         != base.mergeSprite)         j.put("mergeSprite", current.mergeSprite)
            if (current.forceEvenSpritePosition != base.forceEvenSpritePosition) j.put("forceEvenSpritePosition", current.forceEvenSpritePosition)
            if (current.unscaledPaletteDraw != base.unscaledPaletteDraw) j.put("unscaledPaletteDraw", current.unscaledPaletteDraw)
            if (current.textureOffsetX      != base.textureOffsetX)      j.put("textureOffsetX", current.textureOffsetX)
            if (current.textureOffsetY      != base.textureOffsetY)      j.put("textureOffsetY", current.textureOffsetY)
            if (current.gpuPaletteConversion != base.gpuPaletteConversion) j.put("gpuPaletteConversion", current.gpuPaletteConversion)
            if (current.cpuFramebufferConversion != base.cpuFramebufferConversion) j.put("cpuFramebufferConversion", current.cpuFramebufferConversion)
            if (current.readTargetsWhenClosing != base.readTargetsWhenClosing) j.put("readTargetsWhenClosing", current.readTargetsWhenClosing)
            if (current.disableDepthEmulation != base.disableDepthEmulation) j.put("disableDepthEmulation", current.disableDepthEmulation)
            if (current.disablePartialInvalidation != base.disablePartialInvalidation) j.put("disablePartialInvalidation", current.disablePartialInvalidation)
            if (current.disableSafeFeatures != base.disableSafeFeatures) j.put("disableSafeFeatures", current.disableSafeFeatures)
            if (current.disableRenderFixes  != base.disableRenderFixes)  j.put("disableRenderFixes", current.disableRenderFixes)
            if (current.preloadFrameData    != base.preloadFrameData)    j.put("preloadFrameData", current.preloadFrameData)
            if (current.estimateTextureRegion != base.estimateTextureRegion) j.put("estimateTextureRegion", current.estimateTextureRegion)
            if (current.drawBuffering        != base.drawBuffering)        j.put("drawBuffering", current.drawBuffering)
            if (current.cpuClutRender       != base.cpuClutRender)       j.put("cpuClutRender", current.cpuClutRender)
            if (current.triFilter           != base.triFilter)           j.put("triFilter", current.triFilter)
            if (current.maxAnisotropy       != base.maxAnisotropy)       j.put("maxAnisotropy", current.maxAnisotropy)
            if (current.gpuProfile          != base.gpuProfile)          j.put("gpuProfile", current.gpuProfile)
            return j
        }

        fun merge(base: Settings, overrides: JSONObject): Settings = Settings(
            eeCycleRate = if (overrides.has("eeCycleRate")) overrides.getInt("eeCycleRate") else base.eeCycleRate,
            eeCycleSkip = if (overrides.has("eeCycleSkip")) overrides.getInt("eeCycleSkip") else base.eeCycleSkip,
            eeClampMode = if (overrides.has("eeClampMode")) overrides.getInt("eeClampMode") else base.eeClampMode,
            vuClampMode = if (overrides.has("vuClampMode")) overrides.getInt("vuClampMode") else base.vuClampMode,
            mtvu = if (overrides.has("mtvu")) overrides.getBoolean("mtvu") else base.mtvu,
            vu1Instant = if (overrides.has("vu1Instant")) overrides.getBoolean("vu1Instant") else base.vu1Instant,
            vuFlagHack = if (overrides.has("vuFlagHack")) overrides.getBoolean("vuFlagHack") else base.vuFlagHack,
            fastCDVD = if (overrides.has("fastCDVD")) overrides.getBoolean("fastCDVD") else base.fastCDVD,
            intcStat = if (overrides.has("intcStat")) overrides.getBoolean("intcStat") else base.intcStat,
            waitLoop = if (overrides.has("waitLoop")) overrides.getBoolean("waitLoop") else base.waitLoop,
            vuNeonFusions = if (overrides.has("vuNeonFusions")) overrides.getBoolean("vuNeonFusions") else base.vuNeonFusions,
            vuDeferredWrites = if (overrides.has("vuDeferredWrites")) overrides.getBoolean("vuDeferredWrites") else base.vuDeferredWrites,
            vuSkipStallSim = if (overrides.has("vuSkipStallSim")) overrides.getBoolean("vuSkipStallSim") else base.vuSkipStallSim,
            frameLimitEnable = if (overrides.has("frameLimitEnable")) overrides.getBoolean("frameLimitEnable") else base.frameLimitEnable,
            nominalSpeedPercent = if (overrides.has("nominalSpeedPercent")) overrides.getInt("nominalSpeedPercent") else base.nominalSpeedPercent,
            fpsLimit = if (overrides.has("fpsLimit")) overrides.getInt("fpsLimit") else base.fpsLimit,
            frameSkip = if (overrides.has("frameSkip")) overrides.getInt("frameSkip") else base.frameSkip,
            audioVolume = if (overrides.has("audioVolume")) overrides.getInt("audioVolume") else base.audioVolume,
            audioMuted = if (overrides.has("audioMuted")) overrides.getBoolean("audioMuted") else base.audioMuted,
            audioSwapChannels = if (overrides.has("audioSwapChannels")) overrides.getBoolean("audioSwapChannels") else base.audioSwapChannels,
            audioTimeStretch = if (overrides.has("audioTimeStretch")) overrides.getBoolean("audioTimeStretch") else base.audioTimeStretch,
            audioBufferMs = if (overrides.has("audioBufferMs")) overrides.getInt("audioBufferMs") else base.audioBufferMs,
            audioOutputLatencyMs = if (overrides.has("audioOutputLatencyMs")) overrides.getInt("audioOutputLatencyMs") else base.audioOutputLatencyMs,
            audioFastForwardVolume = if (overrides.has("audioFastForwardVolume")) overrides.getInt("audioFastForwardVolume") else base.audioFastForwardVolume,
            spu2NeonReverb = if (overrides.has("spu2NeonReverb")) overrides.getBoolean("spu2NeonReverb") else base.spu2NeonReverb,
            audioOpenSLES = if (overrides.has("audioOpenSLES")) overrides.getBoolean("audioOpenSLES") else base.audioOpenSLES,
            spu2LightweightMix = if (overrides.has("spu2LightweightMix")) overrides.getBoolean("spu2LightweightMix") else base.spu2LightweightMix,
            renderer = if (overrides.has("renderer")) overrides.getString("renderer") else base.renderer,
            upscaleFloat = if (overrides.has("upscaleFloat")) overrides.getDouble("upscaleFloat").toFloat() else base.upscaleFloat,
            customDriverId = if (overrides.has("customDriverId")) overrides.getString("customDriverId") else base.customDriverId,
            orientation = if (overrides.has("orientation")) overrides.getInt("orientation") else base.orientation,
            portraitRenderTop = if (overrides.has("portraitRenderTop")) overrides.getBoolean("portraitRenderTop") else base.portraitRenderTop,
            framerateNtsc = if (overrides.has("framerateNtsc")) overrides.getDouble("framerateNtsc").toFloat() else base.framerateNtsc,
            frameratePal = if (overrides.has("frameratePal")) overrides.getDouble("frameratePal").toFloat() else base.frameratePal,
            enablePatches = if (overrides.has("enablePatches")) overrides.getBoolean("enablePatches") else base.enablePatches,
            enableCheats = if (overrides.has("enableCheats")) overrides.getBoolean("enableCheats") else base.enableCheats,
            enableWideScreenPatches = if (overrides.has("enableWideScreenPatches")) overrides.getBoolean("enableWideScreenPatches") else base.enableWideScreenPatches,
            enableNoInterlacingPatches = if (overrides.has("enableNoInterlacingPatches")) overrides.getBoolean("enableNoInterlacingPatches") else base.enableNoInterlacingPatches,
            enableFastBoot = if (overrides.has("enableFastBoot")) overrides.getBoolean("enableFastBoot") else base.enableFastBoot,
            hostFs = if (overrides.has("hostFs")) overrides.getBoolean("hostFs") else base.hostFs,
            enableGameFixes = if (overrides.has("enableGameFixes")) overrides.getBoolean("enableGameFixes") else base.enableGameFixes,
            gamefixSoftwareRendererFmv = if (overrides.has("gamefixSoftwareRendererFmv")) overrides.getBoolean("gamefixSoftwareRendererFmv") else base.gamefixSoftwareRendererFmv,
            gamefixSkipMpeg = if (overrides.has("gamefixSkipMpeg")) overrides.getBoolean("gamefixSkipMpeg") else base.gamefixSkipMpeg,
            gamefixEETiming = if (overrides.has("gamefixEETiming")) overrides.getBoolean("gamefixEETiming") else base.gamefixEETiming,
            gamefixInstantDma = if (overrides.has("gamefixInstantDma")) overrides.getBoolean("gamefixInstantDma") else base.gamefixInstantDma,
            gamefixBlitInternalFps = if (overrides.has("gamefixBlitInternalFps")) overrides.getBoolean("gamefixBlitInternalFps") else base.gamefixBlitInternalFps,
            gamefixFpuMul = if (overrides.has("gamefixFpuMul")) overrides.getBoolean("gamefixFpuMul") else base.gamefixFpuMul,
            gamefixOphFlag = if (overrides.has("gamefixOphFlag")) overrides.getBoolean("gamefixOphFlag") else base.gamefixOphFlag,
            gamefixGifFifo = if (overrides.has("gamefixGifFifo")) overrides.getBoolean("gamefixGifFifo") else base.gamefixGifFifo,
            gamefixDmaBusy = if (overrides.has("gamefixDmaBusy")) overrides.getBoolean("gamefixDmaBusy") else base.gamefixDmaBusy,
            gamefixVif1Stall = if (overrides.has("gamefixVif1Stall")) overrides.getBoolean("gamefixVif1Stall") else base.gamefixVif1Stall,
            gamefixIbit = if (overrides.has("gamefixIbit")) overrides.getBoolean("gamefixIbit") else base.gamefixIbit,
            gamefixFullVu0Sync = if (overrides.has("gamefixFullVu0Sync")) overrides.getBoolean("gamefixFullVu0Sync") else base.gamefixFullVu0Sync,
            gamefixVuAddSub = if (overrides.has("gamefixVuAddSub")) overrides.getBoolean("gamefixVuAddSub") else base.gamefixVuAddSub,
            gamefixVuOverflow = if (overrides.has("gamefixVuOverflow")) overrides.getBoolean("gamefixVuOverflow") else base.gamefixVuOverflow,
            gamefixXgkick = if (overrides.has("gamefixXgkick")) overrides.getBoolean("gamefixXgkick") else base.gamefixXgkick,
            gamefixGoemonTlb = if (overrides.has("gamefixGoemonTlb")) overrides.getBoolean("gamefixGoemonTlb") else base.gamefixGoemonTlb,
            gamefixVuSync = if (overrides.has("gamefixVuSync")) overrides.getBoolean("gamefixVuSync") else base.gamefixVuSync,
            skipDuplicateFrames = if (overrides.has("skipDuplicateFrames")) overrides.getBoolean("skipDuplicateFrames") else base.skipDuplicateFrames,
            eeFpuRoundMode = if (overrides.has("eeFpuRoundMode")) overrides.getInt("eeFpuRoundMode") else base.eeFpuRoundMode,
            vu0RoundMode = if (overrides.has("vu0RoundMode")) overrides.getInt("vu0RoundMode") else base.vu0RoundMode,
            vu1RoundMode = if (overrides.has("vu1RoundMode")) overrides.getInt("vu1RoundMode") else base.vu1RoundMode,
            screenOffsets = if (overrides.has("screenOffsets")) overrides.getBoolean("screenOffsets") else base.screenOffsets,
            showOverscan = if (overrides.has("showOverscan")) overrides.getBoolean("showOverscan") else base.showOverscan,
            antiBlur = if (overrides.has("antiBlur")) overrides.getBoolean("antiBlur") else base.antiBlur,
            disableInterlaceOffset = if (overrides.has("disableInterlaceOffset")) overrides.getBoolean("disableInterlaceOffset") else base.disableInterlaceOffset,
            syncToHostRefresh = if (overrides.has("syncToHostRefresh")) overrides.getBoolean("syncToHostRefresh") else base.syncToHostRefresh,
            disableFramebufferFetch = if (overrides.has("disableFramebufferFetch")) overrides.getBoolean("disableFramebufferFetch") else base.disableFramebufferFetch,
            hwRov = if (overrides.has("hwRov")) overrides.getBoolean("hwRov") else base.hwRov,
            hwAa1 = if (overrides.has("hwAa1")) overrides.getBoolean("hwAa1") else base.hwAa1,
            hwAat = false,
            adrenoFbFetch = if (overrides.has("adrenoFbFetch")) overrides.getBoolean("adrenoFbFetch") else base.adrenoFbFetch,
            forceMaliFbFetch = if (overrides.has("forceMaliFbFetch")) overrides.getBoolean("forceMaliFbFetch") else base.forceMaliFbFetch,
            useAngleOpenGL = if (overrides.has("useAngleOpenGL")) overrides.getBoolean("useAngleOpenGL") else base.useAngleOpenGL,
            overrideTextureBarriers = if (overrides.has("overrideTextureBarriers")) overrides.getInt("overrideTextureBarriers") else base.overrideTextureBarriers,
            gsBackThreadMode = if (overrides.has("gsBackThreadMode")) overrides.getInt("gsBackThreadMode") else base.gsBackThreadMode,
            disableVertexShaderExpand = if (overrides.has("disableVertexShaderExpand")) overrides.getBoolean("disableVertexShaderExpand") else base.disableVertexShaderExpand,
            useBlitSwapChain = if (overrides.has("useBlitSwapChain")) overrides.getBoolean("useBlitSwapChain") else base.useBlitSwapChain,
            disableShaderCache = if (overrides.has("disableShaderCache")) overrides.getBoolean("disableShaderCache") else base.disableShaderCache,
            hwAccurateAlphaTest = when {
                overrides.has("hwAccurateAlphaTest") -> overrides.getBoolean("hwAccurateAlphaTest")
                overrides.has("hwAat") -> overrides.getBoolean("hwAat")
                else -> base.hwAccurateAlphaTest
            },
            skipDrawStart = if (overrides.has("skipDrawStart")) overrides.getInt("skipDrawStart") else base.skipDrawStart,
            skipDrawEnd = if (overrides.has("skipDrawEnd")) overrides.getInt("skipDrawEnd") else base.skipDrawEnd,
            spinGpuReadbacks = if (overrides.has("spinGpuReadbacks")) overrides.getBoolean("spinGpuReadbacks") else base.spinGpuReadbacks,
            spinCpuReadbacks = if (overrides.has("spinCpuReadbacks")) overrides.getBoolean("spinCpuReadbacks") else base.spinCpuReadbacks,
            integerScaling = if (overrides.has("integerScaling")) overrides.getBoolean("integerScaling") else base.integerScaling,
            cropLeft = if (overrides.has("cropLeft")) overrides.getInt("cropLeft") else base.cropLeft,
            displayZoom = if (overrides.has("displayZoom")) overrides.getInt("displayZoom") else base.displayZoom,
            cropTop = if (overrides.has("cropTop")) overrides.getInt("cropTop") else base.cropTop,
            cropRight = if (overrides.has("cropRight")) overrides.getInt("cropRight") else base.cropRight,
            cropBottom = if (overrides.has("cropBottom")) overrides.getInt("cropBottom") else base.cropBottom,
            dithering = if (overrides.has("dithering")) overrides.getInt("dithering") else base.dithering,
            vsyncQueueSize = if (overrides.has("vsyncQueueSize")) overrides.getInt("vsyncQueueSize") else base.vsyncQueueSize,
            autoFlushSw = if (overrides.has("autoFlushSw")) overrides.getBoolean("autoFlushSw") else base.autoFlushSw,
            mipmapSw = if (overrides.has("mipmapSw")) overrides.getBoolean("mipmapSw") else base.mipmapSw,
            swThreads = if (overrides.has("swThreads")) overrides.getInt("swThreads") else base.swThreads,
            swThreadsHeight = if (overrides.has("swThreadsHeight")) overrides.getInt("swThreadsHeight") else base.swThreadsHeight,
            aspectRatio = if (overrides.has("aspectRatio")) overrides.getInt("aspectRatio") else base.aspectRatio,
            fmvAspectRatio = if (overrides.has("fmvAspectRatio")) overrides.getInt("fmvAspectRatio") else base.fmvAspectRatio,
            deinterlaceMode = if (overrides.has("deinterlaceMode")) overrides.getInt("deinterlaceMode") else base.deinterlaceMode,
            dev9EthEnable = if (overrides.has("dev9EthEnable")) overrides.getBoolean("dev9EthEnable") else base.dev9EthEnable,
            dev9EthApi = if (overrides.has("dev9EthApi")) overrides.getString("dev9EthApi").ifEmpty { base.dev9EthApi } else base.dev9EthApi,
            dev9EthDevice = if (overrides.has("dev9EthDevice")) overrides.getString("dev9EthDevice").ifEmpty { base.dev9EthDevice } else base.dev9EthDevice,
            dev9EthLogDhcp = if (overrides.has("dev9EthLogDhcp")) overrides.getBoolean("dev9EthLogDhcp") else base.dev9EthLogDhcp,
            dev9EthLogDns = if (overrides.has("dev9EthLogDns")) overrides.getBoolean("dev9EthLogDns") else base.dev9EthLogDns,
            dev9InterceptDhcp = if (overrides.has("dev9InterceptDhcp")) overrides.getBoolean("dev9InterceptDhcp") else base.dev9InterceptDhcp,
            dev9Ps2Ip = if (overrides.has("dev9Ps2Ip")) overrides.getString("dev9Ps2Ip").ifEmpty { base.dev9Ps2Ip } else base.dev9Ps2Ip,
            dev9Mask = if (overrides.has("dev9Mask")) overrides.getString("dev9Mask").ifEmpty { base.dev9Mask } else base.dev9Mask,
            dev9Gateway = if (overrides.has("dev9Gateway")) overrides.getString("dev9Gateway").ifEmpty { base.dev9Gateway } else base.dev9Gateway,
            dev9Dns1 = if (overrides.has("dev9Dns1")) overrides.getString("dev9Dns1").ifEmpty { base.dev9Dns1 } else base.dev9Dns1,
            dev9Dns2 = if (overrides.has("dev9Dns2")) overrides.getString("dev9Dns2").ifEmpty { base.dev9Dns2 } else base.dev9Dns2,
            dev9AutoMask = if (overrides.has("dev9AutoMask")) overrides.getBoolean("dev9AutoMask") else base.dev9AutoMask,
            dev9AutoGateway = if (overrides.has("dev9AutoGateway")) overrides.getBoolean("dev9AutoGateway") else base.dev9AutoGateway,
            dev9ModeDns1 = if (overrides.has("dev9ModeDns1")) overrides.getString("dev9ModeDns1").ifEmpty { base.dev9ModeDns1 } else base.dev9ModeDns1,
            dev9ModeDns2 = if (overrides.has("dev9ModeDns2")) overrides.getString("dev9ModeDns2").ifEmpty { base.dev9ModeDns2 } else base.dev9ModeDns2,
            dev9EthHosts = if (overrides.has("dev9EthHosts")) {
                overrides.optJSONArray("dev9EthHosts")?.let { array ->
                    buildList {
                        repeat(array.length()) { index ->
                            array.optJSONObject(index)?.let { host ->
                                add(
                                    Dev9HostMapping(
                                        url = host.optString("url"),
                                        ip = host.optString("ip", "0.0.0.0"),
                                        enabled = host.optBoolean("enabled", true),
                                    ),
                                )
                            }
                        }
                    }
                } ?: base.dev9EthHosts
            } else base.dev9EthHosts,
            dev9HddEnable = if (overrides.has("dev9HddEnable")) overrides.getBoolean("dev9HddEnable") else base.dev9HddEnable,
            dev9HddFile = if (overrides.has("dev9HddFile")) overrides.getString("dev9HddFile").ifEmpty { base.dev9HddFile } else base.dev9HddFile,
            memoryCardSlot1Enabled = if (overrides.has("memoryCardSlot1Enabled")) overrides.getBoolean("memoryCardSlot1Enabled") else base.memoryCardSlot1Enabled,
            memoryCardSlot1Filename = if (overrides.has("memoryCardSlot1Filename")) overrides.getString("memoryCardSlot1Filename").ifEmpty { base.memoryCardSlot1Filename } else base.memoryCardSlot1Filename,
            biosFilename = if (overrides.has("biosFilename")) overrides.getString("biosFilename") else base.biosFilename,
            memoryCardSlot2Enabled = if (overrides.has("memoryCardSlot2Enabled")) overrides.getBoolean("memoryCardSlot2Enabled") else base.memoryCardSlot2Enabled,
            memoryCardSlot2Filename = if (overrides.has("memoryCardSlot2Filename")) overrides.getString("memoryCardSlot2Filename").ifEmpty { base.memoryCardSlot2Filename } else base.memoryCardSlot2Filename,
            usbKeyboard = if (overrides.has("usbKeyboard")) overrides.getBoolean("usbKeyboard") else base.usbKeyboard,
            recEE = if (overrides.has("recEE")) overrides.getBoolean("recEE") else base.recEE,
            recIOP = if (overrides.has("recIOP")) overrides.getBoolean("recIOP") else base.recIOP,
            recVU0 = if (overrides.has("recVU0")) overrides.getBoolean("recVU0") else base.recVU0,
            recVU1 = if (overrides.has("recVU1")) overrides.getBoolean("recVU1") else base.recVU1,
            enableFastmem = if (overrides.has("enableFastmem")) overrides.getBoolean("enableFastmem") else base.enableFastmem,
            useMacEE = true,
            useMacIOP = true,
            useMacVU0 = true,
            useMacVU1 = true,
            vu1InlineFmacStall = if (overrides.has("vu1InlineFmacStall")) overrides.getBoolean("vu1InlineFmacStall") else base.vu1InlineFmacStall,
            vu1CrossBlockPState = if (overrides.has("vu1CrossBlockPState")) overrides.getBoolean("vu1CrossBlockPState") else base.vu1CrossBlockPState,
            vu1InlineDrainTestPipes = if (overrides.has("vu1InlineDrainTestPipes")) overrides.getBoolean("vu1InlineDrainTestPipes") else base.vu1InlineDrainTestPipes,
            vu1FmacInstanceRouting = if (overrides.has("vu1FmacInstanceRouting")) overrides.getBoolean("vu1FmacInstanceRouting") else base.vu1FmacInstanceRouting,
            hwMipmap = if (overrides.has("hwMipmap")) overrides.getBoolean("hwMipmap") else base.hwMipmap,
            accurateBlendingUnit = if (overrides.has("accurateBlendingUnit")) overrides.getInt("accurateBlendingUnit") else base.accurateBlendingUnit,
            textureFiltering = if (overrides.has("textureFiltering")) overrides.getInt("textureFiltering") else base.textureFiltering,
            displayBilinear = if (overrides.has("displayBilinear")) overrides.getInt("displayBilinear") else base.displayBilinear,
            texturePreloading = if (overrides.has("texturePreloading")) overrides.getInt("texturePreloading") else base.texturePreloading,
            hardwareDownloadMode = if (overrides.has("hardwareDownloadMode")) overrides.getInt("hardwareDownloadMode") else base.hardwareDownloadMode,
            tvShader = if (overrides.has("tvShader")) overrides.getInt("tvShader") else base.tvShader,
            shadeBoost = if (overrides.has("shadeBoost")) overrides.getBoolean("shadeBoost") else base.shadeBoost,
            shadeBoostBrightness = if (overrides.has("shadeBoostBrightness")) overrides.getInt("shadeBoostBrightness") else base.shadeBoostBrightness,
            shadeBoostContrast = if (overrides.has("shadeBoostContrast")) overrides.getInt("shadeBoostContrast") else base.shadeBoostContrast,
            shadeBoostSaturation = if (overrides.has("shadeBoostSaturation")) overrides.getInt("shadeBoostSaturation") else base.shadeBoostSaturation,
            shadeBoostGamma = if (overrides.has("shadeBoostGamma")) overrides.getInt("shadeBoostGamma") else base.shadeBoostGamma,
            fxaa = if (overrides.has("fxaa")) overrides.getBoolean("fxaa") else base.fxaa,
            shaderChainEnabled = if (overrides.has("shaderChainEnabled")) overrides.getBoolean("shaderChainEnabled") else base.shaderChainEnabled,
            shaderChainPreset = if (overrides.has("shaderChainPreset")) overrides.getString("shaderChainPreset") else base.shaderChainPreset,
            // Replaces the global map wholesale rather than merging per parameter: a
            // per-game tweak means "this game's chain looks like THIS", and merging would
            // let a later global edit leak into a game the user had already dialled in.
            shaderChainParams = if (overrides.has("shaderChainParams")) {
                shaderChainParamsFromJson(overrides.optJSONObject("shaderChainParams"))
            } else base.shaderChainParams,
            casMode = if (overrides.has("casMode")) overrides.getInt("casMode") else base.casMode,
            casSharpness = if (overrides.has("casSharpness")) overrides.getInt("casSharpness") else base.casSharpness,
            loadTextureReplacements = if (overrides.has("loadTextureReplacements")) overrides.getBoolean("loadTextureReplacements") else base.loadTextureReplacements,
            loadTextureReplacementsAsync = if (overrides.has("loadTextureReplacementsAsync")) overrides.getBoolean("loadTextureReplacementsAsync") else base.loadTextureReplacementsAsync,
            precacheTextureReplacements = if (overrides.has("precacheTextureReplacements")) overrides.getBoolean("precacheTextureReplacements") else base.precacheTextureReplacements,
            dumpReplaceableTextures = if (overrides.has("dumpReplaceableTextures")) overrides.getBoolean("dumpReplaceableTextures") else base.dumpReplaceableTextures,
            osdShowTextureReplacements = if (overrides.has("osdShowTextureReplacements")) overrides.getBoolean("osdShowTextureReplacements") else base.osdShowTextureReplacements,
            osdShowFps = if (overrides.has("osdShowFps")) overrides.getBoolean("osdShowFps") else base.osdShowFps,
            osdScale = if (overrides.has("osdScale")) overrides.getInt("osdScale") else base.osdScale,
            osdColor = if (overrides.has("osdColor")) overrides.getInt("osdColor") else base.osdColor,
            vsyncEnable = if (overrides.has("vsyncEnable")) overrides.getBoolean("vsyncEnable") else base.vsyncEnable,
            osdShowVps = if (overrides.has("osdShowVps")) overrides.getBoolean("osdShowVps") else base.osdShowVps,
            osdShowSpeed = if (overrides.has("osdShowSpeed")) overrides.getBoolean("osdShowSpeed") else base.osdShowSpeed,
            osdShowCpu = if (overrides.has("osdShowCpu")) overrides.getBoolean("osdShowCpu") else base.osdShowCpu,
            osdShowGpu = if (overrides.has("osdShowGpu")) overrides.getBoolean("osdShowGpu") else base.osdShowGpu,
            osdShowResolution = if (overrides.has("osdShowResolution")) overrides.getBoolean("osdShowResolution") else base.osdShowResolution,
            osdShowGsStats = if (overrides.has("osdShowGsStats")) overrides.getBoolean("osdShowGsStats") else base.osdShowGsStats,
            osdShowFrameTimes = if (overrides.has("osdShowFrameTimes")) overrides.getBoolean("osdShowFrameTimes") else base.osdShowFrameTimes,
            osdShowHardwareInfo = if (overrides.has("osdShowHardwareInfo")) overrides.getBoolean("osdShowHardwareInfo") else base.osdShowHardwareInfo,
            osdShowMessages = if (overrides.has("osdShowMessages")) overrides.getBoolean("osdShowMessages") else base.osdShowMessages,
            osdShowGpuStats = if (overrides.has("osdShowGpuStats")) overrides.getBoolean("osdShowGpuStats") else base.osdShowGpuStats,
            osdShowVersion = if (overrides.has("osdShowVersion")) overrides.getBoolean("osdShowVersion") else base.osdShowVersion,
            osdShowSettings = if (overrides.has("osdShowSettings")) overrides.getBoolean("osdShowSettings") else base.osdShowSettings,
            osdShowInputs = if (overrides.has("osdShowInputs")) overrides.getBoolean("osdShowInputs") else base.osdShowInputs,
            autoFlush = if (overrides.has("autoFlush")) overrides.getInt("autoFlush") else base.autoFlush,
            halfPixelOffset = if (overrides.has("halfPixelOffset")) overrides.getInt("halfPixelOffset") else base.halfPixelOffset,
            limit24BitDepth = if (overrides.has("limit24BitDepth")) overrides.getInt("limit24BitDepth") else base.limit24BitDepth,
            manualUserHacks = if (overrides.has("manualUserHacks")) overrides.getBoolean("manualUserHacks") else base.manualUserHacks,
            textureInsideRt = if (overrides.has("textureInsideRt")) overrides.getInt("textureInsideRt") else base.textureInsideRt,
            nativeScaling = if (overrides.has("nativeScaling")) overrides.getInt("nativeScaling") else base.nativeScaling,
            roundSprite = if (overrides.has("roundSprite")) overrides.getInt("roundSprite") else base.roundSprite,
            bilinearUpscale = if (overrides.has("bilinearUpscale")) overrides.getInt("bilinearUpscale") else base.bilinearUpscale,
            gpuTargetClut = if (overrides.has("gpuTargetClut")) overrides.getInt("gpuTargetClut") else base.gpuTargetClut,
            cpuSpriteRenderBw = if (overrides.has("cpuSpriteRenderBw")) overrides.getInt("cpuSpriteRenderBw") else base.cpuSpriteRenderBw,
            cpuSpriteRenderLevel = if (overrides.has("cpuSpriteRenderLevel")) overrides.getInt("cpuSpriteRenderLevel") else base.cpuSpriteRenderLevel,
            alignSprite = if (overrides.has("alignSprite")) overrides.getBoolean("alignSprite") else base.alignSprite,
            mergeSprite = if (overrides.has("mergeSprite")) overrides.getBoolean("mergeSprite") else base.mergeSprite,
            forceEvenSpritePosition = if (overrides.has("forceEvenSpritePosition")) overrides.getBoolean("forceEvenSpritePosition") else base.forceEvenSpritePosition,
            unscaledPaletteDraw = if (overrides.has("unscaledPaletteDraw")) overrides.getBoolean("unscaledPaletteDraw") else base.unscaledPaletteDraw,
            textureOffsetX = if (overrides.has("textureOffsetX")) overrides.getInt("textureOffsetX") else base.textureOffsetX,
            textureOffsetY = if (overrides.has("textureOffsetY")) overrides.getInt("textureOffsetY") else base.textureOffsetY,
            gpuPaletteConversion = if (overrides.has("gpuPaletteConversion")) overrides.getBoolean("gpuPaletteConversion") else base.gpuPaletteConversion,
            cpuFramebufferConversion = if (overrides.has("cpuFramebufferConversion")) overrides.getBoolean("cpuFramebufferConversion") else base.cpuFramebufferConversion,
            readTargetsWhenClosing = if (overrides.has("readTargetsWhenClosing")) overrides.getBoolean("readTargetsWhenClosing") else base.readTargetsWhenClosing,
            disableDepthEmulation = if (overrides.has("disableDepthEmulation")) overrides.getBoolean("disableDepthEmulation") else base.disableDepthEmulation,
            disablePartialInvalidation = if (overrides.has("disablePartialInvalidation")) overrides.getBoolean("disablePartialInvalidation") else base.disablePartialInvalidation,
            disableSafeFeatures = if (overrides.has("disableSafeFeatures")) overrides.getBoolean("disableSafeFeatures") else base.disableSafeFeatures,
            disableRenderFixes = if (overrides.has("disableRenderFixes")) overrides.getBoolean("disableRenderFixes") else base.disableRenderFixes,
            preloadFrameData = if (overrides.has("preloadFrameData")) overrides.getBoolean("preloadFrameData") else base.preloadFrameData,
            estimateTextureRegion = if (overrides.has("estimateTextureRegion")) overrides.getBoolean("estimateTextureRegion") else base.estimateTextureRegion,
            drawBuffering = if (overrides.has("drawBuffering")) overrides.getBoolean("drawBuffering") else base.drawBuffering,
            cpuClutRender = if (overrides.has("cpuClutRender")) overrides.getInt("cpuClutRender") else base.cpuClutRender,
            triFilter = if (overrides.has("triFilter")) overrides.getInt("triFilter") else base.triFilter,
            maxAnisotropy = if (overrides.has("maxAnisotropy")) overrides.getInt("maxAnisotropy") else base.maxAnisotropy,
            gpuProfile = if (overrides.has("gpuProfile")) overrides.getInt("gpuProfile") else base.gpuProfile,
        )
    }
}
