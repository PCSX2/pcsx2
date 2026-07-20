package com.armsx2.input

import android.view.InputDevice

/**
 * Auto-assigns physical controllers to PS2 pad slots for local multiplayer.
 *
 *  - Multitap OFF (default): first distinct gamepad = Player 1 (slot 0); next = Player 2
 *    (slot 1, hot-plugged via [onPlayer2Joined]); any further pads fold into Player 1.
 *  - Multitap ON: up to 8 distinct gamepads claim unified slots 0..7. The unified layout
 *    matches the native engine (Sio.h): slots 0,1 = the two port mains; 2,3,4 = port-0
 *    taps; 5,6,7 = port-1 taps. Filling the next free slot yields (P1, P2, port-0 taps,
 *    port-1 taps) — the natural unified order — so no per-slot mapping is needed here.
 *
 * The tap slots (2-7) are armed natively at game boot / by the Multitap toggle, NOT from
 * this router (mirrors how [onPlayer2Joined] only hot-plugs the P2 main). The on-screen
 * touch controls and all menu navigation always use slot 0 — they never go through here.
 * Reset on VM start AND stop ([reset]) so each session re-pairs deterministically.
 *
 * Called only from the in-game input dispatch where a real `event.deviceId` is live.
 */
object PadRouter {
    // Nintendo USB/Bluetooth vendor id. A Joy-Con pair enumerates as TWO InputDevices
    // (L + R) under this vendor, but they are one physical controller — so both halves
    // are routed to a single port instead of being split across players.
    private const val NINTENDO_VENDOR_ID = 0x057E
    // Unified slot -> claimed Android deviceId (-1 = unclaimed).
    private val slots = IntArray(8) { -1 }
    @Volatile private var pad2Enabled = false

    /** Master gate. OFF (default) => classic 2-slot co-op (3rd+ pad folds to P1). ON =>
     *  up to 8 distinct gamepads claim slots 0..7. Seeded at app start from
     *  ControllerMappings.multitapEnabled() and updated by the Pad-tab toggle. */
    @Volatile var multitapEnabled = false

    /** Fired exactly once, the first time the 2nd controller (slot 1 = P2 main) joins,
     *  so the app can hot-plug the native Pad2 slot before any P2 input is sent. Tap
     *  slots (2-7) don't use this — they're armed at boot / by the Multitap toggle. */
    @Volatile var onPlayer2Joined: (() -> Unit)? = null

    fun reset() {
        for (i in slots.indices) slots[i] = -1
        pad2Enabled = false
    }

    /** True once a second controller has joined this session (P2 main is live). */
    fun coopActive(): Boolean = slots[1] != -1

    /** Android InputDevice id assigned to a unified pad slot, or -1 if unclaimed.
     *  Lets per-slot PS2 rumble buzz the right pad. */
    fun deviceIdForPort(port: Int): Int =
        if (port in slots.indices) slots[port] else -1

    /**
     * Map a physical input device to a unified PS2 pad slot (0..7, or 0..1 when multitap
     * is off), claiming the next free slot. Synthetic / virtual events (deviceId < 0)
     * and non-gamepad nodes never claim a slot — they're treated as Player 1.
     */
    fun portForDevice(deviceId: Int): Int {
        if (deviceId < 0) return 0
        // Fast path: already-claimed nodes (no InputDevice lookup).
        for (i in slots.indices) if (slots[i] == deviceId) return i
        val dev = InputDevice.getDevice(deviceId)
        // Nintendo Joy-Cons (vendor 0x057E) enumerate as TWO InputDevices — the L and R
        // halves of ONE physical controller. Collapse BOTH onto Player 1 (port 0) so a
        // pair drives a single PS2 pad instead of splitting across P1/P2 (which made a
        // 1-player game respond to only one half, and a co-op game see two controllers).
        // They never claim a slot, so onPlayer2Joined stays silent for a lone pair. Gated
        // strictly on the Nintendo vendor id — every other controller keeps its routing.
        if (dev?.vendorId == NINTENDO_VENDOR_ID) return 0
        // Only a real GAMEPAD/JOYSTICK node may claim a slot. One physical controller
        // (notably a DualSense over Bluetooth) enumerates as SEVERAL InputDevices — a
        // gamepad node PLUS a touchpad/mouse node. Gating claims to gamepad sources stops
        // a secondary node from eating a slot and splitting one pad across two players.
        val src = dev?.sources ?: 0
        val isPad = (src and InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD ||
            (src and InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK
        if (!isPad) return 0 // touchpad / mouse / keyboard node → treat as P1, don't claim
        val maxSlots = if (multitapEnabled) 8 else 2
        for (i in 0 until maxSlots) {
            if (slots[i] == -1) {
                slots[i] = deviceId
                if (i == 1 && !pad2Enabled) { pad2Enabled = true; onPlayer2Joined?.invoke() }
                return i
            }
        }
        return 0 // all slots taken -> fold into Player 1
    }
}
