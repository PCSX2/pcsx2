# MetalFX Spatial Upscaler — iOS Settings UI

**Date:** 2026-07-11
**Branch:** `ios/bring-up`
**Depends on:** commit `81e9d4a0cf` (iOS: enable MetalFX spatial upscaling on iOS 16+)

---

## Purpose

Expose the MetalFX Spatial upscaler — already wired through the core engine and Metal backend — as a user-facing setting in the iOS app. This is a pure UI + settings-store task: the `GSUpscaler` enum (`Off` = 0, `MetalFXSpatial` = 1), the `[EmuCore/GS] Upscaler` INI key, and the `GSRenderer → MetalFXUpscale → DoMetalFXSpatial` dispatch path are all complete and platform-neutral. No core changes are needed.

---

## Design decisions (from brainstorming)

1. **Hide when unsupported.** The entire Upscaler UI (both global and per-game) is hidden on devices where MetalFX is unavailable (pre-iOS-16, iOS Simulator, GPUs lacking the hardware). Users never see a dead option.
2. **New section after "Upscaling."** In global Graphics settings, a new `Section("Upscaler")` appears between the existing "Upscaling" (Internal Resolution) and "Filtering" sections. This keeps it visually distinct from the IR multiplier, which is a different concept.
3. **Per-game override.** A per-game Upscaler override is added to the per-game Graphics tab, using the same `-1` "Use Global" sentinel as every other per-game int setting (FXAA, CAS, TV Shader, etc.). Reachable from in-game via Quick Menu → This Game → Graphics.

---

## Component design

### 1. Bridge: `isMetalFXSupported` (new)

**File:** `platforms/ios/app/src/main/cpp/ARMSX2Bridge.h` + `ARMSX2Bridge.mm`

A new class method that wraps the C++ feature probe so Swift can query availability at runtime:

```objc
// ARMSX2Bridge.h — after the existing INI getters (around line 239)
+ (BOOL)isMetalFXSupported;
```

```objc
// ARMSX2Bridge.mm
+ (BOOL)isMetalFXSupported {
    if (@available(iOS 16.0, *)) {
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        if (!device)
            return NO;
        return [MTLFXSpatialScalerDescriptor supportsDevice:device];
    }
    return NO;
}
```

**Why not reuse `g_gs_device->Features().metalfx_spatial`?** That requires a live GS device, which doesn't exist before a game boots. The settings UI is shown on the main menu. We need a standalone probe that works without the emulator running.

**Imports needed in ARMSX2Bridge.mm:** `<Metal/Metal.h>` (already present) and `<MetalFX/MetalFX.h>` (now available on iOS after the gate removal in commit `81e9d4a0cf`).

### 2. SettingsStore: `upscaler` property + `isMetalFXAvailable`

**File:** `platforms/ios/app/src/main/swift/Models/SettingsStore.swift`

#### 2a. Availability computed property

```swift
/// MetalFX Spatial upscaling requires iOS 16+ and a device GPU that supports it.
/// Probes at runtime so the UI can hide the option on unsupported hardware.
var isMetalFXAvailable: Bool {
    ARMSX2Bridge.isMetalFXSupported()
}
```

This is a computed property (no storage, no `didSet`) — it reads the bridge every time the UI evaluates it. The bridge call is cheap (one `supportsDevice:` check).

#### 2b. `upscaler` stored property

Placed after `_tvShaderConfig` (line ~880), following the exact same pattern:

```swift
let _upscalerConfig = Setting<Int>(
    section: "EmuCore/GS", key: "Upscaler", default: 0,
    suppressible: false,
    writer: { s, k, v in ARMSX2Bridge.setINIInt(s, key: k, value: Int32(v)) },
    onSet: { _ in SettingsStore.shared.requestGraphicsApply() })
var upscaler: Int = 0 { didSet {
    guard !(_upscalerConfig.suppressible && suppressINIWrites) else { return }
    _upscalerConfig.writer(_upscalerConfig.section, _upscalerConfig.key, upscaler)
    _upscalerConfig.onSet?(upscaler)
}}
```

- INI section/key: `EmuCore/GS` / `Upscaler` (matches `Pcsx2Config.cpp:1062`).
- Default: `0` (Off). Matches `DEFAULT_UPSCALER = GSUpscaler::Off` in `Config.h:730`.
- `onSet: requestGraphicsApply()` — live-applies without VM reset, same as `tvShader`, `casMode`, `textureFiltering`.

#### 2c. Load in `init()` and `reload()`

Three locations, following the `tvShader` template:

| Location | Line (approx) | Code |
|----------|------|------|
| `init()` load block | after 1642 | `upscaler = Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "Upscaler", defaultValue: 0))` |
| `reload()` load block | after 1842 | same |
| `resetGraphicsDefaults()` | after 2253 | `upscaler = 0` |

