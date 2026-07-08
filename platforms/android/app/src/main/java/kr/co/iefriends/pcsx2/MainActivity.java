package kr.co.iefriends.pcsx2;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.res.AssetManager;
import android.content.res.Configuration;
import android.os.Bundle;
import android.text.TextUtils;
import android.util.Log;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.widget.FrameLayout;

import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.annotation.IdRes;
import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;

import com.armsx2.R;

import org.libsdl.app.SDLControllerManager;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;

public class MainActivity extends AppCompatActivity {
    private String m_szGamefile = "";
    private Thread mEmulationThread = null;

    private boolean isThread() {
        if (mEmulationThread != null) {
            Thread.State _thread_state = mEmulationThread.getState();
            return _thread_state == Thread.State.BLOCKED
                    || _thread_state == Thread.State.RUNNABLE
                    || _thread_state == Thread.State.TIMED_WAITING
                    || _thread_state == Thread.State.WAITING;
        }
        return false;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        // Ask the governor to hold sustained clocks instead of boost-then-throttle.
        // Device-dependent: only Pixel and a handful of others honor it, but it's a
        // one-liner and never hurts on devices that ignore it.
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.N) {
            getWindow().setSustainedPerformanceMode(true);
        }

        // Default resources
        copyAssetAll(getApplicationContext(), "bios");
        copyAssetAll(getApplicationContext(), "resources");

        Initialize();

        makeButtonTouch();
/*
        setSurfaceView(new SDLSurface(this));*/
    }

    // Buttons
    void configureOnClickListener(@IdRes int id, View.OnClickListener onClickListener) {
        View view = findViewById(id);
        if (view != null) {
            view.setOnClickListener(onClickListener);
        }
    }

    @SuppressLint("ClickableViewAccessibility")
    void configureOnTouchListener(@IdRes int id, int... keyCodes) {
        View view = findViewById(id);
        if (view != null) {
            view.setOnTouchListener((v, event) -> {
                for (int keyCode : keyCodes) {
                    sendKeyAction(v, event.getAction(), keyCode);
                }
                return true;
            });
        }
    }

    private void makeButtonTouch() {
        // Game file
        configureOnClickListener(R.id.btn_file, v -> {
            // Internal storage
            Intent intent = new Intent(Intent.ACTION_GET_CONTENT);
            intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
            intent.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, false);
            intent.setType("*/*");
            startActivityResultLocalFilePlay.launch(intent);
        });

        // Game save
        configureOnClickListener(R.id.btn_save, v -> {
            if (NativeApp.saveStateToSlot(1)) {
                // Success
            } else {
                // Failed
            }
            NativeApp.resume();
        });

        // Game load
        configureOnClickListener(R.id.btn_load, v -> {
            if (NativeApp.loadStateFromSlot(1)) {
                // Success
            } else {
                // Failed
            }
            NativeApp.resume();
        });

        //////
