package kr.co.iefriends.pcsx2.utils;

import android.app.Activity;
import android.app.ActivityManager;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.res.AssetManager;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.RectF;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.content.ContextCompat;
import androidx.core.content.pm.ShortcutInfoCompat;
import androidx.core.content.pm.ShortcutManagerCompat;
import androidx.core.graphics.drawable.IconCompat;

import com.caverock.androidsvg.SVG;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Locale;

import kr.co.iefriends.pcsx2.R;
import kr.co.iefriends.pcsx2.activities.MainActivity;

/**
 * Helper for enumerating and applying alternate launcher icons packaged in assets.
 */
public final class AppIconManager {
    private static final String PREFS = "armsx2";
    private static final String PREF_KEY_SELECTION = "app_icon_selection";
    private static final String ICON_ID_DEFAULT = "__default__";
    private static final String ASSET_DIR = "alternative_icons";
    private static final String META_FILE = ASSET_DIR + "/meta_data.json";
    private static final List<String> SUPPORTED_EXTENSIONS = Arrays.asList(".png", ".webp", ".svg");

    private static volatile JSONObject sMetaData;

    private AppIconManager() {
    }

    @NonNull
    public static List<AppIconOption> getAvailableIcons(@NonNull Context context) {
        List<AppIconOption> options = new ArrayList<>();
        String defaultLabel = context.getString(R.string.settings_app_icon_default);
        options.add(new AppIconOption(ICON_ID_DEFAULT, defaultLabel, null));

        AssetManager assetManager = context.getAssets();
        String[] files;
        try {
            files = assetManager.list(ASSET_DIR);
        } catch (IOException e) {
            files = null;
        }
        if (files == null || files.length == 0) {
            return options;
        }
        Arrays.sort(files, String.CASE_INSENSITIVE_ORDER);
        JSONObject meta = loadMetaData(context);
        for (String name : files) {
            if (TextUtils.isEmpty(name)) {
                continue;
            }
            if ("meta_data.json".equalsIgnoreCase(name)) {
                continue;
            }
            String lower = name.toLowerCase(Locale.US);
            boolean supported = false;
            for (String ext : SUPPORTED_EXTENSIONS) {
                if (lower.endsWith(ext)) {
                    supported = true;
                    break;
                }
            }
            if (!supported) {
                continue;
            }
            String assetPath = ASSET_DIR + "/" + name;
            String displayName = meta != null ? meta.optString(name, null) : null;
            if (TextUtils.isEmpty(displayName)) {
                displayName = prettifyFileName(name);
            }
            options.add(new AppIconOption(name, displayName, assetPath));
        }
        return options;
    }

    @NonNull
    public static AppIconOption getCurrentSelection(@NonNull Context context) {
        List<AppIconOption> options = getAvailableIcons(context);
        SharedPreferences prefs = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        String selectedId = prefs.getString(PREF_KEY_SELECTION, ICON_ID_DEFAULT);
        for (AppIconOption option : options) {
            if (TextUtils.equals(option.id, selectedId)) {
                return option;
            }
        }
        return options.get(0);
    }

    public static void saveSelection(@NonNull Context context, @NonNull AppIconOption option) {
        context.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
                .edit()
                .putString(PREF_KEY_SELECTION, option.id)
                .apply();
    }

