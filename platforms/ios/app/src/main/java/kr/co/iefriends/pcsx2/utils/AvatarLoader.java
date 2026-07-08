/*

By MoonPower (Momo-AUX1) GPLv3 License
   This file is part of ARMSX2.

   ARMSX2 is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   ARMSX2 is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with ARMSX2.  If not, see <http://www.gnu.org/licenses/>.

*/

package kr.co.iefriends.pcsx2.utils;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.os.Handler;
import android.os.Looper;
import android.text.TextUtils;
import android.widget.ImageView;

import androidx.annotation.Nullable;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.lang.ref.WeakReference;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.Objects;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

import kr.co.iefriends.pcsx2.R;

/**
 * Lightweight helper for loading small avatar bitmaps with in-memory caching.
 */
public final class AvatarLoader {
    private static final String KEY_PREFIX_LOCAL = "local:";
    private static final String KEY_PREFIX_REMOTE = "remote:";
    private static final int MAX_CACHE_KB;
    private static final android.util.LruCache<String, Bitmap> CACHE;
    private static final ExecutorService EXECUTOR = Executors.newFixedThreadPool(2);
    private static final Handler MAIN = new Handler(Looper.getMainLooper());

    static {
        final long maxMemoryKb = Runtime.getRuntime().maxMemory() / 1024L;
        MAX_CACHE_KB = (int) Math.min(maxMemoryKb / 16L, 2560L); 
        CACHE = new android.util.LruCache<String, Bitmap>(MAX_CACHE_KB) {
            @Override
            protected int sizeOf(String key, Bitmap value) {
                return value.getByteCount() / 1024;
            }
        };
    }

    private AvatarLoader() {
    }

    public static void loadLocal(ImageView target, @Nullable String path) {
        loadInternal(target, keyForLocal(path), () -> decodeLocal(path));
    }

    public static void loadRemote(ImageView target, @Nullable String url) {
        loadInternal(target, keyForRemote(url), () -> downloadBitmap(url));
    }

    public static void clear(ImageView target) {
        if (target == null) {
            return;
        }
        target.setTag(R.id.tag_avatar_loader, null);
        target.setImageResource(R.drawable.ic_avatar_placeholder);
    }

    @Nullable
    public static Bitmap getCachedLocal(@Nullable String path) {
        return CACHE.get(keyForLocal(path));
    }

    private static void loadInternal(ImageView target, @Nullable String cacheKey,
                                     Loader loader) {
        if (target == null) {
            return;
        }
        if (TextUtils.isEmpty(cacheKey)) {
            clear(target);
            return;
        }
        Bitmap cached = CACHE.get(cacheKey);
        target.setTag(R.id.tag_avatar_loader, cacheKey);
        if (cached != null && !cached.isRecycled()) {
            target.setImageBitmap(cached);
            return;
        }
        target.setImageResource(R.drawable.ic_avatar_placeholder);
        WeakReference<ImageView> viewRef = new WeakReference<>(target);
        EXECUTOR.execute(() -> {
            try {
                Bitmap bitmap = loader.load();
                if (bitmap == null) {
                    return;
                }
                CACHE.put(cacheKey, bitmap);
                MAIN.post(() -> {
                    ImageView imageView = viewRef.get();
                    if (imageView == null) {
                        return;
                    }
                    Object currentTag = imageView.getTag(R.id.tag_avatar_loader);
                    if (!Objects.equals(cacheKey, currentTag)) {
                        return;
                    }
                    imageView.setImageBitmap(bitmap);
                });
            } catch (IOException ignored) {
               // we ball
            }
        });
    }

    private static String keyForLocal(@Nullable String path) {
        if (TextUtils.isEmpty(path)) {
            return null;
        }
        return KEY_PREFIX_LOCAL + path;
    }

    private static String keyForRemote(@Nullable String url) {
        if (TextUtils.isEmpty(url)) {
            return null;
        }
        return KEY_PREFIX_REMOTE + url;
    }

    @Nullable
    private static Bitmap decodeLocal(@Nullable String path) {
        if (TextUtils.isEmpty(path)) {
            return null;
        }
        File file = new File(path);
        if (!file.exists()) {
            return null;
        }
        return BitmapFactory.decodeFile(path);
    }

    @Nullable
    private static Bitmap downloadBitmap(@Nullable String urlString) throws IOException {
        if (TextUtils.isEmpty(urlString)) {
            return null;
        }
        URL url = new URL(urlString);
        HttpURLConnection connection = (HttpURLConnection) url.openConnection();
        connection.setConnectTimeout(5000);
        connection.setReadTimeout(7000);
        connection.setInstanceFollowRedirects(true);
        connection.setRequestProperty("User-Agent", "ARMSX2/Android");
        try (InputStream stream = connection.getInputStream()) {
            if (stream == null) {
                return null;
            }
            byte[] data = readAll(stream);
            if (data.length == 0) {
                return null;
            }
            return BitmapFactory.decodeByteArray(data, 0, data.length);
        } finally {
            connection.disconnect();
        }
    }

    private static byte[] readAll(InputStream stream) throws IOException {
        ByteArrayOutputStream buffer = new ByteArrayOutputStream();
        byte[] tmp = new byte[4096];
        int read;
        while ((read = stream.read(tmp)) != -1) {
            buffer.write(tmp, 0, read);
        }
        return buffer.toByteArray();
    }

    private interface Loader {
        @Nullable
        Bitmap load() throws IOException;
    }
}
