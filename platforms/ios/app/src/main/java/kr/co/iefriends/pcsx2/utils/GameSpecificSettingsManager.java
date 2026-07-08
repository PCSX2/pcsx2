package kr.co.iefriends.pcsx2.utils;

import android.content.Context;
import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.Iterator;

import kr.co.iefriends.pcsx2.NativeApp;

public final class GameSpecificSettingsManager {
    private static final String FILE_NAME = "IGS.json";
    private static final String KEY_VERSION = "version";
    private static final String KEY_GAMES = "games";
    private static final int CURRENT_VERSION = 1;
    private static final Object LOCK = new Object();

    private GameSpecificSettingsManager() {
    }

    public static final class GameSettings {
        @Nullable
        public Boolean enableCheats;
        @Nullable
        public Boolean widescreen;
        @Nullable
        public Boolean noInterlacing;
        @Nullable
        public Boolean loadTextures;
        @Nullable
        public Boolean asyncTextures;
        @Nullable
        public Boolean precacheTextures;
        @Nullable
        public Boolean showFps;
        @Nullable
        public Integer renderer;
        @Nullable
        public String aspectRatio;

        public boolean hasOverrides() {
            return enableCheats != null || widescreen != null || noInterlacing != null
                    || loadTextures != null || asyncTextures != null || precacheTextures != null
                    || showFps != null || renderer != null || !TextUtils.isEmpty(aspectRatio);
        }

        JSONObject toJson() throws JSONException {
            JSONObject obj = new JSONObject();
            if (enableCheats != null) obj.put("enableCheats", enableCheats);
            if (widescreen != null) obj.put("widescreen", widescreen);
            if (noInterlacing != null) obj.put("noInterlacing", noInterlacing);
            if (loadTextures != null) obj.put("loadTextures", loadTextures);
            if (asyncTextures != null) obj.put("asyncTextures", asyncTextures);
            if (precacheTextures != null) obj.put("precacheTextures", precacheTextures);
            if (showFps != null) obj.put("showFps", showFps);
            if (renderer != null) obj.put("renderer", renderer);
            if (!TextUtils.isEmpty(aspectRatio)) obj.put("aspectRatio", aspectRatio);
            return obj;
        }

        static GameSettings fromJson(@Nullable JSONObject obj) {
            if (obj == null) {
                return null;
            }
            GameSettings settings = new GameSettings();
            if (obj.has("enableCheats")) settings.enableCheats = obj.optBoolean("enableCheats");
            if (obj.has("widescreen")) settings.widescreen = obj.optBoolean("widescreen");
            if (obj.has("noInterlacing")) settings.noInterlacing = obj.optBoolean("noInterlacing");
            if (obj.has("loadTextures")) settings.loadTextures = obj.optBoolean("loadTextures");
            if (obj.has("asyncTextures")) settings.asyncTextures = obj.optBoolean("asyncTextures");
            if (obj.has("precacheTextures")) settings.precacheTextures = obj.optBoolean("precacheTextures");
            if (obj.has("showFps")) settings.showFps = obj.optBoolean("showFps");
            if (obj.has("renderer")) settings.renderer = obj.optInt("renderer");
            if (obj.has("aspectRatio")) settings.aspectRatio = obj.optString("aspectRatio", null);
            if (!settings.hasOverrides()) {
                return null;
            }
            return settings;
        }
    }

    @Nullable
    public static GameSettings getSettings(Context context, String key) {
        if (context == null || TextUtils.isEmpty(key)) {
            return null;
        }
        synchronized (LOCK) {
            JSONObject root = readRoot(context);
            JSONObject games = root.optJSONObject(KEY_GAMES);
            if (games == null) {
                return null;
            }
            return GameSettings.fromJson(games.optJSONObject(key));
        }
    }

    public static void saveSettings(Context context, String key, GameSettings settings) {
        if (context == null || TextUtils.isEmpty(key)) {
            return;
        }
        if (settings == null || !settings.hasOverrides()) {
            removeSettings(context, key);
            return;
        }
        synchronized (LOCK) {
            JSONObject root = readRoot(context);
            JSONObject games = ensureGamesObject(root);
            try {
                games.put(key, settings.toJson());
                writeRoot(context, root);
            } catch (JSONException | IOException ignored) {
            }
        }
    }

