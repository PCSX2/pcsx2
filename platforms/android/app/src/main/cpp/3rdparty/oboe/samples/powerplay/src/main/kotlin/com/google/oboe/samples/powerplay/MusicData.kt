/*
 * Copyright 2026 The Android Open Source Project
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
package com.google.oboe.samples.powerplay

data class Music(
    val name: String,
    val artist: String,
    val fileName: String,
    val cover: Int,
)

val PlayList = listOf(
    Music(
        name = "Chemical Reaction",
        artist = "Momo Oboe",
        cover = R.drawable.album_art_1,
        fileName = "song1.wav",
    ),
    Music(
        name = "Digital Noca",
        artist = "Momo Oboe",
        cover = R.drawable.album_art_2,
        fileName = "song2.wav",
    ),
    Music(
        name = "Window Seat",
        artist = "Momo Oboe",
        cover = R.drawable.album_art_3,
        fileName = "song3.wav",
    ),
)
