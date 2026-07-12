package com.armsx2.ui

import androidx.compose.runtime.Composable
import androidx.compose.material3.Text
import androidx.compose.ui.tooling.preview.Preview
import com.armsx2.i18n.str

@Composable
fun MyScreen() {
    Text(str("setup.bios.selectTitle"))
}

@Preview(showBackground = true)
@Composable
fun MyScreenPreview() {
    MyScreen()
}
