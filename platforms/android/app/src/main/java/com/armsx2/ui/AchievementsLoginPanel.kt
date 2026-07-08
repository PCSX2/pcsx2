package com.armsx2.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextFieldDefaults
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalSoftwareKeyboardController
import androidx.compose.foundation.layout.Box
import com.armsx2.ui.settings.controllerFocusable
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.text.input.VisualTransformation
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.armsx2.i18n.I18n
import com.armsx2.i18n.str
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import kr.co.iefriends.pcsx2.NativeApp

/**
 * Bottom-left RetroAchievements login form. Drives
 * [NativeApp.loginAchievements] which posts a synchronous HTTP login
 * request to the rcheevos server — dispatched on Dispatchers.IO so the
 * Compose thread stays responsive (the request can take a few seconds
 * over a slow connection).
 *
 * Username + password go directly to the JNI; we don't persist either.
 * The native side stores the auth token through rcheevos's own state
 * (hashed token in the app's settings layer), so a successful login
 * survives app restarts without us holding the password.
 *
 * On success the next AchievementsPanel poll (4s after login completes)
 * picks up loggedIn=true and the row list re-renders. We close the
 * panel back to Root immediately on success — no banner, the panel
 * itself becomes the confirmation.
 */
@Composable
fun AchievementsLoginPanel(onClose: () -> Unit) {
    var user by remember { mutableStateOf("") }
    var pass by remember { mutableStateOf("") }
    var inFlight by remember { mutableStateOf(false) }
    var error by remember { mutableStateOf<String?>(null) }
    val scope = rememberCoroutineScope()
    val scroll = rememberScrollState()
    val userFocus = remember { FocusRequester() }
    val passFocus = remember { FocusRequester() }
    val keyboard = LocalSoftwareKeyboardController.current

    // The bottom-left modal box that hosts this panel is sized with
    // .fillMaxHeight(0.75f) — fine for confirms / slot pickers but the
    // login form (title + disclaimer + 2 OutlinedTextFields + buttons)
    // can exceed the box on phones in landscape, cutting off the
    // Cancel/Sign-In row at the bottom. Wrapping the column in
    // verticalScroll guarantees the buttons stay reachable: on tall
    // screens nothing scrolls, on short ones the user can drag.
    // Content-sized rather than fillMaxHeight so the box is only as tall as
    // it needs to be — the bottom-left modal container can otherwise
    // stretch this to 75% of screen height which clips on small landscape
    // phones. verticalScroll stays as a safety net for very narrow screens.
    Column(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(8.dp))
            .background(Color(0xFF222222))
            .verticalScroll(scroll)
            .padding(10.dp),
    ) {
        Text(
            str("ralogin.title"),
            color = Color.White,
            fontSize = 13.sp,
            fontWeight = FontWeight.Bold,
        )
        Spacer(Modifier.height(2.dp))
        Text(
            str("ralogin.passwordNotStored"),
            color = Color(0xFFAAAAAA),
            fontSize = 9.sp,
        )
        Spacer(Modifier.height(6.dp))

        val tfColors = TextFieldDefaults.colors(
            focusedTextColor = Color.White,
            unfocusedTextColor = Color.White,
            focusedContainerColor = Color(0xFF111111),
            unfocusedContainerColor = Color(0xFF111111),
            disabledContainerColor = Color(0xFF111111),
            focusedLabelColor = Colors.pasx2_blue,
            unfocusedLabelColor = Color(0xFFAAAAAA),
            focusedIndicatorColor = Colors.pasx2_blue,
            unfocusedIndicatorColor = Color(0xFF555555),
            cursorColor = Colors.pasx2_blue,
        )

        Box(Modifier.controllerFocusable("ach:user", onConfirm = {
            runCatching { userFocus.requestFocus() }
            keyboard?.show()
        })) {
            OutlinedTextField(
                value = user,
                onValueChange = { if (!inFlight) user = it.trim() },
                label = { Text(str("ralogin.username.label")) },
                singleLine = true,
                enabled = !inFlight,
                colors = tfColors,
                keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Email),
                modifier = Modifier.fillMaxWidth().focusRequester(userFocus),
            )
        }
        Spacer(Modifier.height(6.dp))
        Box(Modifier.controllerFocusable("ach:pass", onConfirm = {
            runCatching { passFocus.requestFocus() }
            keyboard?.show()
        })) {
            OutlinedTextField(
                value = pass,
                onValueChange = { if (!inFlight) pass = it },
                label = { Text(str("ralogin.password.label")) },
                singleLine = true,
                enabled = !inFlight,
                visualTransformation = PasswordVisualTransformation(),
                colors = tfColors,
                keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Password),
                modifier = Modifier.fillMaxWidth().focusRequester(passFocus),
            )
        }

        val err = error
        if (err != null) {
            Spacer(Modifier.height(6.dp))
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .clip(RoundedCornerShape(6.dp))
                    .background(Color(0xFF5A1A1A))
                    .padding(8.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Text("⚠", color = Color(0xFFFF6B6B), fontSize = 14.sp)
                Spacer(Modifier.width(6.dp))
                Text(err, color = Color.White, fontSize = 12.sp)
            }
        }

        Spacer(Modifier.height(10.dp))
        Row(modifier = Modifier.fillMaxWidth()) {
            ActionButton(
                label = str("action.cancel"),
                primary = false,
                enabled = !inFlight,
                onClick = onClose,
                modifier = Modifier.weight(1f),
                controllerId = "ach:cancel",
            )
            Spacer(Modifier.width(8.dp))
            ActionButton(
                label = if (inFlight) str("ralogin.signingIn") else str("ralogin.signIn"),
                primary = true,
                enabled = !inFlight && user.isNotEmpty() && pass.isNotEmpty(),
                onClick = {
                    inFlight = true
                    error = null
                    scope.launch {
                        val result = withContext(Dispatchers.IO) {
                            runCatching { NativeApp.loginAchievements(user, pass) }
                        }
                        inFlight = false
                        result
                            .onSuccess { msg ->
                                if (msg == null) {
                                    pass = ""
                                    onClose()
                                } else {
                                    error = msg
                                }
                            }
                            .onFailure { error = it.message ?: I18n.get("ralogin.loginFailed") }
                    }
                },
                modifier = Modifier.weight(1f),
                controllerId = "ach:login-submit",
            )
        }
    }
}

@Composable
private fun ActionButton(
    label: String,
    primary: Boolean,
    enabled: Boolean,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    controllerId: String? = null,
) {
    val bg = when {
        !enabled -> Color(0xFF333333)
        primary -> Colors.pasx2_blue
        else -> Color(0xFF444444)
    }
    val border = if (primary) Colors.pasx2_blue else Color(0xFF666666)
    val fg = if (enabled) Color.White else Color(0xFF888888)
    Row(
        modifier = modifier
            .clip(RoundedCornerShape(6.dp))
            .background(bg)
            .border(1.dp, border, RoundedCornerShape(6.dp))
            .then(
                if (enabled && controllerId != null)
                    Modifier.controllerFocusable(controllerId, onConfirm = onClick)
                else Modifier,
            )
            .let { if (enabled) it.clickable(onClick = onClick) else it }
            .padding(vertical = 10.dp),
        horizontalArrangement = androidx.compose.foundation.layout.Arrangement.Center,
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(label, color = fg, fontSize = 13.sp, fontWeight = FontWeight.Bold)
    }
}
