package com.armsx2;

import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.net.Uri;
import android.text.TextUtils;

import java.util.Locale;

import kr.co.iefriends.pcsx2.NativeApp;

/**
 * Repoints the RetroAchievements client at a loopback proxy (e.g. an
 * offline RA proxy) over an intent. The native side persists the host in
 * the [Achievements] Host setting that CreateClient already reads, so a
 * change made while the emulator is running survives the next cold start.
 *
 * When the broadcast arrives before the native library is initialized the
 * desired action is queued in SharedPreferences and replayed from
 * {@link #applyPending(Context)} once NativeApp.initializeOnce has run.
 *
 * Only http loopback hosts (127.0.0.1 / localhost) with an explicit port
 * are accepted; anything else clears the override.
 */
public class RetroAchievementsHostOverrideReceiver extends BroadcastReceiver {
    private static final String ACTION_SET_SUFFIX = ".action.SET_RETROACHIEVEMENTS_HOST_OVERRIDE";
    private static final String ACTION_CLEAR_SUFFIX = ".action.CLEAR_RETROACHIEVEMENTS_HOST_OVERRIDE";

    private static final String PREFS_NAME = "retroachievements_host_override";
    private static final String KEY_PENDING_HOST = "pending_host";
    private static final String KEY_PENDING_CLEAR = "pending_clear";

    public static final String EXTRA_HOST = "host";

    @Override
    public void onReceive(Context context, Intent intent) {
        if (intent == null || intent.getAction() == null) {
            return;
        }

        final String packageName = context.getPackageName();
        final String action = intent.getAction();

        if ((packageName + ACTION_CLEAR_SUFFIX).equals(action)) {
            applyClear(context);
            setResultCode(Activity.RESULT_OK);
            return;
        }

        if (!(packageName + ACTION_SET_SUFFIX).equals(action)) {
            return;
        }

        final String host = normalizeHost(intent.getStringExtra(EXTRA_HOST));
        if (host == null) {
            applyClear(context);
            setResultCode(Activity.RESULT_OK);
            return;
        }

        applySet(context, host);
        setResultCode(Activity.RESULT_OK);
    }

    /** Replays a queued override once native is up. Called from NativeApp.initializeOnce. */
    public static void applyPending(Context context) {
        SharedPreferences prefs = sharedPreferences(context);
        String host = prefs.getString(KEY_PENDING_HOST, null);
        boolean pendingClear = prefs.getBoolean(KEY_PENDING_CLEAR, false);

        if (!TextUtils.isEmpty(host)) {
            NativeApp.setAchievementsHostOverride(host);
        } else if (pendingClear) {
            NativeApp.clearAchievementsHostOverride();
        } else {
            return;
        }

        clearPending(context);
    }

    private static void applySet(Context context, String host) {
        sharedPreferences(context).edit()
                .putString(KEY_PENDING_HOST, host)
                .remove(KEY_PENDING_CLEAR)
                .apply();
        flushIfNativeReady(context);
    }

    private static void applyClear(Context context) {
        sharedPreferences(context).edit()
                .remove(KEY_PENDING_HOST)
                .putBoolean(KEY_PENDING_CLEAR, true)
                .apply();
        flushIfNativeReady(context);
    }

    private static void flushIfNativeReady(Context context) {
        if (NativeApp.getContext() != null) {
            applyPending(context);
        }
    }

    private static void clearPending(Context context) {
        sharedPreferences(context).edit()
                .remove(KEY_PENDING_HOST)
                .remove(KEY_PENDING_CLEAR)
                .apply();
    }

    private static SharedPreferences sharedPreferences(Context context) {
        return context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);
    }

    private static String normalizeHost(String value) {
        String trimmed = value == null ? "" : value.trim();
        if (trimmed.isEmpty()) {
            return null;
        }

        String candidate = trimmed.contains("://") ? trimmed : "http://" + trimmed;
        Uri uri = Uri.parse(candidate);
        if (uri == null) {
            return null;
        }

        String scheme = uri.getScheme();
        String host = uri.getHost();
        if (scheme == null || host == null) {
            return null;
        }

        String normalizedScheme = scheme.toLowerCase(Locale.US);
        String normalizedHost = host.toLowerCase(Locale.US);
        if (!"http".equals(normalizedScheme)) {
            return null;
        }

        if (!"127.0.0.1".equals(normalizedHost) && !"localhost".equals(normalizedHost)) {
            return null;
        }

        int port = uri.getPort();
        if (port < 1 || port > 65535) {
            return null;
        }

        String authority = uri.getEncodedAuthority();
        if ((authority != null && authority.contains("@"))
                || !TextUtils.isEmpty(uri.getEncodedQuery())
                || !TextUtils.isEmpty(uri.getEncodedFragment())) {
            return null;
        }

        String path = uri.getEncodedPath();
        if (!TextUtils.isEmpty(path) && !"/".equals(path)) {
            return null;
        }

        return normalizedScheme + "://" + normalizedHost + ":" + port;
    }
}
