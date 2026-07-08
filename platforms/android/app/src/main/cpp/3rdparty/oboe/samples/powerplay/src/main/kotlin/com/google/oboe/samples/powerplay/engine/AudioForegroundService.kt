/*
 * Copyright 2025 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.google.oboe.samples.powerplay.engine

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.content.pm.ServiceInfo
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.media.AudioAttributes
import android.media.AudioFocusRequest
import android.media.AudioManager
import android.media.MediaMetadata
import android.media.session.MediaSession
import android.media.session.PlaybackState
import android.os.Binder
import android.os.IBinder
import android.util.Log
import androidx.lifecycle.Observer
import com.google.oboe.samples.powerplay.MainActivity
import com.google.oboe.samples.powerplay.PlayList
import com.google.oboe.samples.powerplay.R
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

class AudioForegroundService : Service() {

    private lateinit var audioManager: AudioManager
    private lateinit var audioFocusRequest: AudioFocusRequest
    private lateinit var mediaSession: MediaSession
    private var currentAlbumArt: Bitmap? = null

    lateinit var player: PowerPlayAudioPlayer
    private val binder = LocalBinder()

    private val serviceScope = CoroutineScope(Dispatchers.Main + Job())
    private var playbackJob: Job? = null

    inner class LocalBinder : Binder() {
        fun getService(): AudioForegroundService = this@AudioForegroundService
    }

    private val audioFocusChangeListener = AudioManager.OnAudioFocusChangeListener { focusChange ->
        when (focusChange) {
            AudioManager.AUDIOFOCUS_GAIN -> {
                if (::player.isInitialized) {
                    player.startPlaying(player.currentSongIndex, null)
                }
            }

            AudioManager.AUDIOFOCUS_LOSS, AudioManager.AUDIOFOCUS_LOSS_TRANSIENT, AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK -> {
                if (::player.isInitialized) {
                    player.stopPlaying(player.currentSongIndex)
                }
            }
        }
    }

    private val songIndexObserver = Observer<Int> { index ->
        loadAlbumArt(index)
    }

    override fun onCreate() {
        super.onCreate()
        try {
            player = PowerPlayAudioPlayer()
            player.setupAudioStream()

            audioManager = getSystemService(AUDIO_SERVICE) as AudioManager
            val audioAttributes = AudioAttributes.Builder()
                .setUsage(AudioAttributes.USAGE_MEDIA)
                .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
                .build()

            audioFocusRequest = AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN)
                .setAudioAttributes(audioAttributes)
                .setAcceptsDelayedFocusGain(true)
                .setOnAudioFocusChangeListener(audioFocusChangeListener)
                .build()

            mediaSession = MediaSession(this, "PowerPlayAudioService").apply {
                setCallback(object : MediaSession.Callback() {
                    override fun onPlay() {
                        if (::player.isInitialized) player.startPlaying(player.currentSongIndex, player.currentPerformanceMode)
                    }

                    override fun onPause() {
                        if (::player.isInitialized) player.stopPlaying(player.currentSongIndex)
                    }

                    override fun onStop() {
                        if (::player.isInitialized) player.stopPlaying(player.currentSongIndex)
                        stopSelf()
                    }
                })
                setFlags(MediaSession.FLAG_HANDLES_MEDIA_BUTTONS or MediaSession.FLAG_HANDLES_TRANSPORT_CONTROLS)
                isActive = true
            }

            player.getCurrentSongIndexLive().observeForever(songIndexObserver)

        } catch (e: Exception) {
            Log.e(TAG, "Error in onCreate", e)
            throw RuntimeException("Failed to create AudioForegroundService", e)
        }
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        try {
            val notification = createNotification()
            startForeground(
                NOTIFICATION_ID,
                notification,
                ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PLAYBACK
            )

            val result = audioManager.requestAudioFocus(audioFocusRequest)
            if (result == AudioManager.AUDIOFOCUS_REQUEST_GRANTED) {
                // Let UI control playback start
            } else {
                Log.e(TAG, "Failed to get audio focus, result: $result")
            }

        } catch (e: Exception) {
            Log.e(TAG, "Error in onStartCommand", e)
            stopSelf()
        }

        return START_STICKY
    }

    override fun onDestroy() {
        super.onDestroy()
        if (::player.isInitialized) {
            player.getCurrentSongIndexLive().removeObserver(songIndexObserver)
            player.stopPlaying(player.currentSongIndex)
            player.teardownAudioStream()
        }

        if (::audioManager.isInitialized) {
            audioManager.abandonAudioFocusRequest(audioFocusRequest)
        }
        if (::mediaSession.isInitialized) {
            mediaSession.release()
        }
    }

    private fun loadAlbumArt(index: Int) {
        serviceScope.launch(Dispatchers.IO) {
            val song = PlayList.getOrNull(index)
            val bitmap = song?.let { BitmapFactory.decodeResource(resources, it.cover) }
            withContext(Dispatchers.Main) {
                currentAlbumArt = bitmap
                updateMetadata()
                updateNotification()
            }
        }
    }

    private fun updateMetadata() {
        if (!::player.isInitialized || !::mediaSession.isInitialized) return

        val currentSong = PlayList.getOrNull(player.currentSongIndex)
        val songTitle = currentSong?.name ?: "PowerPlay Audio"
        val songArtist = currentSong?.artist ?: "Playing..."

        val metadataBuilder = MediaMetadata.Builder()
            .putString(MediaMetadata.METADATA_KEY_TITLE, songTitle)
            .putString(MediaMetadata.METADATA_KEY_ARTIST, songArtist)

        if (currentAlbumArt != null) {
            metadataBuilder.putBitmap(MediaMetadata.METADATA_KEY_ALBUM_ART, currentAlbumArt)
        }

        mediaSession.setMetadata(metadataBuilder.build())
    }

    private fun updateNotification() {
         val notification = createNotification()
         val notificationManager = getSystemService(NOTIFICATION_SERVICE) as NotificationManager
         notificationManager.notify(NOTIFICATION_ID, notification)
    }

    private fun createNotification(): Notification {
        val channelId = createNotificationChannel()
        val notificationIntent = Intent(this, MainActivity::class.java)
        val pendingIntent = PendingIntent.getActivity(
            this,
            0,
            notificationIntent,
            PendingIntent.FLAG_IMMUTABLE
        )

        val isPlaying = if (::player.isInitialized) {
            player.getPlayerStateLive().value == PlayerState.Playing
        } else false

        val style = android.app.Notification.MediaStyle()
            .setMediaSession(mediaSession.sessionToken)

        val currentSong = if (::player.isInitialized) PlayList.getOrNull(player.currentSongIndex) else null
        val songTitle = currentSong?.name ?: "PowerPlay Audio"
        val songArtist = currentSong?.artist ?: (if (isPlaying) "Playing" else "Paused")

        val builder = Notification.Builder(this, channelId)
            .setContentTitle(songTitle)
            .setContentText(songArtist)
            .setSmallIcon(if (isPlaying) R.drawable.ic_pause else R.drawable.ic_play)
            .setContentIntent(pendingIntent)
            .setStyle(style)

        if (currentAlbumArt != null) {
            builder.setLargeIcon(currentAlbumArt)
        }

        if (isPlaying) {
            val pauseIntent = PendingIntent.getService(
                this, 1,
                Intent(this, AudioForegroundService::class.java).apply { action = "PAUSE" },
                PendingIntent.FLAG_IMMUTABLE
            )
        }

        return builder.build()
    }

    private fun createNotificationChannel(): String {
        val channelId = "audio_playback_channel"
        val channelName = "Audio Playback"
        val importance = NotificationManager.IMPORTANCE_LOW
        val channel = NotificationChannel(channelId, channelName, importance)
        val notificationManager =
            getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
        notificationManager.createNotificationChannel(channel)
        return channelId
    }

    override fun onBind(intent: Intent?): IBinder? {
        return binder
    }

    companion object {
        private const val NOTIFICATION_ID = 1
        private const val TAG = "AudioForegroundService"
    }
}
