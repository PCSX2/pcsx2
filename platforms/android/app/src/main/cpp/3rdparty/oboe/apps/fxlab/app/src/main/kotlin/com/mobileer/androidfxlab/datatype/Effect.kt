/*
 * Copyright  2019 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.mobileer.androidfxlab.datatype

/**
 * Class which represents an audio effect
 */
data class Effect(val effectDescription: EffectDescription) {
    val name = effectDescription.name
    val paramValues = FloatArray(effectDescription.paramValues.size) {
        i -> effectDescription.paramValues[i].defaultValue
    }
    var enable: Boolean = true
}
