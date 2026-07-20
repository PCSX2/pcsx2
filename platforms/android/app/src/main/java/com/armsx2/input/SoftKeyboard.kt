package com.armsx2.input

import android.app.Activity
import android.content.Context
import android.text.InputType
import android.view.KeyCharacterMap
import android.view.KeyEvent
import android.view.View
import android.view.ViewGroup
import android.view.WindowManager
import android.view.inputmethod.BaseInputConnection
import android.view.inputmethod.EditorInfo
import android.view.inputmethod.InputConnection
import android.view.inputmethod.InputMethodManager
import androidx.compose.runtime.mutableStateOf
import kr.co.iefriends.pcsx2.NativeApp

/**
 * Brings up the Android IME and feeds what it types to the emulated USB keyboard, so games with
 * in-game chat (EverQuest Online Adventures, Monster Hunter) are usable without a Bluetooth
 * keyboard attached.
 *
 * WHY THIS IS NOT JUST "SHOW THE KEYBOARD": Android soft keyboards do not send KeyEvents. They
 * commit text through an InputConnection, which is why a Bluetooth keyboard already worked
 * (real KeyEvents, handled by MainActivityRuntime.forwardKeyToUsbKeyboard) while the on-screen
 * one did nothing at all. So we host an invisible text-editor View, take the IME's committed
 * text, and convert each character back into the key-down/key-up pairs the emulated USB
 * keyboard expects — via KeyCharacterMap, which also produces the shift presses that make
 * capitals and symbols come out right.
 *
 * Characters with no keyboard mapping (emoji, most CJK) are dropped: the PS2's USB keyboard
 * protocol has no way to express them, so there is nothing sensible to send.
 */
object SoftKeyboard {
    /** True while the IME is being shown for the emulated keyboard. */
    val visible = mutableStateOf(false)

    private var sink: KeyboardSink? = null

    /** Virtual layout, so getEvents() maps characters the way a generic keyboard would. */
    private val charMap: KeyCharacterMap = KeyCharacterMap.load(KeyCharacterMap.VIRTUAL_KEYBOARD)

    fun show(activity: Activity) {
        val root = activity.findViewById<ViewGroup>(android.R.id.content) ?: return
        val view = sink ?: KeyboardSink(activity).also {
            sink = it
            // Zero-size and transparent: it exists only to own an InputConnection. Adding it to
            // the content root (rather than replacing the game surface) keeps the emulator
            // rendering underneath while the IME is up.
            root.addView(it, 0, 0)
        }
        // ADJUST_NOTHING so the IME slides over the game instead of squashing the render to
        // the space above it — a resized surface would change the aspect ratio mid-session.
        activity.window?.setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_NOTHING)
        view.isFocusableInTouchMode = true
        view.requestFocus()
        imm(activity)?.showSoftInput(view, InputMethodManager.SHOW_IMPLICIT)
        visible.value = true
    }

    fun hide(activity: Activity) {
        // Drop anything still queued: putting the keyboard away mid-word would otherwise keep
        // feeding the game, and a sequence cut between its SHIFT-down and SHIFT-up would leave
        // the modifier stuck on.
        pending.clear()
        val view = sink
        if (view != null) {
            imm(activity)?.hideSoftInputFromWindow(view.windowToken, 0)
            view.clearFocus()
        }
        visible.value = false
    }

    fun toggle(activity: Activity) {
        if (visible.value) hide(activity) else show(activity)
    }

    /** Drop the sink entirely — used when the VM stops so nothing keeps focus off the game. */
    fun release(activity: Activity) {
        hide(activity)
        sink?.let { (it.parent as? ViewGroup)?.removeView(it) }
        sink = null
    }

    private fun imm(activity: Activity): InputMethodManager? =
        activity.getSystemService(Context.INPUT_METHOD_SERVICE) as? InputMethodManager

    /**
     * How long to leave each state on the emulated device before moving to the next one.
     *
     * THIS IS NOT COSMETIC. usbKeyboardKey just sets a bind value that the VM samples on its own
     * schedule, so a press and release issued back-to-back from here can land entirely between
     * two samples and the game never sees the key at all. A physical keyboard holds a key for
     * tens of milliseconds; we have to do the same. 24ms is ~1.5 frames at 60Hz, so no sample
     * can straddle a whole press, and it still allows ~20 characters/second.
     */
    private const val KEY_STEP_MS = 24L

    private val pending = java.util.concurrent.LinkedBlockingQueue<Pair<Int, Boolean>>()

    /** Drains [pending] on its own thread: the UI thread must not sleep between key states. */
    private val worker: Thread by lazy {
        Thread({
            while (true) {
                val (keyCode, pressed) = pending.take()
                runCatching { NativeApp.usbKeyboardKey(0, keyCode, pressed) }
                runCatching { Thread.sleep(KEY_STEP_MS) }
            }
        }, "usb-kbd-ime").apply { isDaemon = true; start() }
    }

    private fun enqueue(keyCode: Int, pressed: Boolean) {
        worker // start on first use
        pending.put(keyCode to pressed)
    }

    /** Send one character as the key-down/key-up pair(s) a real keyboard would produce. */
    internal fun emitChar(ch: Char) {
        // getEvents() also emits the modifier presses (SHIFT for capitals and symbols), which
        // AndroidKeyCodeToQKeyCode maps, so the shifted character arrives correctly.
        val events = charMap.getEvents(charArrayOf(ch)) ?: return
        events.forEach { emitKeyEvent(it) }
    }

    internal fun emitKeyEvent(event: KeyEvent) {
        if (event.keyCode == KeyEvent.KEYCODE_UNKNOWN) return
        val pressed = when (event.action) {
            KeyEvent.ACTION_DOWN -> true
            KeyEvent.ACTION_UP -> false
            else -> return
        }
        enqueue(event.keyCode, pressed)
    }

    internal fun tap(keyCode: Int) {
        enqueue(keyCode, true)
        enqueue(keyCode, false)
    }
}

