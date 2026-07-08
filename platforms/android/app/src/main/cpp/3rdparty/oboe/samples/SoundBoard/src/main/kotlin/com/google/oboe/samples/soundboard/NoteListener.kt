/*
 * Copyright 2021 The Android Open Source Project
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

package com.google.oboe.samples.soundboard

import com.google.oboe.samples.soundboard.MusicTileView.TileListener

class NoteListener(private var mEngineHandle: Long) : TileListener {
    private external fun noteOn(engineHandle: Long, noteIndex: Int)
    private external fun noteOff(engineHandle: Long, noteIndex: Int)
    override fun onTileOn(index: Int) {
        noteOn(mEngineHandle, index)
    }

    override fun onTileOff(index: Int) {
        noteOff(mEngineHandle, index)
    }
}