### 3. Global Graphics settings — new `Section("Upscaler")`

**File:** `platforms/ios/app/src/main/swift/Views/Settings/GraphicsSettingsView.swift`

Inserted between the existing `Section("Upscaling")` (ends at line 93) and `Section("Filtering")` (starts at line 95):

```swift
if settings.isMetalFXAvailable {
    Section(settings.localized("Upscaler")) {
        Picker(settings.localized("Spatial Upscaler"), selection: $settings.upscaler) {
            Text(settings.localized("Off (Bilinear)")).tag(0)
            Text(settings.localized("MetalFX Spatial")).tag(1)
        }
        Text(settings.localized(
            "GPU-accelerated upscaling via MetalFX. Renders at the native PS2 "
            + "resolution and upscales to the display for sharper visuals at "
            + "lower cost than a higher internal resolution. Applies immediately."
        ))
        .font(.caption)
        .foregroundStyle(.secondary)
    }
}
```

**Visual layout in context:**

```
┌─ Renderer ───────────────────────┐
│  Renderer           Metal (HW) ▾ │
│  [Clear Shader Cache]            │
│  [explanation]                   │
├─ Upscaling ──────────────────────┤
│  Internal Resolution  1x Native ▾│
│  [explanation]                   │
├─ Upscaler ───────────────────────┤   ← NEW (only if MetalFX available)
│  Spatial Upscaler       Off    ▾ │
│  [explanation]                   │
├─ Filtering ──────────────────────┤
│  Texture Filtering  Bilinear   ▾ │
```

### 4. Per-game Graphics tab — Upscaler picker

**Files:** `PerGameSettingsPanel.swift` + `GraphicsTab.swift`

#### 4a. State + load (`PerGameSettingsPanel.swift`)

| What | Template | Line (approx) |
|------|----------|------|
| `@State private var perGameUpscaler: Int` | `perGameFXAA: Int` | after 106 |
| Load in `init` | line 248 pattern | `hasPerGameUpscaler ? ... : -1` |
| Fingerprint | line 322 | append `\|\(perGameUpscaler)` |
| GraphicsTab constructor arg | line 575 area | `perGameUpscaler: $perGameUpscaler` |
| Save | line 1106 pattern | `if enabled && perGameUpscaler != -1 { ... }` |

**Bridge dictionary builder** (`ARMSX2Bridge.mm`, around line 1786 — the `gameSettingsForISO:` method preloads a dictionary consumed by `PerGameSettingsPanel.init`). Add alongside the other `EmuCore/GS` int entries:

```objc
const bool hasPerGameUpscaler = si.ContainsValue("EmuCore/GS", "Upscaler");
result[@"hasPerGameUpscaler"] = @(hasPerGameUpscaler);
result[@"perGameUpscaler"] = @(hasPerGameUpscaler ? si.GetIntValue("EmuCore/GS", "Upscaler", 0) : 0);
```

**Swift load** (in `init`, after line 248 pattern):
```swift
let hasPerGameUpscaler = Self.boolValue(info, "hasPerGameUpscaler")
_perGameUpscaler = State(initialValue: hasPerGameUpscaler
    ? Self.intValue(info["perGameUpscaler"], defaultValue: 0)
    : -1)
```

Note: `hasPerGameUpscaler` is read from the preloaded dictionary (`info["hasPerGameUpscaler"]`), not via `hasPerGameINIValue`, matching the `perGameFXAA` pattern exactly.

**Save code** (in `savePerGameCompatibility`, near line 1106):
```swift
if enabled && perGameUpscaler != -1 {
    Self.setPerGameIntValue("EmuCore/GS", "Upscaler", perGameUpscaler, useCurrent: useCurrent, iso: iso)
} else {
    Self.clearPerGameValue("EmuCore/GS", "Upscaler", useCurrent: useCurrent, iso: iso)
}
```

#### 4b. GraphicsTab binding + picker (`GraphicsTab.swift`)

Add `@Binding var perGameUpscaler: Int` to the struct properties (after line 64).

Picker in the body, placed right after Internal Resolution (before Aspect Ratio), since it's the most resolution-adjacent setting:

```swift
if settings.isMetalFXAvailable {
    Picker(settings.localized("Spatial Upscaler"), selection: $perGameUpscaler) {
        Text(settings.localized("Use Global")).tag(-1)
        Text(settings.localized("Off")).tag(0)
        Text(settings.localized("MetalFX Spatial")).tag(1)
    }
    .disabled(!enabled)
}
```