/** Invisible View whose only job is to own an InputConnection the IME can commit into. */
private class KeyboardSink(context: Context) : View(context) {
    init {
        isFocusable = true
        isFocusableInTouchMode = true
    }

    override fun onCheckIsTextEditor(): Boolean = true

    /** Physical keys currently held, for combo-aware hotkey matching below. */
    private val held = HashSet<Int>()

    /**
     * First crack at every key while the IME is up, before the IME itself sees it.
     *
     * Two things have to happen here rather than in the Activity:
     *  - Back dismisses the IME without reaching us, so [SoftKeyboard.visible] would go stale
     *    and the next hotkey press would try to hide an already-hidden keyboard.
     *  - The TOGGLE_KEYBOARD hotkey must still CLOSE the keyboard. Soft keyboards claim
     *    gamepad buttons for their own navigation, so a pad binding routed the normal way
     *    would open the IME and then be swallowed by it — leaving no way back out.
     */
    override fun onKeyPreIme(keyCode: Int, event: KeyEvent): Boolean {
        val activity = context as? Activity
        if (keyCode == KeyEvent.KEYCODE_BACK && event.action == KeyEvent.ACTION_UP) {
            SoftKeyboard.visible.value = false
            clearFocus()
            return super.onKeyPreIme(keyCode, event)
        }
        when (event.action) {
            KeyEvent.ACTION_DOWN -> {
                if (event.repeatCount == 0 &&
                    ControllerMappings.matchHotkey(keyCode, held) ==
                    ControllerMappings.SysHotkey.TOGGLE_KEYBOARD
                ) {
                    if (activity != null) SoftKeyboard.hide(activity)
                    return true
                }
                held.add(keyCode)
            }
            KeyEvent.ACTION_UP -> held.remove(keyCode)
        }
        return super.onKeyPreIme(keyCode, event)
    }

    override fun onCreateInputConnection(outAttrs: EditorInfo): InputConnection {
        // No autocorrect/suggestions: this is a passthrough to a PS2 keyboard, and a suggestion
        // bar that rewrites what was typed would silently change what the game receives.
        outAttrs.inputType = InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS
        outAttrs.imeOptions = EditorInfo.IME_ACTION_DONE or
            EditorInfo.IME_FLAG_NO_EXTRACT_UI or
            EditorInfo.IME_FLAG_NO_FULLSCREEN
        return SinkConnection(this)
    }
}

private class SinkConnection(view: View) : BaseInputConnection(view, false) {
    override fun commitText(text: CharSequence?, newCursorPosition: Int): Boolean {
        text?.forEach { SoftKeyboard.emitChar(it) }
        return true
    }

    /** Some IMEs stream characters as composing text before committing. */
    override fun setComposingText(text: CharSequence?, newCursorPosition: Int): Boolean = true

    override fun sendKeyEvent(event: KeyEvent): Boolean {
        SoftKeyboard.emitKeyEvent(event)
        return true
    }

    /** Backspace arrives as a delete request, not as a DEL key, on most IMEs. */
    override fun deleteSurroundingText(beforeLength: Int, afterLength: Int): Boolean {
        repeat(beforeLength) { SoftKeyboard.tap(KeyEvent.KEYCODE_DEL) }
        return true
    }

    override fun performEditorAction(editorAction: Int): Boolean {
        SoftKeyboard.tap(KeyEvent.KEYCODE_ENTER)
        return true
    }
}
