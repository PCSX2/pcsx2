// SPDX-License-Identifier: GPL-3.0+
package com.armsx2

import android.app.Application
import android.os.Build
import coil.ImageLoader
import coil.ImageLoaderFactory
import coil.decode.GifDecoder
import coil.decode.ImageDecoderDecoder
import coil.disk.DiskCache
import coil.memory.MemoryCache
import coil.request.CachePolicy
import java.io.File

/**
 * Application subclass that owns the Coil [ImageLoader] used app-wide for game
 * cover art. We override the default Coil disk cache for two reasons:
 *
 *  1. The default location is `cacheDir/image_cache`, which Android may wipe
 *     under memory pressure. Cover art is small, never changes, and worth
 *     keeping across cache-clear events — `filesDir/cover_cache` is the
 *     persistent equivalent.
 *  2. The default size is 2% of free space (max 250 MB). 100 MB is plenty
 *     for thousands of covers (typical PS1/PS2 cover JPEG ~30-80 KB) while
 *     not eating the user's storage.
 *
 * Cover requests in the UI use Coil's `SubcomposeAsyncImage`, which picks up
 * this default `ImageLoader` automatically — no changes needed at call sites.
 */
class Pasx2Application : Application(), ImageLoaderFactory {

	override fun newImageLoader(): ImageLoader {
		val coverCacheDir = File(filesDir, "cover_cache").apply { mkdirs() }

		return ImageLoader.Builder(this)
			.components {
				// Animated library background support: decode animated GIF /
				// WebP / APNG. Android's ImageDecoder (API 28+) covers all
				// three; GifDecoder is the fallback for API 26-27. Static
				// images (JPEG/PNG covers) are unaffected.
				if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P)
					add(ImageDecoderDecoder.Factory())
				else
					add(GifDecoder.Factory())
			}
			.diskCache {
				DiskCache.Builder()
					.directory(coverCacheDir)
					// 100 MiB. Covers are tiny; this fits an enormous library.
					.maxSizeBytes(100L * 1024L * 1024L)
					.build()
			}
			.memoryCache {
				MemoryCache.Builder(this)
					// 25% of the app's memory budget (Coil default), but
					// explicit so it's clear what we're doing.
					.maxSizePercent(0.25)
					.build()
			}
			// Cache policy: keep both layers enabled. Read order is
			// memory → disk → network; write order is the inverse.
			.diskCachePolicy(CachePolicy.ENABLED)
			.memoryCachePolicy(CachePolicy.ENABLED)
			.networkCachePolicy(CachePolicy.ENABLED)
			// Game cover JPEGs from xlenore/* don't change once published,
			// so always serve from the disk cache after the first successful
			// fetch even if the server's Cache-Control says otherwise.
			.respectCacheHeaders(false)
			.crossfade(150)
			.build()
	}
}
