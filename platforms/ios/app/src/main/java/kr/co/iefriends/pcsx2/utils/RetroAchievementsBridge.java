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

import android.os.Handler;
import android.os.Looper;
import android.text.TextUtils;
import android.util.Log;

import java.util.Objects;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.ThreadFactory;

public final class RetroAchievementsBridge {

    private static final String TAG = "RetroAchievementsBridge";

    public interface Listener {
        void onStateUpdated(State state);

        void onLoginRequested(int reason);

        void onLoginSuccess(String username, int points, int softPoints, int unreadMessages);

        void onHardcoreModeChanged(boolean enabled);
    }

    public interface LoginCallback {
        void onResult(boolean success, String message);
    }

    static final int LOGIN_REASON_USER = 0;
    public static final int LOGIN_REASON_TOKEN_INVALID = 1;

    public static final class State {
        public final boolean achievementsEnabled;
        public final boolean loggedIn;
        public final String username;
        public final String displayName;
        public final String avatarPath;
        public final int points;
        public final int softcorePoints;
        public final int unreadMessages;
        public final boolean hardcorePreference;
        public final boolean hardcoreActive;
        public final boolean hasActiveGame;
        public final String gameTitle;
        public final String richPresence;
        public final String gameIconPath;
        public final int unlockedAchievements;
        public final int totalAchievements;
        public final int unlockedPoints;
        public final int totalPoints;
        public final int gameId;
        final boolean hasAchievements;
        public final boolean hasLeaderboards;
        final boolean hasRichPresence;

        State(boolean achievementsEnabled,
              boolean loggedIn,
              String username,
              String displayName,
              String avatarPath,
              int points,
              int softcorePoints,
              int unreadMessages,
              boolean hardcorePreference,
              boolean hardcoreActive,
              boolean hasActiveGame,
              String gameTitle,
              String richPresence,
              String gameIconPath,
              int unlockedAchievements,
              int totalAchievements,
              int unlockedPoints,
              int totalPoints,
              int gameId,
              boolean hasAchievements,
              boolean hasLeaderboards,
              boolean hasRichPresence) {
            this.achievementsEnabled = achievementsEnabled;
            this.loggedIn = loggedIn;
            this.username = username != null ? username : "";
            this.displayName = !TextUtils.isEmpty(displayName) ? displayName : this.username;
            this.avatarPath = avatarPath != null ? avatarPath : "";
            this.points = points;
            this.softcorePoints = softcorePoints;
            this.unreadMessages = unreadMessages;
            this.hardcorePreference = hardcorePreference;
            this.hardcoreActive = hardcoreActive;
            this.hasActiveGame = hasActiveGame;
            this.gameTitle = gameTitle != null ? gameTitle : "";
            this.richPresence = richPresence != null ? richPresence : "";
            this.gameIconPath = gameIconPath != null ? gameIconPath : "";
            this.unlockedAchievements = unlockedAchievements;
            this.totalAchievements = totalAchievements;
            this.unlockedPoints = unlockedPoints;
            this.totalPoints = totalPoints;
            this.gameId = gameId;
            this.hasAchievements = hasAchievements;
            this.hasLeaderboards = hasLeaderboards;
            this.hasRichPresence = hasRichPresence;
        }
    }

    private static final Handler sMainHandler = new Handler(Looper.getMainLooper());
    private static final ExecutorService sExecutor = Executors.newSingleThreadExecutor(new ThreadFactory() {
        @Override
        public Thread newThread(Runnable r) {
            Thread thread = new Thread(r, "RA-Bridge");
            thread.setDaemon(true);
            return thread;
        }
    });
    private static Listener sListener;
    private static State sLastState = new State(false, false, "", "", "", 0, 0, 0,
            false, false, false, "", "", "", 0, 0, 0, 0, 0, false, false, false);

    private RetroAchievementsBridge() {
    }

    public static synchronized void setListener(Listener listener) {
        sListener = listener;
        if (listener != null && sLastState != null) {
            postToMain(() -> listener.onStateUpdated(sLastState));
        }
    }

