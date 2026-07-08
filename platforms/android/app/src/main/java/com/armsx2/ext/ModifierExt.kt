package com.armsx2.ext

import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.ui.Modifier
import androidx.compose.ui.input.pointer.pointerInput

object ModifierExt {
    //Multiplatform supported click/press detection
    fun Modifier.onPress(action: () -> Unit): Modifier {
        return this.pointerInput(Unit) {
            detectTapGestures(onTap = {
                action()
            })
        }
    }
}