
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

import android.app.Activity;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.res.AssetManager;
import android.os.Handler;
import android.os.Looper;
import android.text.TextUtils;
import android.util.Log;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.InputStream;
import java.lang.reflect.Method;
import java.util.HashMap;
import java.util.Map;
import java.lang.ref.WeakReference;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

import org.json.JSONObject;

import kr.co.iefriends.pcsx2.BuildConfig;
import kr.co.iefriends.pcsx2.NativeApp;


public final class DiscordBridge {
    private static final String TAG = "DiscordBridge";
    private static final String CREDS_FILE = "discord_creds.env";
    private static final String PREFS_NAME = "discord_bridge";
    private static final String KEY_ACCESS_TOKEN = "access_token";
    private static final String KEY_REFRESH_TOKEN = "refresh_token";
    private static final String KEY_TOKEN_TYPE = "token_type";
    private static final String KEY_SCOPE = "scope";
    private static final String KEY_EXPIRES_AT = "expires_at";
    private static final String KEY_USER_ID = "user_id";
    private static final String KEY_USERNAME = "user_name";
    private static final String KEY_AVATAR_URL = "avatar_url";
    private static final String AVATAR_ENDPOINT = "https://discord.com/api/v10/users/@me";
    private static final String CDN_AVATAR_BASE = "https://cdn.discordapp.com/avatars/";
    private static final String CDN_DEFAULT_AVATAR_BASE = "https://cdn.discordapp.com/embed/avatars/";
    private static final String SDK_INIT_CLASS_NAME = "com.discord.socialsdk.DiscordSocialSdkInit";
    private static final Method sSetEngineActivityMethod;
    private static final boolean sSdkAvailable;

    static {
        Method method = null;
        boolean available = false;
        if (BuildConfig.DISCORD_INTEGRATION_ENABLED) {
            try {
                Class<?> clazz = Class.forName(SDK_INIT_CLASS_NAME);
                method = clazz.getMethod("setEngineActivity", Activity.class);
                available = true;
            } catch (Throwable t) {
                Log.i(TAG, "Discord SDK not found. Discord integration disabled.", t);
            }
        } else {
            Log.i(TAG, "Discord integration disabled at build time; skipping SDK lookup.");
        }
        sSetEngineActivityMethod = method;
        sSdkAvailable = available;
    }

    private static Context sAppContext;
    private static boolean sInitialized;
    private static long sApplicationId;
    private static String sScheme = "armsx2";
    private static String sDisplayName = "ARMSX2";
    private static String sLargeImageKey = "app_icon";
    private static boolean sLoggedIn;
    private static String sUserId;
    private static String sUsername;
    private static String sAvatarUrl;
    private static DiscordStateListener sListener;
    private static final Handler sMainHandler = new Handler(Looper.getMainLooper());
    private static final long FOREGROUND_DEBOUNCE_MS = 400L;
    private static int sForegroundActivityCount;
    private static boolean sAppInForeground;
    private static final long CALLBACK_POLL_INTERVAL_MS = 50L;
    private static final Runnable sCallbackPump = new Runnable() {
        @Override
        public void run() {
            if (!sInitialized) {
                return;
            }
            nativePollCallbacks();
            sMainHandler.postDelayed(this, CALLBACK_POLL_INTERVAL_MS);
        }
    };
    private static WeakReference<Activity> sLastResumedActivity = new WeakReference<>(null);
    private static boolean sPendingForeground;
    private static final Runnable sBackgroundRunnable = new Runnable() {
        @Override
        public void run() {
            boolean shouldNotify = false;
            synchronized (DiscordBridge.class) {
                if (!sInitialized || sForegroundActivityCount != 0 || !sAppInForeground) {
                    return;
                }
                sAppInForeground = false;
                shouldNotify = true;
            }
            if (shouldNotify) {
                nativeSetAppForeground(false);
            }
        }
    };
    private static final ExecutorService sAvatarExecutor = Executors.newSingleThreadExecutor();
    private static boolean sAvatarFetchInProgress;

    public interface DiscordStateListener {
        void onLoginStateChanged(boolean loggedIn);

        void onError(String message);

        void onUserInfoUpdated(String username);
    }

    private static native void nativeConfigure(long appId, String scheme, String displayName, String imageKey);

    private static native void nativeProvideStoredToken(String accessToken, String refreshToken, String tokenType,
                                                        long expiresAtSeconds, String scope);

    private static native void nativeBeginAuthorize();

    private static native void nativeClearTokens();

