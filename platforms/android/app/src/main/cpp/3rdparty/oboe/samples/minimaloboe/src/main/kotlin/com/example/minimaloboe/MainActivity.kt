/*
 * Copyright 2022 The Android Open Source Project
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

package com.example.minimaloboe

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material.Button
import androidx.compose.material.MaterialTheme
import androidx.compose.material.Surface
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.tooling.preview.Preview
import androidx.lifecycle.ProcessLifecycleOwner
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.example.minimaloboe.ui.theme.SamplesTheme

class MainActivity : ComponentActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // Let our AudioPlayer observe lifecycle events for the application so when it goes into the
        // background we can stop audio playback.
        ProcessLifecycleOwner.get().lifecycle.addObserver(AudioPlayer)

        setContent {
            SamplesTheme {
                // A surface container using the 'background' color from the theme
                Surface(
                    modifier = Modifier.fillMaxSize(),
                    color = MaterialTheme.colors.background
                ) {
                    MainControls()
                }
            }
        }
    }
}

@Composable
fun MainControls() {
    val playerState by AudioPlayer.playerState.collectAsStateWithLifecycle()
    MainControls(playerState, AudioPlayer::setPlaybackEnabled)
}

@Composable
fun MainControls(playerState: PlayerState, setPlaybackEnabled: (Boolean) -> Unit) {

    Box(
        modifier = Modifier
            .fillMaxSize(), contentAlignment = Alignment.Center
    ) {
        Column {

            val isPlaying = playerState is PlayerState.Started
            Text(text = "Minimal Oboe!")
            Row {
                Button(
                    onClick = { setPlaybackEnabled(true) },
                    enabled = !isPlaying
                ) {
                    Text(text = "Start Audio")
                }
                Button(
                    onClick = { setPlaybackEnabled(false) },
                    enabled = isPlaying
                ) {
                    Text(text = "Stop Audio")
                }
            }

            // Create a status message for displaying the current playback state.
            val uiStatusMessage = "Current status: " +
                    when (playerState) {
                        PlayerState.NoResultYet -> "No result yet"
                        PlayerState.Started -> "Started"
                        PlayerState.Stopped -> "Stopped"
                        is PlayerState.Unknown -> {
                            "Unknown. Result = " + playerState.resultCode
                        }
                    }
            Text(uiStatusMessage)
        }
    }
}

@Preview(showBackground = true)
@Composable
fun DefaultPreview() {
    SamplesTheme {
        MainControls(PlayerState.Started) { }
    }
}