/*
        // RENDERER
        configureOnClickListener(R.id.btn_ogl, v -> NativeApp.renderGpu(12));
        configureOnClickListener(R.id.btn_vulkan, v -> NativeApp.renderGpu(14));
        configureOnClickListener(R.id.btn_sw, v -> NativeApp.renderGpu(13));
*/

        //////
        // PAD
        configureOnTouchListener(R.id.btn_pad_select, KeyEvent.KEYCODE_BUTTON_SELECT);

        configureOnTouchListener(R.id.btn_pad_start, KeyEvent.KEYCODE_BUTTON_START);

        configureOnTouchListener(R.id.btn_pad_a, KeyEvent.KEYCODE_BUTTON_A);
        configureOnTouchListener(R.id.btn_pad_b, KeyEvent.KEYCODE_BUTTON_B);
        configureOnTouchListener(R.id.btn_pad_x, KeyEvent.KEYCODE_BUTTON_X);
        configureOnTouchListener(R.id.btn_pad_y, KeyEvent.KEYCODE_BUTTON_Y);

        ////
        configureOnTouchListener(R.id.btn_pad_l1, KeyEvent.KEYCODE_BUTTON_L1);
        configureOnTouchListener(R.id.btn_pad_r1, KeyEvent.KEYCODE_BUTTON_R1);

        configureOnTouchListener(R.id.btn_pad_l2, KeyEvent.KEYCODE_BUTTON_L2);
        configureOnTouchListener(R.id.btn_pad_r2, KeyEvent.KEYCODE_BUTTON_R2);

        configureOnTouchListener(R.id.btn_pad_l3, KeyEvent.KEYCODE_BUTTON_THUMBL);
        configureOnTouchListener(R.id.btn_pad_r3, KeyEvent.KEYCODE_BUTTON_THUMBR);

        ////

        final int PAD_L_UP = 110;
        final int PAD_L_RIGHT = 111;
        final int PAD_L_DOWN = 112;
        final int PAD_L_LEFT = 113;

        final int PAD_R_UP = 120;
        final int PAD_R_RIGHT = 121;
        final int PAD_R_DOWN = 122;
        final int PAD_R_LEFT = 123;

        configureOnTouchListener(R.id.btn_pad_joy_lt, PAD_L_UP, PAD_L_LEFT);
        configureOnTouchListener(R.id.btn_pad_joy_t, PAD_L_UP);
        configureOnTouchListener(R.id.btn_pad_joy_rt, PAD_L_UP, PAD_L_RIGHT);
        configureOnTouchListener(R.id.btn_pad_joy_r, PAD_L_RIGHT);
        configureOnTouchListener(R.id.btn_pad_joy_rb, PAD_L_DOWN, PAD_L_RIGHT);
        configureOnTouchListener(R.id.btn_pad_joy_b, PAD_L_DOWN);
        configureOnTouchListener(R.id.btn_pad_joy_lb, PAD_L_DOWN, PAD_L_LEFT);
        configureOnTouchListener(R.id.btn_pad_joy_l, PAD_L_LEFT);

        ////
        configureOnTouchListener(R.id.btn_pad_dir_top, KeyEvent.KEYCODE_DPAD_UP);
        configureOnTouchListener(R.id.btn_pad_dir_bottom, KeyEvent.KEYCODE_DPAD_DOWN);
        configureOnTouchListener(R.id.btn_pad_dir_left, KeyEvent.KEYCODE_DPAD_LEFT);
        configureOnTouchListener(R.id.btn_pad_dir_right, KeyEvent.KEYCODE_DPAD_RIGHT);
    }

    public final ActivityResultLauncher<Intent> startActivityResultLocalFilePlay = registerForActivityResult(
            new ActivityResultContracts.StartActivityForResult(),
            result -> {
                if (result.getResultCode() == Activity.RESULT_OK) {
                    try {
                        Intent _intent = result.getData();
                        if (_intent != null) {
                            m_szGamefile = _intent.getDataString();
                            if (!TextUtils.isEmpty(m_szGamefile)) {
                                restartEmuThread();
                            }
                        }
                    } catch (Exception ignored) {
                    }
                }
            });

    @Override
    public void onConfigurationChanged(@NonNull Configuration p_newConfig) {
        super.onConfigurationChanged(p_newConfig);
    }

    @Override
    protected void onPause() {
        NativeApp.pause();
        NativeApp.flushShaderCache();
        super.onPause();
    }

    @Override
    protected void onResume() {
        NativeApp.resume();
        super.onResume();
    }

    @Override
    protected void onDestroy() {
        NativeApp.shutdown();
        super.onDestroy();
        ////
        if (mEmulationThread != null) {
            try {
                mEmulationThread.join();
                mEmulationThread = null;
            } catch (InterruptedException ignored) {
            }
        }

        int appPid = android.os.Process.myPid();
        android.os.Process.killProcess(appPid);
    }

    /// ///////////////////////////////////////////////////////////////////////////////////////////

    public void Initialize() {
        NativeApp.initializeOnce(getApplicationContext());

        // Set up JNI
        SDLControllerManager.nativeSetupJNI();

        // Initialize state
        SDLControllerManager.initialize();
    }

    private void setSurfaceView(Object p_value) {
        FrameLayout fl_board = findViewById(R.id.fl_board);
        if (fl_board != null) {
            if (fl_board.getChildCount() > 0) {
                fl_board.removeAllViews();
            }
            ////
/*            if (p_value instanceof SDLSurface) {
                fl_board.addView((SDLSurface) p_value);
            }*/
        }
    }

    public void startEmuThread() {
        if (!isThread()) {
            mEmulationThread = new Thread(() -> NativeApp.runVMThread(m_szGamefile));
            mEmulationThread.start();
        }
    }

    private void restartEmuThread() {
        NativeApp.shutdown();
        if (mEmulationThread != null) {
            try {
                mEmulationThread.join();
                mEmulationThread = null;
            } catch (InterruptedException ignored) {
            }
        }
        ////
        startEmuThread();
    }

    /// ///////////////////////////////////////////////////////////////////////////////////////////

