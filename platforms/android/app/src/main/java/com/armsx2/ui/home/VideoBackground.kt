package com.armsx2.ui.home

import android.content.Context
import android.graphics.Matrix
import android.graphics.SurfaceTexture
import android.media.MediaPlayer
import android.net.Uri
import android.view.Surface
import android.view.TextureView
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.viewinterop.AndroidView
import com.armsx2.R

/** Resource URI for the bundled default XMB-wave library background video. */
fun defaultBackgroundVideoUri(context: Context): Uri =
    Uri.parse("android.resource://${context.packageName}/${R.raw.xmb_wave}")

/**
 * True if the URI points at a video we should play as a moving background, vs a
 * still image (or animated GIF/WebP) that Coil handles. Prefers the resolver
 * MIME type, falls back to the file extension for providers that don't report one.
 */
fun isVideoBackground(context: Context, uriString: String): Boolean {
    val uri = runCatching { Uri.parse(uriString) }.getOrNull() ?: return false
    runCatching { context.contentResolver.getType(uri) }.getOrNull()?.let { mime ->
        if (mime.startsWith("video/")) return true
        if (mime.startsWith("image/")) return false
    }
    val path = uriString.substringBefore('?').lowercase()
    return path.endsWith(".mp4") || path.endsWith(".m4v") || path.endsWith(".webm") ||
        path.endsWith(".mkv") || path.endsWith(".3gp") || path.endsWith(".mov")
}

/**
 * Looping, muted, centre-cropped video background. Mirrors the boot-splash
 * VideoView pattern but uses a TextureView so it can crop-to-fill (VideoView only
 * letterboxes) and sit behind Compose content. The MediaPlayer is released when
 * this leaves composition (e.g. a game launches), so no codec is held while a
 * title runs. Errors are swallowed — on failure the app backdrop shows through.
 *
 * Recreate the whole composable (wrap the call in `key(uri)`) when the URI
 * changes; the AndroidView factory only runs once.
 */
@Composable
fun VideoBackground(uri: Uri, modifier: Modifier = Modifier) {
    val player = remember { MediaPlayer() }
    DisposableEffect(Unit) {
        onDispose {
            runCatching { player.stop() }
            runCatching { player.release() }
        }
    }
    AndroidView(
        modifier = modifier,
        factory = { ctx ->
            TextureView(ctx).apply {
                surfaceTextureListener = object : TextureView.SurfaceTextureListener {
                    override fun onSurfaceTextureAvailable(st: SurfaceTexture, width: Int, height: Int) {
                        runCatching {
                            player.reset()
                            player.setDataSource(ctx, uri)
                            player.setSurface(Surface(st))
                            player.isLooping = true
                            player.setVolume(0f, 0f)
                            player.setOnVideoSizeChangedListener { _, vw, vh -> applyCenterCrop(this@apply, vw, vh) }
                            player.setOnPreparedListener { it.start() }
                            player.setOnErrorListener { _, _, _ -> true }
                            player.prepareAsync()
                        }
                    }

                    override fun onSurfaceTextureSizeChanged(st: SurfaceTexture, width: Int, height: Int) {
                        applyCenterCrop(this@apply, player.videoWidth, player.videoHeight)
                    }

                    override fun onSurfaceTextureDestroyed(st: SurfaceTexture): Boolean {
                        runCatching { player.stop() }
                        return true
                    }

                    override fun onSurfaceTextureUpdated(st: SurfaceTexture) {}
                }
            }
        },
    )
}

/**
 * Scale the TextureView content so the video covers the whole view (centre-crop),
 * matching ContentScale.Crop for still images. TextureView stretches the video to
 * its bounds by default; we scale the over-fitting axis back up around the centre.
 */
private fun applyCenterCrop(view: TextureView, videoW: Int, videoH: Int) {
    val vw = view.width
    val vh = view.height
    if (videoW <= 0 || videoH <= 0 || vw == 0 || vh == 0) return
    val viewAspect = vw.toFloat() / vh.toFloat()
    val videoAspect = videoW.toFloat() / videoH.toFloat()
    val scaleX: Float
    val scaleY: Float
    if (videoAspect > viewAspect) {
        // Video is wider than the view: fill height, let width overflow (cropped).
        scaleX = videoAspect / viewAspect
        scaleY = 1f
    } else {
        // Video is taller than the view: fill width, let height overflow (cropped).
        scaleX = 1f
        scaleY = viewAspect / videoAspect
    }
    val matrix = Matrix()
    matrix.setScale(scaleX, scaleY, vw / 2f, vh / 2f)
    view.setTransform(matrix)
}