    public static boolean requestPinnedShortcut(@NonNull Context context, @NonNull AppIconOption option) {
        if (!ShortcutManagerCompat.isRequestPinShortcutSupported(context)) {
            return false;
        }
        Bitmap bitmap = loadIconBitmap(context, option, dpToPx(context, 108));
        if (bitmap == null) {
            return false;
        }
        Intent launchIntent = new Intent(context, MainActivity.class)
                .setAction(Intent.ACTION_VIEW)
                .addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_SINGLE_TOP);
        ShortcutInfoCompat shortcut = new ShortcutInfoCompat.Builder(context, "app-icon-" + option.id)
                .setShortLabel(option.displayName)
                .setIcon(IconCompat.createWithAdaptiveBitmap(bitmap))
                .setIntent(launchIntent)
                .build();
        return ShortcutManagerCompat.requestPinShortcut(context, shortcut, null);
    }

    public static void applyTaskDescription(@NonNull Activity activity) {
        Bitmap bitmap = loadIconBitmap(activity, getCurrentSelection(activity), dpToPx(activity, 48));
        if (bitmap == null) {
            return;
        }
        ActivityManager.TaskDescription description = new ActivityManager.TaskDescription(
                activity.getString(R.string.app_name), bitmap);
        activity.setTaskDescription(description);
    }

    @Nullable
    public static Bitmap loadIconBitmap(@NonNull Context context, @NonNull AppIconOption option, int sizePx) {
        if (sizePx <= 0) {
            sizePx = dpToPx(context, 48);
        }
        Bitmap bitmap = null;
        if (ICON_ID_DEFAULT.equals(option.id) || option.assetPath == null) {
            bitmap = loadApplicationIconBitmap(context, sizePx);
        } else {
            bitmap = loadAssetBitmap(context, option.assetPath, sizePx);
        }
        if (bitmap == null) {
            return null;
        }
        if (bitmap.getWidth() != sizePx || bitmap.getHeight() != sizePx) {
            bitmap = centerScale(bitmap, sizePx);
        }
        return bitmap;
    }

    private static Bitmap loadApplicationIconBitmap(@NonNull Context context, int sizePx) {
        try {
            PackageManager pm = context.getPackageManager();
            ApplicationInfo info = pm.getApplicationInfo(context.getPackageName(), 0);
            Drawable drawable = info.loadIcon(pm);
            return drawableToBitmap(drawable, sizePx);
        } catch (PackageManager.NameNotFoundException e) {
            Drawable drawable = ContextCompat.getDrawable(context, R.mipmap.ic_launcher);
            return drawableToBitmap(drawable, sizePx);
        }
    }

    @Nullable
    private static Bitmap loadAssetBitmap(@NonNull Context context, @NonNull String assetPath, int sizePx) {
        AssetManager assetManager = context.getAssets();
        String lower = assetPath.toLowerCase(Locale.US);
        InputStream stream = null;
        try {
            stream = assetManager.open(assetPath);
            if (lower.endsWith(".svg")) {
                return renderSvg(stream, sizePx);
            }
            Bitmap bitmap = BitmapFactory.decodeStream(stream);
            if (bitmap == null) {
                return null;
            }
            return centerScale(bitmap, sizePx);
        } catch (IOException e) {
            return null;
        } finally {
            if (stream != null) {
                try {
                    stream.close();
                } catch (IOException ignored) {
                }
            }
        }
    }

    private static Bitmap renderSvg(@NonNull InputStream inputStream, int sizePx) throws IOException {
        try {
            SVG svg = SVG.getFromInputStream(inputStream);
            float width = svg.getDocumentWidth();
            float height = svg.getDocumentHeight();
            if (Float.isNaN(width) || width <= 0f || Float.isNaN(height) || height <= 0f) {
                RectF viewBox = svg.getDocumentViewBox();
                if (viewBox != null && viewBox.width() > 0f && viewBox.height() > 0f) {
                    width = viewBox.width();
                    height = viewBox.height();
                } else {
                    width = 512f;
                    height = 512f;
                }
            }
            float scale = sizePx / Math.max(width, height);
            int renderWidth = Math.round(width * scale);
            int renderHeight = Math.round(height * scale);
            if (renderWidth <= 0) {
                renderWidth = sizePx;
            }
            if (renderHeight <= 0) {
                renderHeight = sizePx;
            }
            Bitmap bitmap = Bitmap.createBitmap(sizePx, sizePx, Bitmap.Config.ARGB_8888);
            Canvas canvas = new Canvas(bitmap);
            float dx = (sizePx - renderWidth) / 2f;
            float dy = (sizePx - renderHeight) / 2f;
            canvas.translate(dx, dy);
            canvas.scale(scale, scale);
            svg.renderToCanvas(canvas);
            return bitmap;
        } catch (Exception e) {
            throw new IOException("Failed to parse SVG", e);
        }
    }

    private static Bitmap centerScale(@NonNull Bitmap source, int sizePx) {
        if (source.getWidth() == sizePx && source.getHeight() == sizePx) {
            return source;
        }
        Bitmap output = Bitmap.createBitmap(sizePx, sizePx, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(output);
        float scale = Math.min((float) sizePx / source.getWidth(), (float) sizePx / source.getHeight());
        float width = source.getWidth() * scale;
        float height = source.getHeight() * scale;
        float left = (sizePx - width) / 2f;
        float top = (sizePx - height) / 2f;
        RectF dest = new RectF(left, top, left + width, top + height);
        canvas.drawBitmap(source, null, dest, null);
        return output;
    }

    private static Bitmap drawableToBitmap(@Nullable Drawable drawable, int sizePx) {
        if (drawable == null) {
            return null;
        }
        if (drawable instanceof BitmapDrawable bitmapDrawable) {
            Bitmap bitmap = bitmapDrawable.getBitmap();
            if (bitmap != null) {
                return centerScale(bitmap, sizePx);
            }
        }
        Bitmap bitmap = Bitmap.createBitmap(sizePx, sizePx, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        drawable.setBounds(0, 0, sizePx, sizePx);
        drawable.draw(canvas);
        return bitmap;
    }

    private static int dpToPx(@NonNull Context context, int dp) {
        float density = context.getResources().getDisplayMetrics().density;
        return Math.max(1, Math.round(dp * density));
    }

    @Nullable
    private static JSONObject loadMetaData(@NonNull Context context) {
        JSONObject cached = sMetaData;
        if (cached != null) {
            return cached;
        }
        AssetManager assetManager = context.getAssets();
        try (InputStream stream = assetManager.open(META_FILE)) {
            byte[] bytes = readAll(stream);
            if (bytes.length == 0) {
                sMetaData = new JSONObject();
            } else {
                sMetaData = new JSONObject(new String(bytes, StandardCharsets.UTF_8));
            }
        } catch (IOException | JSONException e) {
            sMetaData = new JSONObject();
        }
        return sMetaData;
    }

    private static byte[] readAll(@NonNull InputStream stream) throws IOException {
        byte[] buffer = new byte[8192];
        int read;
        ArrayList<byte[]> chunks = new ArrayList<>();
        int length = 0;
        while ((read = stream.read(buffer)) != -1) {
            byte[] chunk = Arrays.copyOf(buffer, read);
            chunks.add(chunk);
            length += read;
        }
        if (chunks.isEmpty()) {
            return new byte[0];
        }
        byte[] result = new byte[length];
        int offset = 0;
        for (byte[] chunk : chunks) {
            System.arraycopy(chunk, 0, result, offset, chunk.length);
            offset += chunk.length;
        }
        return result;
    }

    private static String prettifyFileName(@NonNull String fileName) {
        String name = fileName;
        int dot = name.lastIndexOf('.');
        if (dot >= 0) {
            name = name.substring(0, dot);
        }
        name = name.replace('_', ' ').replace('-', ' ');
        name = name.trim();
        if (name.isEmpty()) {
            return fileName;
        }
        return Character.toUpperCase(name.charAt(0)) + name.substring(1);
    }

    public static final class AppIconOption {
        public final String id;
        public final String displayName;
        @Nullable
        public final String assetPath;

        AppIconOption(@NonNull String id, @NonNull String displayName, @Nullable String assetPath) {
            this.id = id;
            this.displayName = displayName;
            this.assetPath = assetPath;
        }
    }
}