/*    @Override
    public boolean onGenericMotionEvent(MotionEvent event) {
        if (SDLControllerManager.isDeviceSDLJoystick(event.getDeviceId())) {
            SDLControllerManager.handleJoystickMotionEvent(event);
            return true;
        }
        return super.onGenericMotionEvent(event);
    }

    @SuppressLint("GestureBackNavigation")
    @Override
    public boolean onKeyDown(int p_keyCode, KeyEvent p_event) {
        if ((p_event.getSource() & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD) {
            if (p_event.getRepeatCount() == 0) {
                SDLControllerManager.onNativePadDown(p_event.getDeviceId(), p_keyCode);
                return true;
            }
        } else {
            if (p_keyCode == KeyEvent.KEYCODE_BACK) {
                finish();
                return true;
            }
        }
        return super.onKeyDown(p_keyCode, p_event);
    }

    @Override
    public boolean onKeyUp(int p_keyCode, KeyEvent p_event) {
        if ((p_event.getSource() & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD) {
            if (p_event.getRepeatCount() == 0) {
                SDLControllerManager.onNativePadUp(p_event.getDeviceId(), p_keyCode);
                return true;
            }
        }
        return super.onKeyUp(p_keyCode, p_event);
    }*/

    public static void sendKeyAction(View p_view, int p_action, int p_keycode) {
        if (p_action == MotionEvent.ACTION_DOWN) {
            p_view.setPressed(true);
            int pad_force = 0;
            if (p_keycode >= 110) {
                float _abs = 90; // Joystic test value
                _abs = Math.min(_abs, 100);
                pad_force = (int) (_abs * 32766.0f / 100);
            }
            NativeApp.setPadButton(p_keycode, pad_force, true);
        } else if (p_action == MotionEvent.ACTION_UP || p_action == MotionEvent.ACTION_CANCEL) {
            p_view.setPressed(false);
            NativeApp.setPadButton(p_keycode, 0, false);
        }
    }

    /// ///////////////////////////////////////////////////////////////////////////////////////////

    public static void copyAssetAll(Context p_context, String srcPath) {
        AssetManager assetMgr = p_context.getAssets();
        String[] assets = null;
        try {
            String destPath = p_context.getExternalFilesDir(null) + File.separator + srcPath;
            assets = assetMgr.list(srcPath);
            if (assets != null) {
                if (assets.length == 0) {
                    copyFile(p_context, srcPath, destPath);
                } else {
                    File dir = new File(destPath);
                    if (!dir.exists())
                        dir.mkdir();
                    for (String element : assets) {
                        copyAssetAll(p_context, srcPath + File.separator + element);
                    }
                }
            }
        } catch (IOException ignored) {
        }
    }

    public static void copyFile(Context p_context, String srcFile, String destFile) {
        AssetManager assetMgr = p_context.getAssets();

        InputStream is = null;
        FileOutputStream os = null;
        // Files that MUST always reflect the current APK assets, even on an app
        // UPDATE where the on-disk copy already exists (copyFile otherwise skips
        // existing files): shader sources (a C++ entry-point bump needs the new
        // GLSL or the compiler reports "main() is missing"), the GameDB
        // (GameIndex.yaml — shipped per-game fixes like True Crime's must reach
        // users who already have an old copy, or the fix never applies), AND our
        // GameDB override (armsx2_overrides.yaml — the whole point of that file is
        // shipping per-game fixes like VP2/LEGO Batman; without force-refresh an
        // existing user keeps the original copy and override edits never land).
        final boolean forceFresh = srcFile.contains("shaders")
                || srcFile.endsWith("GameIndex.yaml")
                || srcFile.endsWith("armsx2_overrides.yaml");
        try {
            is = assetMgr.open(srcFile);
            File destFileObj = new File(destFile);
            boolean _exists = destFileObj.exists();
            if (forceFresh) {
                // Delete first so a partial/denied write can't silently fall back
                // to stale content.
                if (_exists && !destFileObj.delete()) {
                    Log.w("ARMSX2", "copyFile: failed to delete stale asset " + destFile);
                }
                _exists = false;
            }
            if (!_exists) {
                os = new FileOutputStream(destFile);

                byte[] buffer = new byte[1024];
                int read;
                while ((read = is.read(buffer)) != -1) {
                    os.write(buffer, 0, read);
                }
                is.close();
                os.flush();
                os.close();
            }
        } catch (IOException e) {
            Log.e("ARMSX2", "copyFile failed: " + srcFile + " -> " + destFile + ": " + e.getMessage());
            try { if (is != null) is.close(); } catch (IOException ignored) {}
            try { if (os != null) os.close(); } catch (IOException ignored) {}
        }
    }
}