    public static void removeSettings(Context context, String key) {
        if (context == null || TextUtils.isEmpty(key)) {
            return;
        }
        synchronized (LOCK) {
            JSONObject root = readRoot(context);
            JSONObject games = root.optJSONObject(KEY_GAMES);
            if (games == null || !games.has(key)) {
                return;
            }
            games.remove(key);
            if (games.length() == 0) {
                root.remove(KEY_GAMES);
            }
            try {
                writeRoot(context, root);
            } catch (IOException ignored) {
            }
        }
    }

    static void pruneMissingEntries(Context context, Iterable<String> validKeys) {
        if (context == null || validKeys == null) {
            return;
        }
        synchronized (LOCK) {
            JSONObject root = readRoot(context);
            JSONObject games = root.optJSONObject(KEY_GAMES);
            if (games == null || games.length() == 0) {
                return;
            }
            java.util.HashSet<String> keep = new java.util.HashSet<>();
            for (String key : validKeys) {
                if (!TextUtils.isEmpty(key)) {
                    keep.add(key);
                }
            }
            boolean changed = false;
            Iterator<String> keys = games.keys();
            while (keys.hasNext()) {
                String storedKey = keys.next();
                if (!keep.contains(storedKey)) {
                    keys.remove();
                    changed = true;
                }
            }
            if (changed) {
                try {
                    writeRoot(context, root);
                } catch (IOException ignored) {
                }
            }
        }
    }

    private static JSONObject readRoot(Context context) {
        File file = getSettingsFile(context);
        if (!file.exists() || !file.isFile()) {
            return createEmptyRoot();
        }
        try (BufferedInputStream in = new BufferedInputStream(new FileInputStream(file))) {
            byte[] buffer = readAllBytes(in);
            if (buffer.length == 0) {
                return createEmptyRoot();
            }
            JSONObject root = new JSONObject(new String(buffer, StandardCharsets.UTF_8));
            ensureGamesObject(root);
            if (!root.has(KEY_VERSION)) {
                root.put(KEY_VERSION, CURRENT_VERSION);
            }
            return root;
        } catch (Exception ignored) {
            return createEmptyRoot();
        }
    }

    private static void writeRoot(Context context, JSONObject root) throws IOException {
        if (context == null || root == null) {
            return;
        }
        File file = getSettingsFile(context);
        File parent = file.getParentFile();
        if (parent != null && !parent.exists()) {
            parent.mkdirs();
        }
        try (BufferedOutputStream out = new BufferedOutputStream(new FileOutputStream(file, false))) {
            root.put(KEY_VERSION, CURRENT_VERSION);
            out.write(root.toString(2).getBytes(StandardCharsets.UTF_8));
            out.flush();
        } catch (JSONException e) {
            throw new IOException(e);
        }
    }

    private static JSONObject ensureGamesObject(JSONObject root) {
        JSONObject games = root.optJSONObject(KEY_GAMES);
        if (games == null) {
            games = new JSONObject();
            try {
                root.put(KEY_GAMES, games);
            } catch (JSONException ignored) {
            }
        }
        return games;
    }

    private static JSONObject createEmptyRoot() {
        JSONObject root = new JSONObject();
        try {
            root.put(KEY_VERSION, CURRENT_VERSION);
            root.put(KEY_GAMES, new JSONObject());
        } catch (JSONException ignored) {
        }
        return root;
    }

    private static File getSettingsFile(Context context) {
        File base = DataDirectoryManager.getDataRoot(context != null ? context : NativeApp.getContext());
        return new File(base, FILE_NAME);
    }

    private static byte[] readAllBytes(BufferedInputStream in) throws IOException {
        byte[] buffer = new byte[8192];
        int read;
        java.io.ByteArrayOutputStream bos = new java.io.ByteArrayOutputStream();
        while ((read = in.read(buffer)) != -1) {
            bos.write(buffer, 0, read);
        }
        return bos.toByteArray();
    }
}
