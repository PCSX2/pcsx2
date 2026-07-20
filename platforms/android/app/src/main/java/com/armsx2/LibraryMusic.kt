package com.armsx2

import android.content.Context
import android.media.AudioAttributes
import android.media.AudioFocusRequest
import android.media.AudioManager
import android.media.MediaPlayer
import android.os.Build
import android.util.Log
import androidx.compose.runtime.mutableStateOf
import androidx.core.content.edit
import com.armsx2.runtime.MainActivityRuntime

/**
 * Ambient background music for the game library — the "console dashboard" feel.
 *
 * DELIBERATELY LIBRARY-ONLY. It plays while no VM is running and stops the moment a
 * game boots, which is both what a PS2/Xbox dashboard does and what keeps it clear of
 * the emulator's own audio: SPU2 output goes through Oboe, and Android has been
 * observed reclaiming that stream when the VM pauses (issue #333). A second long-lived
 * stream competing over gameplay would land straight on top of that.
 *
 * Track: "Calm Ambient 1 (Synthwave 4k)" by cynicmusic (The Cynic Project), released
 * CC0 / public domain on OpenGameArt. CC0 imposes no attribution requirement, but the
 * author asks for credit and we give it — see the About screen.
 */
object LibraryMusic {
    private const val TAG = "LibraryMusic"
    private const val EnabledKey = "ui.libraryMusic"

    /** Background music should sit under the UI, not command it. */
    private const val VOLUME = 0.55f

    val enabled = mutableStateOf(true)

    private var player: MediaPlayer? = null
    private var focusRequest: AudioFocusRequest? = null
    /** True when we stopped for something temporary (a call, another app ducking us)
     *  and should resume ourselves when focus comes back — as opposed to being off. */
    private var pausedForFocus = false

    fun load() {
        enabled.value = MainActivityRuntime.prefs.getBoolean(EnabledKey, true)
    }

    /** True while the track is actually audible — drives the cold-start retry below. */
    fun isPlaying(): Boolean = runCatching { player?.isPlaying == true }.getOrDefault(false)

    fun set(context: Context, value: Boolean) {
        enabled.value = value
        MainActivityRuntime.prefs.edit { putBoolean(EnabledKey, value) }
        if (value) start(context) else stop(context)
    }

    private fun audioManager(context: Context): AudioManager? =
        context.getSystemService(Context.AUDIO_SERVICE) as? AudioManager

    /**
     * Begin playing, if we should. No-ops when the setting is off, a VM is running, or
     * something else is already playing audio.
     *
     * The isMusicActive check is the difference between a nice touch and a hostile one:
     * without it, opening the app over someone's podcast or Spotify starts a second
     * stream on top of theirs. Deferring to whoever is already playing costs us nothing
     * — the user can still toggle it on explicitly.
     */
    fun start(context: Context) {
        if (!enabled.value) return
        if (MainActivityRuntime.eState.value != EmuState.STOPPED) return
        if (player != null) { resume(); return }
        val am = audioManager(context)
        if (am?.isMusicActive == true) {
            Log.i(TAG, "another app is playing audio; not starting library music")
            return
        }
        if (am != null && !requestFocus(am)) return

        // Built by hand rather than MediaPlayer.create(): create() does setDataSource +
        // prepare internally, so attributes set afterwards land on an already-prepared
        // player and are ignored. Android routes and ducks by those attributes, so
        // getting them applied before prepare() is what makes this behave like media
        // rather than a system sound.
        runCatching {
            MediaPlayer().apply {
                setAudioAttributes(
                    AudioAttributes.Builder()
                        .setUsage(AudioAttributes.USAGE_MEDIA)
                        .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
                        .build()
                )
                setDataSource(
                    context,
                    android.net.Uri.parse("android.resource://${context.packageName}/${R.raw.library_music}"),
                )
                isLooping = true
                setVolume(VOLUME, VOLUME)
                prepare()
                start()
                player = this
            }
        }.onFailure { Log.w(TAG, "start failed", it) }
    }

    /** Stop and release. Called when a game boots and when the toggle goes off. */
    fun stop(context: Context) {
        pausedForFocus = false
        player?.let { p ->
            runCatching { if (p.isPlaying) p.stop() }
            runCatching { p.release() }
        }
        player = null
        audioManager(context)?.let { abandonFocus(it) }
    }

    /** Suspend without releasing — app backgrounded. */
    fun pause() {
        runCatching { player?.takeIf { it.isPlaying }?.pause() }
    }

    fun resume() {
        if (!enabled.value) return
        if (MainActivityRuntime.eState.value != EmuState.STOPPED) return
        runCatching { player?.takeIf { !it.isPlaying }?.start() }
    }

    // ---- audio focus ----

    private val focusListener = AudioManager.OnAudioFocusChangeListener { change ->
        when (change) {
            AudioManager.AUDIOFOCUS_LOSS -> {
                // Permanent: another app took over for good. Drop the player entirely
                // rather than sitting paused forever holding a decoder.
                pausedForFocus = false
                player?.let { p ->
                    runCatching { if (p.isPlaying) p.stop() }
                    runCatching { p.release() }
                }
                player = null
            }
            AudioManager.AUDIOFOCUS_LOSS_TRANSIENT,
            AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK -> {
                if (player?.isPlaying == true) { pausedForFocus = true; pause() }
            }
            AudioManager.AUDIOFOCUS_GAIN -> {
                if (pausedForFocus) { pausedForFocus = false; resume() }
            }
        }
    }

    private fun requestFocus(am: AudioManager): Boolean {
        val granted = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val req = AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN)
                .setAudioAttributes(
                    AudioAttributes.Builder()
                        .setUsage(AudioAttributes.USAGE_MEDIA)
                        .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
                        .build()
                )
                .setOnAudioFocusChangeListener(focusListener)
                .build()
            focusRequest = req
            am.requestAudioFocus(req)
        } else {
            @Suppress("DEPRECATION")
            am.requestAudioFocus(focusListener, AudioManager.STREAM_MUSIC, AudioManager.AUDIOFOCUS_GAIN)
        }
        return granted == AudioManager.AUDIOFOCUS_REQUEST_GRANTED
    }

    private fun abandonFocus(am: AudioManager) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            focusRequest?.let { runCatching { am.abandonAudioFocusRequest(it) } }
            focusRequest = null
        } else {
            @Suppress("DEPRECATION")
            runCatching { am.abandonAudioFocus(focusListener) }
        }
    }
}