    private static native boolean nativeIsLoggedIn();

    private static native boolean nativeIsClientReady();

    private static native String nativeConsumeLastError();

    private static native void nativePollCallbacks();

    private static native void nativeSetAppForeground(boolean isForeground);

    private DiscordBridge() {
    }

    public static boolean isAvailable() {
        return sSdkAvailable && sApplicationId != 0L;
    }

    private static void setEngineActivityInternal(Activity activity) {
        if (!sSdkAvailable || activity == null || sSetEngineActivityMethod == null) {
            return;
        }
        try {
            sSetEngineActivityMethod.invoke(null, activity);
        } catch (Throwable t) {
            Log.w(TAG, "Failed to notify Discord SDK of activity", t);
        }
    }

    public static void updateEngineActivity(Activity activity) {
        setEngineActivityInternal(activity);
    }

    private static String redactToken(String token) {
        if (TextUtils.isEmpty(token)) {
            return "<empty>";
        }
        if (token.length() <= 8) {
            return "<len=" + token.length() + ">";
        }
        return token.substring(0, 4) + "…" + token.substring(token.length() - 4) + " (len=" + token.length() + ")";
    }

    private static String normalizeScheme(String rawScheme, long appId) {
        String scheme = TextUtils.isEmpty(rawScheme) ? "" : rawScheme.trim();
        int protocolIdx = scheme.indexOf("://");
        if (protocolIdx >= 0) {
            scheme = scheme.substring(0, protocolIdx);
        }
        int colonIdx = scheme.indexOf(':');
        if (colonIdx >= 0) {
            scheme = scheme.substring(0, colonIdx);
        }
        int slashIdx = scheme.indexOf('/');
        if (slashIdx >= 0) {
            scheme = scheme.substring(0, slashIdx);
        }
        if (TextUtils.isEmpty(scheme) && appId != 0L) {
            scheme = "discord-" + appId;
        }
        return scheme;
    }

    public static synchronized void initialize(Context context) {
        if (sInitialized) {
            Log.d(TAG, "initialize(): already initialized");
            return;
        }
        @SuppressWarnings("unused")
        boolean ensureLibs = NativeApp.hasNoNativeBinary;
        sAppContext = context.getApplicationContext();
        if (!sSdkAvailable) {
            Log.i(TAG, "initialize(): Discord SDK unavailable; skipping setup");
            return;
        }
        loadCredentials(sAppContext.getAssets());
        if (sApplicationId == 0L) {
            Log.w(TAG, "Discord APPLICATION_ID missing. Update discord_creds.env.");
            return;
        }

        Log.d(TAG, "initialize(): appId=" + sApplicationId + " scheme=" + sScheme + " displayName=" + sDisplayName);
        nativeConfigure(sApplicationId, sScheme, sDisplayName, sLargeImageKey);
        loadStoredTokens();
        sLoggedIn = nativeIsLoggedIn();
        Log.d(TAG, "initialize(): native login state = " + sLoggedIn);
        sForegroundActivityCount = 0;
        sAppInForeground = false;
        sInitialized = true;
        startCallbackPump();
        if (sPendingForeground) {
            Activity last = sLastResumedActivity != null ? sLastResumedActivity.get() : null;
            sPendingForeground = false;
            if (last != null) {
                setEngineActivityInternal(last);
                enterForegroundLocked(true);
            }
        }
    }

    public static synchronized void onActivityResumed(Activity activity) {
        sLastResumedActivity = new WeakReference<>(activity);
        setEngineActivityInternal(activity);
        if (!sSdkAvailable) {
            sPendingForeground = false;
            return;
        }
        if (!sInitialized) {
            sPendingForeground = true;
            return;
        }
        enterForegroundLocked(false);
    }

    public static synchronized void onActivityPaused(Activity activity) {
        if (!sInitialized) {
            sPendingForeground = false;
            return;
        }
        if (sForegroundActivityCount > 0) {
            sForegroundActivityCount--;
            if (sForegroundActivityCount == 0) {
                sMainHandler.removeCallbacks(sBackgroundRunnable);
                sMainHandler.postDelayed(sBackgroundRunnable, FOREGROUND_DEBOUNCE_MS);
            }
        }
    }

    private static void enterForegroundLocked(boolean forceNotify) {
        sMainHandler.removeCallbacks(sBackgroundRunnable);
        boolean shouldNotify = forceNotify;
        if (sForegroundActivityCount == 0 && !sAppInForeground) {
            sAppInForeground = true;
            shouldNotify = true;
        }
        sForegroundActivityCount++;
        if (shouldNotify) {
            nativeSetAppForeground(true);
        }
    }

