package kr.co.iefriends.pcsx2.reactnative;

import android.app.Activity;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.facebook.react.bridge.Arguments;
import com.facebook.react.bridge.Promise;
import com.facebook.react.bridge.ReactApplicationContext;
import com.facebook.react.bridge.ReactContextBaseJavaModule;
import com.facebook.react.bridge.ReactMethod;
import com.facebook.react.bridge.WritableMap;
import com.facebook.react.modules.core.DeviceEventManagerModule;

import kr.co.iefriends.pcsx2.NativeApp;
import kr.co.iefriends.pcsx2.utils.DataDirectoryManager;
import kr.co.iefriends.pcsx2.utils.DiscordBridge;
import kr.co.iefriends.pcsx2.utils.RetroAchievementsBridge;

public class Armsx2NativeModule extends ReactContextBaseJavaModule
        implements RetroAchievementsBridge.Listener, DiscordBridge.DiscordStateListener {

    static final String NAME = "Armsx2Bridge";
    private static final String EVENT_RA_STATE = "armsx2.retroAchievements";
    private static final String EVENT_RA_LOGIN = "armsx2.retroAchievementsLogin";
    private static final String EVENT_DISCORD = "armsx2.discord";

    private final ReactApplicationContext reactContext;

    Armsx2NativeModule(ReactApplicationContext context) {
        super(context);
        this.reactContext = context;
        NativeApp.initializeOnce(context.getApplicationContext());
        DiscordBridge.initialize(context);
        RetroAchievementsBridge.setListener(this);
        DiscordBridge.setListener(this);
    }

    @NonNull
    @Override
    public String getName() {
        return NAME;
    }

    @Override
    public void initialize() {
        super.initialize();
        emitRetroAchievementsState(RetroAchievementsBridge.getLastState());
        emit(EVENT_DISCORD, buildDiscordState());
    }

    @Override
    public void invalidate() {
        RetroAchievementsBridge.setListener(null);
        DiscordBridge.setListener(null);
        super.invalidate();
    }

    private boolean ensureNativeAvailable(Promise promise) {
        if (NativeApp.hasNoNativeBinary) {
            promise.reject("armsx2_native_missing", "Native ARMSX2 binary not bundled in this build.");
            return false;
        }
        return true;
    }

    private void emit(String event, @Nullable WritableMap payload) {
        if (!reactContext.hasActiveReactInstance()) {
            return;
        }
        reactContext
                .getJSModule(DeviceEventManagerModule.RCTDeviceEventEmitter.class)
                .emit(event, payload);
    }

    private void emitRetroAchievementsState(@Nullable RetroAchievementsBridge.State state) {
        emit(EVENT_RA_STATE, serializeRetroAchievements(state));
    }

    private WritableMap serializeRetroAchievements(@Nullable RetroAchievementsBridge.State state) {
        WritableMap map = Arguments.createMap();
        if (state == null) {
            map.putBoolean("loggedIn", false);
            return map;
        }
        map.putBoolean("achievementsEnabled", state.achievementsEnabled);
        map.putBoolean("loggedIn", state.loggedIn);
        map.putString("username", state.username);
        map.putString("displayName", state.displayName);
        map.putString("avatarPath", state.avatarPath);
        map.putInt("points", state.points);
        map.putInt("softcorePoints", state.softcorePoints);
        map.putInt("unreadMessages", state.unreadMessages);
        map.putBoolean("hardcorePreference", state.hardcorePreference);
        map.putBoolean("hardcoreActive", state.hardcoreActive);
        map.putBoolean("hasActiveGame", state.hasActiveGame);
        map.putString("gameTitle", state.gameTitle);
        map.putString("richPresence", state.richPresence);
        map.putString("gameIconPath", state.gameIconPath);
        map.putInt("unlockedAchievements", state.unlockedAchievements);
        map.putInt("totalAchievements", state.totalAchievements);
        map.putInt("unlockedPoints", state.unlockedPoints);
        map.putInt("totalPoints", state.totalPoints);
        map.putInt("gameId", state.gameId);
        map.putBoolean("hasLeaderboards", state.hasLeaderboards);
        return map;
    }

    private WritableMap buildDiscordState() {
        WritableMap map = Arguments.createMap();
        map.putBoolean("available", DiscordBridge.isAvailable());
        map.putBoolean("loggedIn", DiscordBridge.isLoggedIn());
        map.putString("username", DiscordBridge.getLoggedInUsername());
        map.putString("avatarUrl", DiscordBridge.getLoggedInAvatarUrl());
        return map;
    }

    // region RetroAchievements
    @ReactMethod
    public void getRetroAchievementsState(Promise promise) {
        promise.resolve(serializeRetroAchievements(RetroAchievementsBridge.getLastState()));
    }

    @ReactMethod
    public void refreshRetroAchievementsState(Promise promise) {
        RetroAchievementsBridge.refreshState();
        promise.resolve(null);
    }

    @ReactMethod
    public void loginRetroAchievements(String username, String password, Promise promise) {
        RetroAchievementsBridge.login(username, password, (success, message) -> {
            WritableMap map = Arguments.createMap();
            map.putBoolean("success", success);
            if (!TextUtils.isEmpty(message)) {
                map.putString("message", message);
            }
            promise.resolve(map);
        });
    }

    @ReactMethod
    public void logoutRetroAchievements(Promise promise) {
        RetroAchievementsBridge.logout();
        promise.resolve(null);
    }

    @ReactMethod
    public void setRetroAchievementsEnabled(boolean enabled, Promise promise) {
        RetroAchievementsBridge.setEnabled(enabled);
        promise.resolve(null);
    }

    @ReactMethod
    public void setRetroAchievementsHardcore(boolean enabled, Promise promise) {
        RetroAchievementsBridge.setHardcore(enabled);
        promise.resolve(null);
    }
    // endregion

    // region Discord
    @ReactMethod
    public void getDiscordProfile(Promise promise) {
        promise.resolve(buildDiscordState());
    }

    @ReactMethod
    public void beginDiscordLogin(Promise promise) {
        Activity activity = getCurrentActivity();
        if (activity == null) {
            promise.reject("armsx2_no_activity", "No activity to start Discord auth");
            return;
        }
        if (!DiscordBridge.isAvailable()) {
            promise.reject("armsx2_discord_unavailable", "Discord SDK not bundled for this build");
            return;
        }
        DiscordBridge.beginAuthorize(activity);
        promise.resolve(true);
    }

    @ReactMethod
    public void logoutDiscord(Promise promise) {
        DiscordBridge.clearTokens();
        promise.resolve(true);
    }
    // endregion

    // region Settings / core
    @ReactMethod
    public void getSetting(String section, String key, String type, Promise promise) {
        if (!ensureNativeAvailable(promise)) return;
        promise.resolve(NativeApp.getSetting(section, key, type));
    }

    @ReactMethod
    public void setSetting(String section, String key, String type, String value, Promise promise) {
        if (!ensureNativeAvailable(promise)) return;
        NativeApp.setSetting(section, key, type, value);
        promise.resolve(true);
    }

    @ReactMethod
    public void refreshBIOS(Promise promise) {
        if (!ensureNativeAvailable(promise)) return;
        NativeApp.refreshBIOS();
        promise.resolve(null);
    }

    @ReactMethod
    public void hasValidVm(Promise promise) {
        if (!ensureNativeAvailable(promise)) return;
        promise.resolve(NativeApp.hasValidVm());
    }

    @ReactMethod
    public void setPadVibration(boolean enabled, Promise promise) {
        if (!ensureNativeAvailable(promise)) return;
        NativeApp.setPadVibration(enabled);
        promise.resolve(null);
    }

    @ReactMethod
    public void convertIsoToChd(String path, Promise promise) {
        if (!ensureNativeAvailable(promise)) return;
        if (!NativeApp.hasNativeTools) {
            promise.reject("armsx2_native_tools_missing", "Native tools library not bundled in this build.");
            return;
        }
        new Thread(() -> {
            int result = NativeApp.convertIsoToChd(path);
            promise.resolve(result);
        }, "ARMSX2-CHD").start();
    }

    @ReactMethod
    public void getDataRoot(Promise promise) {
        promise.resolve(DataDirectoryManager.getDataRoot(reactContext).getAbsolutePath());
    }

    @ReactMethod
    public void setDataRootOverride(String path, Promise promise) {
        NativeApp.setDataRootOverride(path);
        NativeApp.reinitializeDataRoot(path);
        promise.resolve(path);
    }
    // endregion

    // region Bridge callbacks
    @Override
    public void onStateUpdated(RetroAchievementsBridge.State state) {
        emitRetroAchievementsState(state);
    }

    @Override
    public void onLoginRequested(int reason) {
        WritableMap map = Arguments.createMap();
        map.putInt("reason", reason);
        emit(EVENT_RA_LOGIN, map);
    }

    @Override
    public void onLoginSuccess(String username, int points, int softPoints, int unreadMessages) {
        emitRetroAchievementsState(RetroAchievementsBridge.getLastState());
    }

    @Override
    public void onHardcoreModeChanged(boolean enabled) {
        WritableMap map = Arguments.createMap();
        map.putBoolean("hardcoreEnabled", enabled);
        emit(EVENT_RA_STATE, map);
    }

    @Override
    public void onLoginStateChanged(boolean loggedIn) {
        emit(EVENT_DISCORD, buildDiscordState());
    }

    @Override
    public void onError(String message) {
        WritableMap map = buildDiscordState();
        if (!TextUtils.isEmpty(message)) {
            map.putString("error", message);
        }
        emit(EVENT_DISCORD, map);
    }

    @Override
    public void onUserInfoUpdated(String username) {
        emit(EVENT_DISCORD, buildDiscordState());
    }
    // endregion
}
