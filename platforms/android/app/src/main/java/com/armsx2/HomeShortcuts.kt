package com.armsx2

import com.armsx2.runtime.MainActivityRuntime

import android.app.ActivityManager
import android.content.Context
import android.content.Intent
import android.content.pm.ShortcutManager
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.util.Log
import androidx.core.content.pm.ShortcutInfoCompat
import androidx.core.content.pm.ShortcutManagerCompat
import androidx.core.graphics.drawable.IconCompat
import kotlin.math.max
import kotlin.math.roundToInt

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

    private const val TAG = "HomeShortcuts"

    /** Fallback max icon edge (px) if the platform won't tell us. Matches the classic
     *  launcher large-icon size on xhdpi, i.e. deliberately conservative. */
    private const val FALLBACK_MAX_ICON_PX = 128

    fun isSupported(ctx: Context): Boolean =
        runCatching { ShortcutManagerCompat.isRequestPinShortcutSupported(ctx) }.getOrDefault(false)

    /** Ask the launcher to add a home-screen shortcut that boots [game]. Returns false
     *  if nothing was requested (the caller should surface that). A true return means the
     *  launcher *accepted the request* — it then shows its own confirm UI, so this can't
     *  report the user's accept/decline. */
    fun pin(ctx: Context, game: GameInfo): Boolean {
        // Raw file:// games hand the core the
        // bare /storage path; SAF games pass the content:// URI string. The boot path
        // (MainActivityRuntime.handleExternalLaunchIntent -> extractLaunchUri) reads the "path" extra.
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

        // Attempt the pin even when isRequestPinShortcutSupported reports false — some
        // launchers (handheld/custom) under-report it but still accept the pin. A genuine
        // unsupported/oversized-icon throw ends up false so the caller can toast: there is
        // no legacy fallback to fall back TO. com.android.launcher.action.INSTALL_SHORTCUT
        // has been ignored by the framework since API 26, and minSdk is 26, so the old
        // broadcast path was unreachable-by-construction AND reported unconditional
        // success — which is exactly why #335 pinned nothing and never toasted.
        return runCatching {
            val shortcut = ShortcutInfoCompat.Builder(ctx, id)
                .setShortLabel(label.take(20))
                .setLongLabel(label.take(40))
                .setIcon(icon)
                .setIntent(intent)
                .build()
            ShortcutManagerCompat.requestPinShortcut(ctx, shortcut, null)
        }.getOrElse { e ->
            Log.w(TAG, "pin: requestPinShortcut failed for '$label' (id=$id): $e")
            false
        }
    }

    /** The game's custom cover as a shortcut icon, or null to fall back to the app icon.
     *
     *  The cover is decoded downscaled and clamped to the launcher's max icon edge before
     *  it ever reaches IconCompat: PS2 box art (512x728+) is far larger than the shortcut
     *  icon budget, and an unbounded bitmap makes requestPinShortcut throw — the failure
     *  #335 was reporting as success. Kept as a plain (non-adaptive) bitmap on purpose:
     *  adaptive icons mask everything outside the centre ~66% safe zone, which would crop
     *  the title off portrait box art. The app-icon fallback is already adaptive. */
    private fun coverIcon(ctx: Context, game: GameInfo): IconCompat? = runCatching {
        val f = CustomCovers.fileFor(ctx, game) ?: return null
        val bmp = decodeScaled(f.absolutePath, maxIconEdgePx(ctx)) ?: return null
        IconCompat.createWithBitmap(bmp)
    }.getOrElse { e ->
        Log.w(TAG, "coverIcon: cover unusable, falling back to app icon: $e")
        null
    }

    /** The largest icon edge (px) the platform will accept for a shortcut. */
    private fun maxIconEdgePx(ctx: Context): Int {
        runCatching {
            val sm = ctx.getSystemService(ShortcutManager::class.java)
            minOf(sm.iconMaxWidth, sm.iconMaxHeight).takeIf { it > 0 }
        }.getOrNull()?.let { return it }
        runCatching {
            (ctx.getSystemService(Context.ACTIVITY_SERVICE) as ActivityManager)
                .launcherLargeIconSize.takeIf { it > 0 }
        }.getOrNull()?.let { return it }
        return FALLBACK_MAX_ICON_PX
    }

    /** Decode [path] at no more than [maxPx] on its longest edge, preserving aspect. */
    private fun decodeScaled(path: String, maxPx: Int): Bitmap? {
        val bounds = BitmapFactory.Options().apply { inJustDecodeBounds = true }
        BitmapFactory.decodeFile(path, bounds)
        val w = bounds.outWidth
        val h = bounds.outHeight
        if (w <= 0 || h <= 0) return null
        // Cheap power-of-two prepass so a 512x728 cover never lands in memory full size,
        // then an exact scale to land inside the budget.
        val opts = BitmapFactory.Options().apply { inSampleSize = sampleSizeFor(w, h, maxPx) }
        val bmp = BitmapFactory.decodeFile(path, opts) ?: return null
        return clampToMax(bmp, maxPx)
    }

    private fun sampleSizeFor(w: Int, h: Int, maxPx: Int): Int {
        var sample = 1
        while (max(w, h) / (sample * 2) >= maxPx) sample *= 2
        return sample
    }

    /** Scale [bmp] down so neither edge exceeds [maxPx], preserving aspect. */
    private fun clampToMax(bmp: Bitmap, maxPx: Int): Bitmap {
        val longest = max(bmp.width, bmp.height)
        if (longest <= maxPx) return bmp
        val ratio = maxPx.toFloat() / longest
        val w = max(1, (bmp.width * ratio).roundToInt())
        val h = max(1, (bmp.height * ratio).roundToInt())
        val scaled = Bitmap.createScaledBitmap(bmp, w, h, true)
        if (scaled !== bmp) bmp.recycle()
        return scaled
    }
}
