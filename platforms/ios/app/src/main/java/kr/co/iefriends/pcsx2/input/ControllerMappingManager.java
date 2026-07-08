package kr.co.iefriends.pcsx2.input;

import android.content.Context;
import android.content.SharedPreferences;
import android.util.SparseArray;
import android.util.SparseIntArray;
import android.view.KeyEvent;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;

import java.util.EnumMap;
import java.util.Map;

import kr.co.iefriends.pcsx2.R;

public final class ControllerMappingManager {
    public static final int NO_MAPPING = -1;

    private static final String PREFS_NAME = "controller_mapping_prefs";
    private static final String KEY_PREFIX = "action_";

    private static SharedPreferences sPrefs;
    private static final Map<Action, Integer> sActionToKey = new EnumMap<>(Action.class);
    private static final SparseIntArray sKeyToPad = new SparseIntArray();
    private static final SparseArray<Action> sPadCodeToAction = new SparseArray<>();

    private ControllerMappingManager() {
    }

    public static synchronized void init(@NonNull Context context) {
        if (sPrefs == null) {
            sPrefs = context.getApplicationContext().getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);
        }
        for (Action action : Action.values()) {
            sPadCodeToAction.put(action.getPadCode(), action);
            int defaultKey = action.getDefaultKeyCode();
            int stored = sPrefs.getInt(KEY_PREFIX + action.name(), defaultKey);
            sActionToKey.put(action, stored);
            if (stored != NO_MAPPING) {
                sKeyToPad.put(stored, action.getPadCode());
            }
        }
    }

    public static synchronized int getPadCodeForKey(int keyCode) {
        return sKeyToPad.get(keyCode, NO_MAPPING);
    }

    public static synchronized int getAssignedKeyCode(@NonNull Action action) {
        Integer stored = sActionToKey.get(action);
        if (stored != null) {
            return stored;
        }
        return action.getDefaultKeyCode();
    }

    public static synchronized void assign(@NonNull Action action, int keyCode) {
        if (keyCode == NO_MAPPING) {
            clear(action);
            return;
        }

        for (Action a : Action.values()) {
            Integer currentKey = sActionToKey.get(a);
            if (currentKey != null && currentKey == keyCode) {
                sActionToKey.put(a, NO_MAPPING);
                sKeyToPad.delete(keyCode);
                persist(a, NO_MAPPING);
            }
        }

        Integer previous = sActionToKey.get(action);
        if (previous != null && previous != NO_MAPPING) {
            sKeyToPad.delete(previous);
        }

        sActionToKey.put(action, keyCode);
        sKeyToPad.put(keyCode, action.getPadCode());
        persist(action, keyCode);
    }

    public static synchronized void clear(@NonNull Action action) {
        Integer previous = sActionToKey.get(action);
        if (previous != null && previous != NO_MAPPING) {
            sKeyToPad.delete(previous);
        }
        sActionToKey.put(action, NO_MAPPING);
        persist(action, NO_MAPPING);
    }

    public static synchronized void resetToDefaults() {
        if (sPrefs == null) {
            return;
        }
        sActionToKey.clear();
        sKeyToPad.clear();
        SharedPreferences.Editor editor = sPrefs.edit();
        for (Action action : Action.values()) {
            int key = action.getDefaultKeyCode();
            sActionToKey.put(action, key);
            if (key != NO_MAPPING) {
                sKeyToPad.put(key, action.getPadCode());
            }
            editor.putInt(KEY_PREFIX + action.name(), key);
        }
        editor.apply();
    }

    private static void persist(@NonNull Action action, int keyCode) {
        if (sPrefs == null) {
            return;
        }
        sPrefs.edit().putInt(KEY_PREFIX + action.name(), keyCode).apply();
    }

    public static synchronized Map<Action, Integer> getCurrentMappings() {
        return new EnumMap<>(sActionToKey);
    }

    public static String keyCodeToDisplay(int keyCode) {
        if (keyCode == NO_MAPPING) {
            return null;
        }
        String raw = KeyEvent.keyCodeToString(keyCode);
        if (raw == null) {
            return null;
        }
        if (raw.startsWith("KEYCODE_")) {
            raw = raw.substring(8);
        }
        return raw.replace('_', ' ');
    }

    public static synchronized boolean isPadCodeBound(int padCode) {
        Action action = findActionByPadCode(padCode);
        if (action == null) {
            return true;
        }
        Integer keyCode = sActionToKey.get(action);
        return keyCode == null || keyCode != NO_MAPPING;
    }

    @Nullable
    public static synchronized Action findActionByPadCode(int padCode) {
        return sPadCodeToAction.get(padCode);
    }

    public enum Action {
        DPAD_UP(R.string.controller_action_dpad_up, KeyEvent.KEYCODE_DPAD_UP),
        DPAD_DOWN(R.string.controller_action_dpad_down, KeyEvent.KEYCODE_DPAD_DOWN),
        DPAD_LEFT(R.string.controller_action_dpad_left, KeyEvent.KEYCODE_DPAD_LEFT),
        DPAD_RIGHT(R.string.controller_action_dpad_right, KeyEvent.KEYCODE_DPAD_RIGHT),
        CROSS(R.string.controller_action_cross, KeyEvent.KEYCODE_BUTTON_A),
        CIRCLE(R.string.controller_action_circle, KeyEvent.KEYCODE_BUTTON_B),
        SQUARE(R.string.controller_action_square, KeyEvent.KEYCODE_BUTTON_X),
        TRIANGLE(R.string.controller_action_triangle, KeyEvent.KEYCODE_BUTTON_Y),
        L1(R.string.controller_action_l1, KeyEvent.KEYCODE_BUTTON_L1),
        L2(R.string.controller_action_l2, KeyEvent.KEYCODE_BUTTON_L2),
        R1(R.string.controller_action_r1, KeyEvent.KEYCODE_BUTTON_R1),
        R2(R.string.controller_action_r2, KeyEvent.KEYCODE_BUTTON_R2),
        L3(R.string.controller_action_l3, KeyEvent.KEYCODE_BUTTON_THUMBL),
        R3(R.string.controller_action_r3, KeyEvent.KEYCODE_BUTTON_THUMBR),
        SELECT(R.string.controller_action_select, KeyEvent.KEYCODE_BUTTON_SELECT),
        START(R.string.controller_action_start, KeyEvent.KEYCODE_BUTTON_START);

        private final @StringRes int labelRes;
        private final int defaultKeyCode;

        Action(@StringRes int labelRes, int defaultKeyCode) {
            this.labelRes = labelRes;
            this.defaultKeyCode = defaultKeyCode;
        }

        public @StringRes int getLabelRes() {
            return labelRes;
        }

        public int getDefaultKeyCode() {
            return defaultKeyCode;
        }

        public int getPadCode() {
            return defaultKeyCode;
        }
    }
}
