# iOS UI Polish Pass — Design Spec

**Date:** 2026-07-12
**Branch:** `ios/bring-up`
**Scope:** Visual polish across 8 Swift files — no new design systems, no structural rewrites.

## Motivation

Tester feedback and a UI audit identified visual inconsistencies and accessibility
gaps across the iOS frontend. The in-game overlay system (OverlayTheme) is polished
and premium, but the menu tabs and game library feel like stock SwiftUI by comparison.
This pass raises the baseline without redesigning anything.

## Documentation basis

- **WCAG 2.2 SC 1.4.3:** Normal text requires ≥4.5:1 contrast at Level AA. Current
  `textSecondary` (#9AA0AC) on `card` (#262A33) measures ~4.0:1 — fails. Fixing this
  is a compliance fix, not cosmetic.
- **Apple WWDC25 Session 356 (Liquid Glass):** "Remove background colors from custom
  toolbars and tab bars." On iOS 26 the floating tab bar adapts to content
  automatically. Our `.toolbarBackground(.visible, for: .tabBar)` should only apply
  on iOS < 26.
- **SwiftUI Expert Skill (Context7):** `@ScaledMetric` for non-text dimensions
  (padding, spacing, icon sizes) so they scale with Dynamic Type.
- **Apple HIG:** 44pt minimum tap target. The list-row favorite star currently lacks
  this.
- **Metal Best Practices:** `nextDrawable()` should be called as late as possible.
  Validates removal of drawable-wait instrumentation (already done in audit).

## Changes

### 1. Game library cards (`GameListView.swift`)

**1a. Center card text (HIGH)**
Change `gameGridCard` (line ~797) text alignment from `.leading` to `.center`:
- VStack alignment: `.leading` → `.center`
- Title: `.multilineTextAlignment(.leading)` → `.center`, frame alignment → `.center`
- Metadata HStack: wrap in `.frame(maxWidth: .infinity, alignment: .center)`
- Rationale: the cover art is already centered; left-aligned text below it looks
  unbalanced. Standard iOS grid cards (App Store, Apple Arcade) center text.

**1b. Card padding (MEDIUM)**
- Replace hardcoded `.padding(8)` with `@ScaledMetric private var cardPadding: CGFloat = 12`
  so padding grows with Dynamic Type.
- Rationale: 8pt is tight; 12pt gives breathing room and scales for accessibility.

**1c. Favorite star refinement (MEDIUM)**
- Grid star: `.padding(8)` → `.padding(6)`, `.black.opacity(0.48)` → `.black.opacity(0.36)`
- Rationale: the current dark circle is visually heavy; smaller + lighter is less
  intrusive over cover art.

**1d. BIOS Only button prominence (MEDIUM)**
- Change from `.font(.caption)` to `.font(.callout)` (line ~364)
- Rationale: it's a primary action but currently the smallest toolbar item.

**1e. List-row favorite star tap target (MEDIUM)**
- Add `.frame(width: 44, height: 44)` content shape to the list-row star button
- Rationale: currently just the glyph (~20pt), below Apple's 44pt minimum.

### 2. Game screen (`GameScreenView.swift`)

**2a. Portrait game viewport proportions (HIGH)**
- Change `geo.size.height * 0.55` to `geo.size.height * 0.6` (line ~228)
- Rationale: 55% leaves excessive dead space in the controller area; 60% gives
  more screen to the game while still leaving a comfortable controller deck.

**2b. Menu button icon (MEDIUM)**
- Change from `ellipsis.circle.fill` to `pause.circle.fill` (line ~446)
- Rationale: two bars (pause) is clearer than three dots (more options) for a
  pause-menu affordance.

**2c. Menu button padding and visibility (MEDIUM)**
- Standardize portrait padding from 4pt to 8pt (matching landscape)
- Bump background opacity from 0.28 to 0.40
- Rationale: 4pt puts it too close to the corner; 0.28 is too subtle over bright
  game content.

### 3. Overlay design system (`OverlayDesignSystem.swift`)

**3a. Fix textSecondary contrast (HIGH — WCAG compliance)**
- Change from `(0.604, 0.627, 0.674)` to `(0.66, 0.69, 0.74)` (line ~61)
- New contrast ratio on `card` (#262A33): ~5.2:1 (passes WCAG AA 4.5:1)
- Rationale: compliance fix, not cosmetic.

**3b. Frost stability (MEDIUM)**
- Bump `shellGlassTint` from 0.62 to 0.72 (line ~89)
- Rationale: 62% graphite over `.regularMaterial` can pick up color cast from
  bright game content; 72% wins more decisively.

### 4. Pause menu (`QuickMenuView.swift`)

**4a. Title truncation (LOW)**
- Switch landscape game title from `.middle` to `.tail` truncation (line ~282)
- Rationale: tail truncation ("Final Fantasy...") is more readable than middle
  ("Final...XII") for game titles.

### 5. Per-game settings (`PerGameSettingsPanel.swift`)

**5a. Unlocalized string + wrong color (MEDIUM)**
- Line ~747: wrap "Start this game once before saving its settings." in
  `settings.localized(...)`
- Change `.foregroundStyle(.orange)` to `.foregroundStyle(OverlayTheme.warm)`
- Rationale: the string is English-only in an otherwise localized panel, and `.orange`
  bypasses the design system's warning color token.

### 6. Settings root (`SettingsRootView.swift`)

**6a. Group settings into named sections (HIGH)**
- Split the flat 14-link section into:
  - **Interface:** Language, Appearance
  - **Emulation:** Emulator, Graphics, Audio
  - **Input:** Game Controller, Virtual Pad, Local Multiplayer
  - **Storage & Memory:** Memory Cards, Storage, Network
  - **Features:** RetroAchievements, Overlay
- Rationale: Apple's Settings pattern groups related items. 14 flat links is
  scannable but not organized.

**6b. Move JIT section below main settings (HIGH)**
- Move the JIT status section from the top to below the grouped settings
- Rationale: it's a developer-grade diagnostic panel; leading with it is
  intimidating for users who have JIT working.

### 7. BIOS list (`BIOSListView.swift`)

**7a. Fix monospaced prose (MEDIUM)**
- Change description text from `.font(.caption2.monospaced())` to
  `.font(.caption2)` (lines ~129, 134)
- Rationale: monospaced is appropriate for serials/CRCs, odd for prose like
  "Companion ROM or unsupported BIOS dump."

**7b. Consolidate empty-state CTAs (MEDIUM)**
- Remove the secondary "Compatibility Picker" button from the empty state
  (it's already in the toolbar overflow menu)
- Rationale: two primary-looking buttons in an empty state is confusing;
  one clear CTA is better.

### 8. Tab bar iOS 26 Liquid Glass gating (`RootView.swift`)

**8a. Gate toolbarBackground to iOS < 26 (MEDIUM)**
- Wrap `.toolbarBackground(.visible, for: .tabBar)` in
  `if #unavailable(iOS 26) { ... }` or apply conditionally
- Rationale: on iOS 26 the Liquid Glass tab bar floats and adapts automatically;
  forcing `.visible` would override the system's adaptive behavior.

## Files touched

| File | Changes |
|---|---|
| `GameListView.swift` | 1a, 1b, 1c, 1d, 1e |
| `GameScreenView.swift` | 2a, 2b, 2c |
| `OverlayDesignSystem.swift` | 3a, 3b |
| `QuickMenuView.swift` | 4a |
| `PerGameSettingsPanel.swift` | 5a |
| `SettingsRootView.swift` | 6a, 6b |
| `BIOSListView.swift` | 7a, 7b |
| `RootView.swift` | 8a |

## What is explicitly NOT changing

- The two-column grid layout (kept)
- The custom background system (kept, preserved)
- The overlay design system architecture (no new components)
- The per-game settings rotation behavior (portrait push vs. landscape rail — kept
  as-is; unifying is a larger architectural change)
- The toolbar icon set (no new icons except pause vs. ellipsis swap)
- No new dependencies, no new files
