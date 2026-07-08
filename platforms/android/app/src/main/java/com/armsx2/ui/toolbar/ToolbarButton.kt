package com.armsx2.ui.toolbar

import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxScope
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.runtime.Composable
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.unit.dp
import com.armsx2.ui.Colors
import com.armsx2.ui.ToolbarImpl

open class ToolbarButton {
    open var icon = mutableStateOf<ImageVector?>(null)
    open var drawerSize = mutableIntStateOf(200)

    open var background = mutableStateOf(Colors.pasx2_blue)
    var expanded = mutableStateOf(false)

    @OptIn(ExperimentalFoundationApi::class)
    @Composable
    fun Button() {
        Box(
            Modifier.clip(RoundedCornerShape(4.dp)).size(34.dp)
                .background(background.value)
                .clickable { action() }) {
            Content()
        }
    }

    @Composable
    open fun BoxScope.Content() {
        Box(
            Modifier.size(30.dp).background(Color.Transparent)
                .align(Alignment.Center)
        ) {
            Icon()
        }
    }

    @Composable
    fun BoxScope.Icon() {
        Box(
            Modifier.background(Color.Transparent)
                .align(Alignment.Center)
        ) {
            icon.value?.let {
                Image(it, "")
            }
        }
    }

    open fun isVisible() = true

    open fun action() {
        if (!expanded.value) {
            if (ToolbarImpl.expanded.value != drawerSize.intValue) {
                ToolbarImpl.expanded.value = drawerSize.intValue
            }
            ToolbarImpl.drawerContext.value = this
            expanded.value = true
        } else {
            if (ToolbarImpl.drawerContext.value == this) {
                ToolbarImpl.expanded.value = 0
                expanded.value = false
            } else {
                if (ToolbarImpl.expanded.value != drawerSize.intValue) {
                    ToolbarImpl.expanded.value = drawerSize.intValue
                }
                ToolbarImpl.drawerContext.value = this
                expanded.value = true
            }
        }
    }

    @Composable
    open fun DrawerContent() {}
}