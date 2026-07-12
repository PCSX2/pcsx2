package com.armsx2.ui

import androidx.compose.runtime.Composable
import androidx.compose.material3.Text
import androidx.compose.ui.tooling.preview.Preview

@Composable
fun MyScreen() {
    Text("Hello from Compose")
}

@Preview(showBackground = true)
@Composable
fun MyScreenPreview() {
    MyScreen()
}