**Note:** the per-game picker also gates on `settings.isMetalFXAvailable`, so on unsupported devices the picker is hidden entirely — consistent with the global settings.

### 5. GraphicsTab constructor call (`PerGameSettingsPanel.swift`)

The `GraphicsTab(...)` initializer call (around lines 546-603) needs the new `perGameUpscaler: $perGameUpscaler` argument added.

---

## Data flow

```
User taps "MetalFX Spatial" in picker
    ↓
$settings.upscaler = 1  (SwiftUI binding)
    ↓
SettingsStore.upscaler didSet
    ↓
_upscalerConfig.writer("EmuCore/GS", "Upscaler", 1)
    ↓
ARMSX2Bridge.setINIInt("EmuCore/GS", key: "Upscaler", value: 1)
    ↓
Host::SetBaseIntSettingValue("EmuCore/GS", "Upscaler", 1)
    ↓
_onSet → requestGraphicsApply()
    ↓
ARMSX2Bridge.applyGraphicsSettingsNow()
    ↓
VMManager::ApplySettings()  (if VM running) or queued
    ↓
GSConfig.Upscaler = GSUpscaler::MetalFXSpatial
    ↓
GSRenderer::VSync → if (GSConfig.Upscaler == MetalFXSpatial && Features().metalfx_spatial)
    ↓
g_gs_device->MetalFXUpscale(current, src_rect, src_uv, draw_rect)
    ↓
DoMetalFXSpatial(sTex, dTex)
    ↓
[MTLFXSpatialScaler encodeToCommandBuffer:...]
```

For per-game: the same flow, but through `setPerGameINIInt` / the per-game INI file. When a game boots, `VMManager` loads per-game settings which override the base `GSConfig.Upscaler`.

---

## Availability detection — edge cases

| Scenario | `isMetalFXAvailable` | UI shows | Behavior |
|----------|---------------------|----------|----------|
| iPhone 14 (iOS 17, A15) | `true` | Full Upscaler section | MetalFX works |
| iPhone SE 2 (iOS 15) | `false` | Section hidden | Bilinear fallback |
| iPhone 13 (iOS 16, A14) | `true` | Full Upscaler section | MetalFX works |
| iOS Simulator | `false` | Section hidden | `supportsDevice:` returns NO |
| Older iPad (iOS 15) | `false` | Section hidden | Bilinear fallback |

The runtime probe is authoritative — no hard-coded device list.

---

## What this design does NOT include (YAGNI)

- **No upscaling-quality slider.** MetalFX Spatial has no tunable quality parameter (unlike FSR's EASU/RCAS stages, which are handled separately by the existing CAS setting).
- **No custom upscaling scale.** MetalFX auto-determines the output size from the draw rectangle; there is no scale factor to expose.
- **No debug overlay.** The existing `GSPerfMon::TextureCopies` counter already tracks MetalFX passes.
- **No settings migration.** The default is `Off` (0), which is the existing behavior. No stored-value migration needed.

---

## Implementation order

1. **Bridge:** Add `isMetalFXSupported` to `ARMSX2Bridge.h` + `.mm`
2. **Bridge dictionary:** Add `hasPerGameUpscaler` + `perGameUpscaler` to the `gameSettingsForISO:` dictionary builder in `ARMSX2Bridge.mm`
3. **SettingsStore:** Add `isMetalFXAvailable` + `upscaler` property + load/reset
4. **Global UI:** Add `Section("Upscaler")` to `GraphicsSettingsView.swift`
5. **Per-game state:** Add `perGameUpscaler` to `PerGameSettingsPanel.swift` (state, load, fingerprint, save, constructor arg)
6. **Per-game UI:** Add binding + picker to `GraphicsTab.swift`
7. **Review + commit** as `iOS: add MetalFX spatial upscaler setting to global and per-game graphics`

Steps 1-2 are bridge changes (ObjC++). Steps 3-6 are Swift. Each step compiles independently except 5+6 which must land together (binding + constructor arg).

---

## Testing checklist

- [ ] On iOS 16+ device: Settings → Graphics → "Upscaler" section visible with Off / MetalFX Spatial picker
- [ ] Selecting MetalFX Spatial live-applies without VM restart (no "requires restart" notice needed)
- [ ] Starting a game with MetalFX on produces sharper output than bilinear (visual check)
- [ ] Per-game settings → Graphics → Spatial Upscaler picker shows Use Global / Off / MetalFX Spatial
- [ ] Per-game override persists across launches and live-applies to running game
- [ ] On iOS Simulator or pre-iOS-16: no Upscaler section appears anywhere
- [ ] Reset Graphics Defaults returns upscaler to Off
- [ ] Quick Menu → This Game → Graphics → Spatial Upscaler is reachable and functional
