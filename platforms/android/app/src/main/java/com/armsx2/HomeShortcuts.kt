package com.armsx2

import android.content.Context
import android.content.Intent
import android.graphics.BitmapFactory
import androidx.core.content.pm.ShortcutInfoCompat
import androidx.core.content.pm.ShortcutManagerCompat
import androidx.core.graphics.drawable.IconCompat

/**
 * Pin a game to the Android home screen as a launcher shortcut (issue #242), like
 * NetherSX2/AetherSX2 — so a game in a Cocoon/launcher folder can boot ARMSX2 straight
 * into it. The shortcut targets [Main] with the same launch extra the external-launch
 * path (ES-DE etc.) already understands, so tapping it boots the game directly.
 *
 * Icon: the game's custom cover when set, otherwise the app icon. Requires a launcher
 * that supports pinned shortcuts (Android 8.0+ ShortcutManager); [isSupported] reports it.
 */
object HomeShortcuts {

    fun isSupported(ctx: Context): Boolean =
        runCatching { ShortcutManagerCompat.isRequestPinShortcutSupported(ctx) }.getOrDefault(false)

    /** Ask the launcher to add a home-screen shortcut that boots [game]. Returns false
     *  if pinning isn't supported (the caller should surface that). The launcher shows
     *  its own confirm UI, so this can't report the user's accept/decline. */
    fun pin(ctx: Context, game: GameInfo): Boolean {
        // Mirror GamesList.launchGame: file:// (raw all-files) games hand the core the
        // bare /storage path; SAF games pass the content:// URI string. The boot path
        // (Main.handleExternalLaunchIntent -> extractLaunchUri) reads the "path" extra.
        val launchArg = if (game.uri.scheme == "file") (game.uri.path ?: game.uri.toString())
            else game.uri.toString()
        val intent = Intent(ctx, Main::class.java).apply {
            action = Intent.ACTION_VIEW
            putExtra("path", launchArg)
            addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TOP)
        }
        val label = game.title.ifBlank { "PS2 Game" }
        val id = "game:" + (game.serial?.takeIf { it.isNotEmpty() } ?: game.uri.toString())
        val icon = coverIcon(ctx, game) ?: IconCompat.createWithResource(ctx, ctx.applicationInfo.icon)

        // 1) Modern pinned-shortcut API. Attempt it even when isRequestPinShortcutSupported
        //    reports false — some launchers (handheld/custom) under-report it but still
        //    accept the pin; runCatching swallows the "unsupported" throw if they don't.
        val modern = runCatching {
            val shortcut = ShortcutInfoCompat.Builder(ctx, id)
                .setShortLabel(label.take(20))
                .setLongLabel(label.take(40))
                .setIcon(icon)
                .setIntent(intent)
                .build()
            ShortcutManagerCompat.requestPinShortcut(ctx, shortcut, null)
        }.getOrDefault(false)
        if (modern) return true

        // 2) Legacy INSTALL_SHORTCUT broadcast — older / handheld launchers still honour
        //    it. Fire-and-forget (can't confirm), so report best-effort success.
        return runCatching {
            @Suppress("DEPRECATION")
            val add = Intent("com.android.launcher.action.INSTALL_SHORTCUT").apply {
                putExtra(Intent.EXTRA_SHORTCUT_INTENT, intent)
                putExtra(Intent.EXTRA_SHORTCUT_NAME, label)
                putExtra(Intent.EXTRA_SHORTCUT_ICON_RESOURCE,
                    Intent.ShortcutIconResource.fromContext(ctx, ctx.applicationInfo.icon))
                putExtra("duplicate", false)
            }
            ctx.sendBroadcast(add)
            true
        }.getOrDefault(false)
    }

    /** The game's custom cover as a shortcut icon, or null to fall back to the app icon. */
    private fun coverIcon(ctx: Context, game: GameInfo): IconCompat? = runCatching {
        val f = CustomCovers.fileFor(ctx, game) ?: return null
        val bmp = BitmapFactory.decodeFile(f.absolutePath) ?: return null
        IconCompat.createWithBitmap(bmp)
    }.getOrNull()
}