    public static void refreshState() {
        executeBackground(RetroAchievementsBridge::nativeRequestState);
    }

    public static void login(String username, String password, LoginCallback callback) {
        final String trimmedUser = username != null ? username.trim() : "";
        final String pwd = password != null ? password : "";
        if (TextUtils.isEmpty(trimmedUser) || TextUtils.isEmpty(pwd)) {
            if (callback != null) {
                callback.onResult(false, "Username and password are required.");
            }
            return;
        }

        executeBackground(() -> {
            String error = nativeLogin(trimmedUser, pwd);
            boolean success = TextUtils.isEmpty(error);
            postToMain(() -> {
                if (callback != null) {
                    callback.onResult(success, success ? null : error);
                }
            });
        });
    }

    public static void logout() {
        executeBackground(() -> {
            nativeLogout();
            nativeRequestState();
        });
    }

    public static void setEnabled(boolean enabled) {
        executeBackground(() -> {
            nativeSetEnabled(enabled);
            nativeRequestState();
        });
    }

    public static void setHardcore(boolean enabled) {
        executeBackground(() -> {
            nativeSetHardcore(enabled);
            nativeRequestState();
        });
    }

    public static State getLastState() {
        synchronized (RetroAchievementsBridge.class) {
            return sLastState;
        }
    }

    private static void executeBackground(Runnable runnable) {
        Runnable safeRunnable = () -> {
            try {
                Objects.requireNonNull(runnable).run();
            } catch (Throwable t) {
                Log.e(TAG, "RA background task failed", t);
            }
        };
        sExecutor.execute(safeRunnable);
    }

    private static void postToMain(Runnable runnable) {
        sMainHandler.post(runnable);
    }

    @SuppressWarnings("unused") // Called from native
    static void notifyLoginRequested(int reason) {
        postToMain(() -> {
            Listener listener = sListener;
            if (listener != null) {
                listener.onLoginRequested(reason);
            }
        });
    }

    @SuppressWarnings("unused") // Called from native
    static void notifyLoginSuccess(String username, int points, int softPoints, int unreadMessages) {
        postToMain(() -> {
            Listener listener = sListener;
            if (listener != null) {
                listener.onLoginSuccess(username, points, softPoints, unreadMessages);
            }
        });
    }

    @SuppressWarnings("unused") // Called from native
    static void notifyStateChanged(boolean achievementsEnabled,
                                   boolean loggedIn,
                                   String username,
                                   String displayName,
                                   String avatarPath,
                                   int points,
                                   int softPoints,
                                   int unreadMessages,
                                   boolean hardcorePreference,
                                   boolean hardcoreActive,
                                   boolean hasActiveGame,
                                   String gameTitle,
                                   String richPresence,
                                   String gameIconPath,
                                   int unlockedAchievements,
                                   int totalAchievements,
                                   int unlockedPoints,
                                   int totalPoints,
                                   int gameId,
                                   boolean hasAchievements,
                                   boolean hasLeaderboards,
                                   boolean hasRichPresence) {
        State state = new State(achievementsEnabled, loggedIn, username, displayName, avatarPath, points, softPoints,
                unreadMessages, hardcorePreference, hardcoreActive, hasActiveGame, gameTitle, richPresence,
                gameIconPath, unlockedAchievements, totalAchievements, unlockedPoints, totalPoints, gameId,
                hasAchievements, hasLeaderboards, hasRichPresence);

        synchronized (RetroAchievementsBridge.class) {
            sLastState = state;
        }

        postToMain(() -> {
            Listener listener = sListener;
            if (listener != null) {
                listener.onStateUpdated(state);
            }
        });
    }

    @SuppressWarnings("unused") // Called from native
    static void notifyHardcoreModeChanged(boolean enabled) {
        postToMain(() -> {
            Listener listener = sListener;
            if (listener != null) {
                listener.onHardcoreModeChanged(enabled);
            }
        });
    }

    private static native void nativeRequestState();

    private static native String nativeLogin(String username, String password);

    private static native void nativeLogout();

    private static native void nativeSetEnabled(boolean enabled);

    private static native void nativeSetHardcore(boolean enabled);
}