    public static void setListener(DiscordStateListener listener) {
        sListener = listener;
        if (sListener != null && !TextUtils.isEmpty(sUsername)) {
            sListener.onUserInfoUpdated(sUsername);
        }
    }

    public static boolean isLoggedIn() {
        return sLoggedIn;
    }

    public static String getLoggedInUsername() {
        return sUsername;
    }

    public static String getLoggedInAvatarUrl() {
        return sAvatarUrl;
    }

    static boolean isClientReady() {
        if (!sSdkAvailable) {
            return false;
        }
        return nativeIsClientReady();
    }

    public static void beginAuthorize(Activity activity) {
        Log.d(TAG, "beginAuthorize(): initialized=" + sInitialized + " loggedIn=" + sLoggedIn);
        if (!sSdkAvailable) {
            if (sListener != null) {
                sListener.onError("Discord integration is not available.");
            }
            return;
        }
        if (!sInitialized) {
            initialize(activity.getApplicationContext());
            if (!sInitialized) {
                if (sListener != null) {
                    sListener.onError("Discord integration is not configured.");
                }
                return;
            }
        }
        startCallbackPump();
        setEngineActivityInternal(activity);
        nativeBeginAuthorize();
    }

    public static void clearTokens() {
        Log.d(TAG, "clearTokens()");
        if (sSdkAvailable) {
            nativeClearTokens();
        }
        if (sAppContext != null) {
            sAppContext.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE).edit().clear().apply();
        }
        clearStoredUserInfo();
        sLoggedIn = false;
    }

    public static String consumeLastError() {
        if (!sSdkAvailable) {
            return null;
        }
        String error = nativeConsumeLastError();
        if (!TextUtils.isEmpty(error)) {
            Log.w(TAG, "consumeLastError(): " + error);
        }
        return error;
    }

    private static void loadCredentials(AssetManager assets) {
        Map<String, String> values = new HashMap<>();
        try (BufferedReader reader = new BufferedReader(new InputStreamReader(assets.open(CREDS_FILE)))) {
            String line;
            while ((line = reader.readLine()) != null) {
                line = line.trim();
                if (line.isEmpty() || line.startsWith("#")) {
                    continue;
                }
                int idx = line.indexOf('=');
                if (idx <= 0) {
                    continue;
                }
                String key = line.substring(0, idx).trim();
                String value = line.substring(idx + 1).trim();
                values.put(key, value);
            }
        } catch (IOException e) {
            Log.w(TAG, "Unable to read discord_creds.env", e);
        }

        //Log.d(TAG, "loadCredentials(): raw values = " + values);
        try {
            sApplicationId = Long.parseLong(values.getOrDefault("APPLICATION_ID", "0"));
        } catch (NumberFormatException ignored) {
            sApplicationId = 0L;
        }
        sScheme = values.getOrDefault("APPLICATION_SCHEME", sScheme);
        sScheme = normalizeScheme(sScheme, sApplicationId);
        sDisplayName = values.getOrDefault("APPLICATION_DISPLAY_NAME", sDisplayName);
        sLargeImageKey = values.getOrDefault("LARGE_IMAGE_KEY", sLargeImageKey);
    }

    private static void loadStoredTokens() {
        if (sAppContext == null) {
            return;
        }
        SharedPreferences prefs = sAppContext.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);
        String access = prefs.getString(KEY_ACCESS_TOKEN, null);
        String refresh = prefs.getString(KEY_REFRESH_TOKEN, null);
        if (TextUtils.isEmpty(access) || TextUtils.isEmpty(refresh)) {
            return;
        }
        String type = prefs.getString(KEY_TOKEN_TYPE, "Bearer");
        long expiresAt = prefs.getLong(KEY_EXPIRES_AT, 0L);
        String scope = prefs.getString(KEY_SCOPE, "");
        sUserId = prefs.getString(KEY_USER_ID, null);
        sUsername = prefs.getString(KEY_USERNAME, null);
        sAvatarUrl = prefs.getString(KEY_AVATAR_URL, null);
        nativeProvideStoredToken(access, refresh, type, expiresAt, scope);
        //Log.d(TAG, "loadStoredTokens(): restored tokens access=" + redactToken(access) + " refresh=" + redactToken(refresh) +
        //        " type=" + type + " expiresAt=" + expiresAt + " scope=" + scope);
        if (sListener != null && !TextUtils.isEmpty(sUsername)) {
            sListener.onUserInfoUpdated(sUsername);
        }
        fetchUserAvatarAsync();
    }

    private static void persistTokens(String accessToken, String refreshToken, String tokenType,
                                      long expiresAtSeconds, String scope) {
        if (sAppContext == null) {
            return;
        }
        sAppContext.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
                .edit()
                .putString(KEY_ACCESS_TOKEN, accessToken)
                .putString(KEY_REFRESH_TOKEN, refreshToken)
                .putString(KEY_TOKEN_TYPE, tokenType)
                .putLong(KEY_EXPIRES_AT, expiresAtSeconds)
                .putString(KEY_SCOPE, scope)
                .apply();
    }

    private static long computeExpiryEpochSeconds(int expiresInSeconds) {
        long nowSeconds = System.currentTimeMillis() / 1000L;
        return nowSeconds + Math.max(expiresInSeconds, 0);
    }

    private static void persistUserInfo(String userId, String username) {
        String normalizedId = TextUtils.isEmpty(userId) ? null : userId;
        String normalizedName = TextUtils.isEmpty(username) ? null : username;
        boolean changed = !TextUtils.equals(sUserId, normalizedId) || !TextUtils.equals(sUsername, normalizedName);
        if (!changed) {
            return;
        }

        sUserId = normalizedId;
        sUsername = normalizedName;
        if (sAppContext != null) {
            SharedPreferences.Editor editor = sAppContext.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE).edit();
            if (normalizedId != null) {
                editor.putString(KEY_USER_ID, normalizedId);
            } else {
                editor.remove(KEY_USER_ID);
            }
            if (normalizedName != null) {
                editor.putString(KEY_USERNAME, normalizedName);
            } else {
                editor.remove(KEY_USERNAME);
            }
            editor.apply();
        }
        if (sListener != null) {
            sListener.onUserInfoUpdated(sUsername);
        }
        fetchUserAvatarAsync();
    }

    private static void clearStoredUserInfo() {
        sUserId = null;
        sUsername = null;
        updateAvatarUrl(null, false);
        if (sAppContext != null) {
            sAppContext.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
                    .edit()
                    .remove(KEY_USER_ID)
                    .remove(KEY_USERNAME)
                    .apply();
        }
        if (sListener != null) {
            sListener.onUserInfoUpdated(null);
        }
    }

    private static void startCallbackPump() {
        if (!sSdkAvailable) {
            return;
        }
        sMainHandler.removeCallbacks(sCallbackPump);
        sMainHandler.post(sCallbackPump);
    }

    // Called from native
    private static void onTokenUpdatedFromNative(String accessToken, String refreshToken, int tokenType,
                                                 String scope, int expiresInSeconds) {
        //Log.d(TAG, "onTokenUpdatedFromNative(): access=" + redactToken(accessToken) + " refresh=" + redactToken(refreshToken) +
        //        " tokenType=" + tokenType + " scope=" + scope + " expiresIn=" + expiresInSeconds);
        String typeString = (tokenType == 0) ? "Bearer" : "RefreshToken";
        long expiresAt = computeExpiryEpochSeconds(expiresInSeconds);
        persistTokens(accessToken, refreshToken, typeString, expiresAt, scope);
    }

    private static void onUserInfoUpdatedFromNative(String userId, String username) {
        Log.d(TAG, "onUserInfoUpdatedFromNative(): userId=" + userId + " username=" + username);
        persistUserInfo(userId, username);
    }

    // Called from native
    private static void onErrorFromNative(String message) {
        Log.e(TAG, "onErrorFromNative(): " + message);
        if (TextUtils.isEmpty(message)) {
            return;
        }
        if (sListener != null) {
            sListener.onError(message);
        } else {
            Log.w(TAG, "Discord error: " + message);
        }
    }

    // Called from native
    private static void onLoginStateChangedFromNative(boolean loggedIn) {
        Log.d(TAG, "onLoginStateChangedFromNative(): loggedIn=" + loggedIn);
        if (!loggedIn) {
            clearStoredUserInfo();
        }
        sLoggedIn = loggedIn;
        if (sListener != null) {
            sListener.onLoginStateChanged(loggedIn);
        }
    }

    private static void fetchUserAvatarAsync() {
        if (sAppContext == null || TextUtils.isEmpty(sUserId)) {
            return;
        }
        SharedPreferences prefs = sAppContext.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);
        final String accessToken = prefs.getString(KEY_ACCESS_TOKEN, null);
        final String tokenType = prefs.getString(KEY_TOKEN_TYPE, "Bearer");
        if (TextUtils.isEmpty(accessToken) || TextUtils.isEmpty(tokenType) ||
                !"Bearer".equalsIgnoreCase(tokenType)) {
            return;
        }
        synchronized (DiscordBridge.class) {
            if (sAvatarFetchInProgress) {
                return;
            }
            sAvatarFetchInProgress = true;
        }
        sAvatarExecutor.execute(() -> {
            try {
                String url = requestAvatarUrl(tokenType, accessToken);
                if (!TextUtils.isEmpty(url)) {
                    updateAvatarUrl(url, true);
                }
            } catch (Exception e) {
                Log.w(TAG, "fetchUserAvatarAsync(): failed", e);
            } finally {
                synchronized (DiscordBridge.class) {
                    sAvatarFetchInProgress = false;
                }
            }
        });
    }

    private static String requestAvatarUrl(String tokenType, String accessToken) throws IOException {
        HttpURLConnection connection = null;
        try {
            URL url = new URL(AVATAR_ENDPOINT);
            connection = (HttpURLConnection) url.openConnection();
            connection.setConnectTimeout(6000);
            connection.setReadTimeout(6000);
            connection.setRequestMethod("GET");
            connection.setRequestProperty("Authorization", tokenType + " " + accessToken);
            connection.setRequestProperty("User-Agent", "ARMSX2/DiscordAvatar");
            connection.setRequestProperty("Accept", "application/json");
            int responseCode = connection.getResponseCode();
            InputStream stream = (responseCode >= 200 && responseCode < 300)
                    ? connection.getInputStream()
                    : connection.getErrorStream();
            if (stream == null) {
                return null;
            }
            String body = readStream(stream);
            if (responseCode >= 200 && responseCode < 300) {
                try {
                    JSONObject json = new JSONObject(body);
                    String userId = json.optString("id", sUserId);
                    String avatar = json.optString("avatar", null);
                    if (!TextUtils.isEmpty(avatar)) {
                        String format = avatar.startsWith("a_") ? "gif" : "png";
                        return CDN_AVATAR_BASE + userId + "/" + avatar + "." + format + "?size=128";
                    }
                    String discriminator = json.optString("discriminator", null);
                    return buildDefaultAvatarUrl(userId, discriminator);
                } catch (Exception e) {
                    Log.w(TAG, "requestAvatarUrl(): parse failure", e);
                }
            } else {
                Log.w(TAG, "requestAvatarUrl(): HTTP " + responseCode + " body=" + body);
            }
        } finally {
            if (connection != null) {
                connection.disconnect();
            }
        }
        return null;
    }

    private static String readStream(InputStream stream) throws IOException {
        InputStreamReader reader = new InputStreamReader(stream, "UTF-8");
        BufferedReader buffered = new BufferedReader(reader);
        StringBuilder sb = new StringBuilder();
        char[] buf = new char[2048];
        int read;
        while ((read = buffered.read(buf)) != -1) {
            sb.append(buf, 0, read);
        }
        return sb.toString();
    }

    private static String buildDefaultAvatarUrl(String userId, String discriminator) {
        int index = 0;
        if (!TextUtils.isEmpty(discriminator) && TextUtils.isDigitsOnly(discriminator)) {
            try {
                index = Integer.parseInt(discriminator) % 5;
            } catch (NumberFormatException ignored) {
                index = 0;
            }
        } else if (!TextUtils.isEmpty(userId)) {
            try {
                long parsed = Long.parseLong(userId);
                index = (int) (parsed % 5);
            } catch (NumberFormatException ignored) {
                index = 0;
            }
        }
        if (index < 0 || index > 5) {
            index = 0;
        }
        return CDN_DEFAULT_AVATAR_BASE + index + ".png";
    }

    private static void updateAvatarUrl(String avatarUrl, boolean notifyListener) {
        String normalized = TextUtils.isEmpty(avatarUrl) ? null : avatarUrl;
        if (TextUtils.equals(sAvatarUrl, normalized)) {
            return;
        }
        sAvatarUrl = normalized;
        if (sAppContext != null) {
            SharedPreferences.Editor editor = sAppContext.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE).edit();
            if (normalized != null) {
                editor.putString(KEY_AVATAR_URL, normalized);
            } else {
                editor.remove(KEY_AVATAR_URL);
            }
            editor.apply();
        }
        if (notifyListener && sListener != null) {
            sListener.onUserInfoUpdated(sUsername);
        }
    }
}
