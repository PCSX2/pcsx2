package kr.co.iefriends.pcsx2.utils;

import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.net.Uri;
import android.text.TextUtils;

import java.util.Locale;

import kr.co.iefriends.pcsx2.NativeApp;

public class RetroAchievementsHostOverrideReceiver extends BroadcastReceiver {
    private static final String ACTION_SET_HOST_OVERRIDE_SUFFIX = ".action.SET_RETROACHIEVEMENTS_HOST_OVERRIDE";
    private static final String ACTION_CLEAR_HOST_OVERRIDE_SUFFIX = ".action.CLEAR_RETROACHIEVEMENTS_HOST_OVERRIDE";
    private static final String PREFS_NAME = "retroachievements_host_override";
    private static final String KEY_HOST = "host";
    private static final String KEY_HARDCORE_RESTORE_PRESENT = "hardcore_restore_present";
    private static final String KEY_HARDCORE_RESTORE_VALUE = "hardcore_restore_value";
    private static final String KEY_PENDING_HARDCORE_RESTORE_PRESENT = "pending_hardcore_restore_present";
    private static final String KEY_PENDING_HARDCORE_RESTORE_VALUE = "pending_hardcore_restore_value";
    private static final String KEY_LAST_KNOWN_HARDCORE = "last_known_hardcore";

    public static final String EXTRA_HOST = "host";

    @Override
    public void onReceive(Context context, Intent intent) {
        if (intent == null || intent.getAction() == null) {
            return;
        }

        String packageName = context.getPackageName();
        String setAction = packageName + ACTION_SET_HOST_OVERRIDE_SUFFIX;
        String clearAction = packageName + ACTION_CLEAR_HOST_OVERRIDE_SUFFIX;

        if (clearAction.equals(intent.getAction())) {
            clearHostOverride(context);
            setResultCode(Activity.RESULT_OK);
            return;
        }

        if (!setAction.equals(intent.getAction())) {
            return;
        }

        String rawHost = intent.getStringExtra(EXTRA_HOST);
        if (TextUtils.isEmpty(rawHost) || TextUtils.isEmpty(rawHost.trim())) {
            clearHostOverride(context);
            setResultCode(Activity.RESULT_OK);
            return;
        }

        String host = normalizeHost(rawHost);
        if (host == null) {
            setResultCode(Activity.RESULT_CANCELED);
            return;
        }

        SharedPreferences prefs = sharedPreferences(context);
        boolean hasExistingOverride = !TextUtils.isEmpty(prefs.getString(KEY_HOST, null));

        if (!hasExistingOverride) {
            prefs.edit()
                    .putBoolean(KEY_HARDCORE_RESTORE_PRESENT, true)
                    .putBoolean(KEY_HARDCORE_RESTORE_VALUE, prefs.getBoolean(KEY_LAST_KNOWN_HARDCORE, false))
                    .apply();
        }

        prefs.edit()
                .putString(KEY_HOST, host)
                .remove(KEY_PENDING_HARDCORE_RESTORE_PRESENT)
                .remove(KEY_PENDING_HARDCORE_RESTORE_VALUE)
                .apply();

        if (NativeApp.getContext() != null) {
            NativeApp.setAchievementsHostOverride(host);
        }

        setResultCode(Activity.RESULT_OK);
    }

    public static String getHostOverride(Context context) {
        return sharedPreferences(context).getString(KEY_HOST, null);
    }

    public static void cacheHardcorePreference(Context context, boolean enabled) {
        sharedPreferences(context)
                .edit()
                .putBoolean(KEY_LAST_KNOWN_HARDCORE, enabled)
                .apply();
    }

    public static boolean hasPendingHardcoreRestore(Context context) {
        return sharedPreferences(context).getBoolean(KEY_PENDING_HARDCORE_RESTORE_PRESENT, false);
    }

    public static boolean getPendingHardcoreRestoreValue(Context context) {
        return sharedPreferences(context).getBoolean(KEY_PENDING_HARDCORE_RESTORE_VALUE, false);
    }

    public static void clearPendingHardcoreRestore(Context context) {
        sharedPreferences(context)
                .edit()
                .remove(KEY_PENDING_HARDCORE_RESTORE_PRESENT)
                .remove(KEY_PENDING_HARDCORE_RESTORE_VALUE)
                .apply();
    }

    private static void clearHostOverride(Context context) {
        SharedPreferences prefs = sharedPreferences(context);
        boolean restoreHardcore = prefs.getBoolean(KEY_HARDCORE_RESTORE_VALUE, false);
        boolean hasRestoreValue = prefs.getBoolean(KEY_HARDCORE_RESTORE_PRESENT, false);

        SharedPreferences.Editor editor = prefs.edit()
                .remove(KEY_HOST)
                .remove(KEY_HARDCORE_RESTORE_PRESENT)
                .remove(KEY_HARDCORE_RESTORE_VALUE);

        if (NativeApp.getContext() == null && hasRestoreValue) {
            editor.putBoolean(KEY_PENDING_HARDCORE_RESTORE_PRESENT, true)
                    .putBoolean(KEY_PENDING_HARDCORE_RESTORE_VALUE, restoreHardcore);
        } else {
            editor.remove(KEY_PENDING_HARDCORE_RESTORE_PRESENT)
                    .remove(KEY_PENDING_HARDCORE_RESTORE_VALUE);
        }

        editor.apply();

        if (NativeApp.getContext() != null) {
            NativeApp.clearAchievementsHostOverride(hasRestoreValue, restoreHardcore);
        }
    }

    private static SharedPreferences sharedPreferences(Context context) {
        return context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);
    }

    private static String normalizeHost(String value) {
        String trimmedValue = value == null ? "" : value.trim();
        if (trimmedValue.isEmpty()) {
            return null;
        }

        String candidate = trimmedValue.contains("://") ? trimmedValue : "http://" + trimmedValue;
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
