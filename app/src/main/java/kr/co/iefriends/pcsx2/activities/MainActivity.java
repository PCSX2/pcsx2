package kr.co.iefriends.pcsx2.activities;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.GameManager;
import android.app.GameState;
import android.content.ClipData;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.res.ColorStateList;
import android.content.res.Configuration;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;
import android.os.SystemClock;
import android.provider.OpenableColumns;
import android.provider.Settings;
import android.text.TextUtils;
import android.util.SparseIntArray;
import android.util.TypedValue;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.CompoundButton;
import android.widget.FrameLayout;
import android.widget.ProgressBar;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;

import androidx.activity.OnBackPressedCallback;
import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.annotation.DrawableRes;
import androidx.annotation.IdRes;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.PopupMenu;
import androidx.core.content.ContextCompat;
import androidx.core.util.Pair;
import androidx.core.view.GravityCompat;
import androidx.core.view.WindowCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.core.view.WindowInsetsControllerCompat;
import androidx.core.view.ViewCompat;
import androidx.drawerlayout.widget.DrawerLayout;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.google.android.material.button.MaterialButton;
import com.google.android.material.button.MaterialButtonToggleGroup;
import com.google.android.material.dialog.MaterialAlertDialogBuilder;
import com.google.android.material.floatingactionbutton.FloatingActionButton;
import com.google.android.material.materialswitch.MaterialSwitch;
import com.google.android.material.navigation.NavigationView;
import com.google.android.material.slider.Slider;
import com.google.android.material.textfield.TextInputEditText;
import com.google.android.material.textfield.TextInputLayout;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.BufferedReader;
import java.io.Closeable;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.channels.FileChannel;
import java.security.MessageDigest;
import java.lang.ref.WeakReference;
import java.net.HttpURLConnection;
import java.net.URL;
import java.net.URLEncoder;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Locale;
import java.util.Set;
import java.util.zip.Inflater;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

import kr.co.iefriends.pcsx2.BuildConfig;
import kr.co.iefriends.pcsx2.NativeApp;
import kr.co.iefriends.pcsx2.R;
import kr.co.iefriends.pcsx2.hid.HIDDeviceManager;
import kr.co.iefriends.pcsx2.input.ControllerMappingDialog;
import kr.co.iefriends.pcsx2.input.ControllerMappingManager;
import kr.co.iefriends.pcsx2.input.view.DPadView;
import kr.co.iefriends.pcsx2.input.view.JoystickView;
import kr.co.iefriends.pcsx2.input.view.PSButtonView;
import kr.co.iefriends.pcsx2.input.view.PSShoulderButtonView;
import kr.co.iefriends.pcsx2.utils.DataDirectoryManager;
import kr.co.iefriends.pcsx2.utils.DebugLog;
import kr.co.iefriends.pcsx2.utils.DeviceProfiles;
import kr.co.iefriends.pcsx2.utils.DiscordBridge;
import kr.co.iefriends.pcsx2.utils.GameSpecificSettingsManager;
import kr.co.iefriends.pcsx2.utils.LogcatRecorder;
import kr.co.iefriends.pcsx2.utils.RetroAchievementsBridge;
import kr.co.iefriends.pcsx2.utils.SDLControllerManager;
import kr.co.iefriends.pcsx2.utils.SDLSurface;
import kr.co.iefriends.pcsx2.utils.NetworkAdapterCollector;

public class MainActivity extends AppCompatActivity {
    private String m_szGamefile = "";

    private HIDDeviceManager mHIDDeviceManager;
    private Thread mEmulationThread = null;

    // UI groups for on-screen controls
    private View llPadSelectStart;
    private View llPadRight;
    private DrawerLayout inGameDrawer;
    private FloatingActionButton drawerToggle;
    private FloatingActionButton drawerPauseButton;
    private FloatingActionButton drawerFastForwardButton;
    private MaterialSwitch drawerWidescreenSwitch;
    private View drawerRaSection;
    private TextView drawerRaTitle;
    private TextView drawerRaSubtitle;
    private android.widget.ImageView drawerRaIcon;
    private TextView drawerRaLabel;
    private RetroAchievementsBridge.State currentRetroAchievementsState;
    private boolean lastRetroAchievementsLoggedIn = false;
    private int lastRetroAchievementsGameId = -1;
    private String lastRetroAchievementsIconPath = "";
    private boolean isVmPaused = false;
    private final Runnable hideDrawerToggleRunnable = () -> hideDrawerToggle();
    private boolean isFastForwardEnabled = false;
    private final CompoundButton.OnCheckedChangeListener drawerWidescreenListener =
            (buttonView, isChecked) ->
                    NativeApp.setSetting("EmuCore", "EnableWideScreenPatches", "bool", isChecked ? "true" : "false");

    private static final int RUMBLE_DURATION_MS = 160;
    private static volatile int sLastControllerDeviceId = -1;
    private static volatile boolean sVibrationEnabled = true;
    private static WeakReference<MainActivity> sInstanceRef = new WeakReference<>(null);

    // Home UI
    private DrawerLayout drawerLayout;
    private View homeContainer;
    private View emptyContainer;
    private android.widget.EditText etSearch;
    private android.widget.ImageView bgImage;
    private RecyclerView rvGames;
    private GridLayoutManager gamesGridLayoutManager;
    private SpacingDecoration gameSpacingDecoration;
    private TextView tvEmpty;
    private GamesAdapter gamesAdapter;
    private boolean listMode = false;
    private Uri gamesFolderUri;
    private final Object coverPrefetchLock = new Object();
    private boolean coverPrefetchRunning;
    private boolean storagePromptShown = false;
    private String pendingChdCachePath;
    private String pendingChdDisplayName;
    private Uri pendingChdSourceUri;
    private String pendingChdSourceSerial;
    private String pendingChdSourceTitle;
    private AlertDialog dataDirProgressDialog;
    private static final String PREFS = "armsx2";
    private static final String PREF_GAMES_URI = "games_folder_uri";
    private static final String PREF_CHD_SERIAL_PREFIX = "chd_serial:";
    private static final String PREF_CHD_TITLE_PREFIX = "chd_title:";
    private static final String PREF_ONBOARDING_COMPLETE = "onboarding_complete";
    private static final String PREF_ONSCREEN_UI_STYLE = "on_screen_ui_style";
    private static final String PREF_UI_SCALE_MULTIPLIER = "onscreen_ui_scale_multiplier";
    private static final String STYLE_DEFAULT = "default";
    private static final String STYLE_NETHER = "nether";
    private static final float ONSCREEN_UI_SCALE_MIN = 0.2f;
    private static final float ONSCREEN_UI_SCALE_MAX = 4.0f;
    // Preflight
    private Uri pendingGameUri = null;
    private int pendingLaunchRetries = 0;
    private boolean onboardingLaunched = false;
    private boolean postOnboardingChecksRun = false;
    private String currentOnScreenUiStyle = STYLE_DEFAULT;
    private float onScreenUiScaleMultiplier = 1.0f;
    private float faceButtonsBaseScale = 1.0f;

    @Nullable
    private PerGameOverrideSnapshot lastPerGameOverrideSnapshot = null;
    @Nullable
    private String lastPerGameOverrideKey = null;
    private boolean perGameOverridesActive = false;

    // Auto-hide state
    private enum InputSource { TOUCH, CONTROLLER }
    private InputSource lastInput = InputSource.TOUCH;
    private long lastTouchTimeMs = 0L;
    private long lastControllerTimeMs = 0L;
    // 0 = never hide; seconds otherwise
    private long hideDelayMs = 2500L;
    private static final String PREF_HIDE_CONTROLS_SECONDS = "onscreen_timeout_seconds";

    private static final float ANALOG_DEADZONE = 0.08f;
    private static final float TRIGGER_DEADZONE = 0.04f;
    private final SparseIntArray analogStates = new SparseIntArray();
    private boolean hatUp, hatDown, hatLeft, hatRight;
    private boolean disableTouchControls;

    public static final String EXTRA_SETTINGS_LAYOUT_CHANGED = "SET_LAYOUT_CHANGED";
    public static final String EXTRA_SETTINGS_GPU_PROFILE_OVERRIDE = "SET_GPU_PROFILE_OVERRIDE";
    public static final String EXTRA_SETTINGS_GPU_PROFILE_PERSISTED = "SET_GPU_PROFILE_PERSISTED";
    
    private int currentControllerMode = 0; // 0=2 Sticks, 1=1 Stick+Face, 2=D-Pad Only

    private final OnBackPressedCallback onBackPressCallback =
        new OnBackPressedCallback(false) {
            @Override
            public void handleOnBackPressed() {
                shutdownVmToHome();
            }
        };
    private final OnBackPressedCallback onSearchBackPressCallback =
        new OnBackPressedCallback(true) {
            @Override
            public void handleOnBackPressed() {
                toggleSearchBar();
                remove();
            }
        };

    private final RetroAchievementsBridge.Listener retroAchievementsListener = new RetroAchievementsBridge.Listener() {
        @Override
        public void onStateUpdated(RetroAchievementsBridge.State state) {
            handleRetroAchievementsStateChanged(state);
        }

        @Override
        public void onLoginRequested(int reason) {
            // No in-drawer prompt; handled in settings flow.
        }

        @Override
        public void onLoginSuccess(String username, int points, int softPoints, int unreadMessages) {
            // State refresh will surface the appropriate toast.
        }

        @Override
        public void onHardcoreModeChanged(boolean enabled) {
            RetroAchievementsBridge.refreshState();
        }
    };

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

    private File getCoversCacheDir() {
        File base = DataDirectoryManager.getDataRoot(getApplicationContext());
        if (base == null) {
            return null;
        }
        File dir = new File(base, "armsx2_covers");
        if (!dir.exists() && !dir.mkdirs()) {
            try { DebugLog.e("Covers", "Failed to create cover cache directory: " + dir); } catch (Throwable ignored) {}
            return null;
        }
        return dir;
    }

    private static File getCoversCacheDir(Context ctx) {
        if (ctx == null) {
            return null;
        }
        Context appCtx = ctx.getApplicationContext();
        if (appCtx == null) {
            appCtx = ctx;
        }
        File base = DataDirectoryManager.getDataRoot(appCtx);
        if (base == null) {
            return null;
        }
        File dir = new File(base, "armsx2_covers");
        if (!dir.exists() && !dir.mkdirs()) {
            return null;
        }
        return dir;
    }

    @Nullable
    private static String stripFileExtension(@Nullable String name) {
        if (TextUtils.isEmpty(name)) {
            return name;
        }
        int dot = name.lastIndexOf('.');
        return dot > 0 ? name.substring(0, dot) : name;
    }

    private static String makeChdMetadataKey(@NonNull String prefix, @NonNull Uri uri) {
        return prefix + uri.toString();
    }

    private void persistChdMetadata(@Nullable Uri uri, @Nullable String serial, @Nullable String title) {
        if (uri == null) {
            return;
        }
        android.content.SharedPreferences.Editor editor = getSharedPreferences(PREFS, MODE_PRIVATE).edit();
        String serialValue = TextUtils.isEmpty(serial) ? null : serial.trim();
        String titleValue = TextUtils.isEmpty(title) ? null : title.trim();
        String serialKey = makeChdMetadataKey(PREF_CHD_SERIAL_PREFIX, uri);
        String titleKey = makeChdMetadataKey(PREF_CHD_TITLE_PREFIX, uri);
        if (TextUtils.isEmpty(serialValue)) {
            editor.remove(serialKey);
        } else {
            editor.putString(serialKey, serialValue);
        }
        if (TextUtils.isEmpty(titleValue)) {
            editor.remove(titleKey);
        } else {
            editor.putString(titleKey, titleValue);
        }
        editor.apply();
    }

    @Nullable
    private static Pair<String, String> getPersistedChdMetadata(@Nullable Context ctx, @Nullable Uri uri) {
        if (ctx == null || uri == null) {
            return null;
        }
        Context appCtx = ctx.getApplicationContext() != null ? ctx.getApplicationContext() : ctx;
        android.content.SharedPreferences prefs = appCtx.getSharedPreferences(PREFS, MODE_PRIVATE);
        String serial = prefs.getString(makeChdMetadataKey(PREF_CHD_SERIAL_PREFIX, uri), null);
        String title = prefs.getString(makeChdMetadataKey(PREF_CHD_TITLE_PREFIX, uri), null);
        if (TextUtils.isEmpty(serial) && TextUtils.isEmpty(title)) {
            return null;
        }
        return new Pair<>(serial, title);
    }

    private static boolean isChdEntry(@Nullable Uri uri, @Nullable String title) {
        String lowerTitle = title != null ? title.toLowerCase(Locale.US) : "";
        if (lowerTitle.endsWith(".chd")) {
            return true;
        }
        if (uri == null) {
            return false;
        }
        String last = uri.getLastPathSegment();
        if (last != null && last.toLowerCase(Locale.US).endsWith(".chd")) {
            return true;
        }
        String uriString = uri.toString().toLowerCase(Locale.US);
        return uriString.endsWith(".chd") || uriString.contains(".chd?");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        NetworkAdapterCollector.collectAdapters();
        DiscordBridge.updateEngineActivity(this);
        sInstanceRef = new WeakReference<>(this);
        setContentView(R.layout.activity_main);
        disableTouchControls = DeviceProfiles.isTvOrDesktop(this);
	// Keep screen awake during gameplay
	getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        if (Build.VERSION.SDK_INT >= 33) {
            try {
                GameManager gm = (GameManager) getSystemService(Context.GAME_SERVICE);
                if (gm != null) {
                    gm.setGameState(new GameState(false, GameState.MODE_GAMEPLAY_INTERRUPTIBLE));
                }
            } catch (Throwable ignored) {}
        }

        try {
            if (NativeApp.isFullscreenUIEnabled()) {
                setOnScreenControlsVisible(false);
            }
        } catch (Throwable ignored) {}

        // Hide title/action bar explicitly
        if (getSupportActionBar() != null) getSupportActionBar().hide();

        // Force immersive fullscreen
        applyFullscreen();
        DataDirectoryManager.copyAssetAll(getApplicationContext(), "resources");

    Initialize();

    ControllerMappingManager.init(this);
    refreshVibrationPreference();

    // Load on-screen controls hide timeout
    loadHideTimeoutFromPrefs();

    loadOnScreenUiScalePreference();
    currentOnScreenUiStyle = resolveOnScreenUiStylePreference();
        if (!disableTouchControls) {
            makeButtonTouch();
        }

    setSurfaceView(new SDLSurface(this));

        maybeStartOnboardingFlow();

    // Cache on-screen pad containers
    llPadSelectStart = findViewById(R.id.ll_pad_select_start);
    llPadRight = findViewById(R.id.ll_pad_right);
    JoystickView joystickLeft = findViewById(R.id.joystick_left);
    DPadView dpadView = findViewById(R.id.dpad_view);
    setupInGameDrawer();
    setupTouchRevealOverlay();
    // Home UI
    drawerLayout = findViewById(R.id.drawer_root);
    homeContainer = findViewById(R.id.home_container);
    rvGames = findViewById(R.id.rv_games);
    emptyContainer = findViewById(R.id.empty_container);
    tvEmpty = findViewById(R.id.tv_empty);
    etSearch = findViewById(R.id.et_search);
    bgImage = findViewById(R.id.bg_image);
    getOnBackPressedDispatcher().addCallback(onBackPressCallback);
    if (rvGames != null) {
        gamesGridLayoutManager = new GridLayoutManager(this, getGameGridSpanCount());
        rvGames.setLayoutManager(gamesGridLayoutManager);
        gamesAdapter = new GamesAdapter(new ArrayList<>(), entry -> onGameSelected(entry));
        rvGames.setAdapter(gamesAdapter);
        // Controller navigation
    rvGames.setFocusable(true);
    rvGames.setFocusableInTouchMode(true);
        rvGames.setDescendantFocusability(ViewGroup.FOCUS_AFTER_DESCENDANTS);
        gameSpacingDecoration = new SpacingDecoration(getResources().getDimensionPixelSize(R.dimen.game_selector_tile_spacing));
        rvGames.addItemDecoration(gameSpacingDecoration);
        rvGames.setOnFocusChangeListener((v, hasFocus) -> {
            if (hasFocus && gamesAdapter.getItemCount() > 0) {
                rvGames.post(() -> {
                    RecyclerView.ViewHolder vh = rvGames.findViewHolderForAdapterPosition(0);
                    if (vh != null) vh.itemView.requestFocus();
                });
            }
        });
        applyGameGridConfig();
    }
        enforceTouchControlsPolicy();
        // Search text change -> filter
        if (etSearch != null) {
            etSearch.addTextChangedListener(new android.text.TextWatcher() {
                @Override public void beforeTextChanged(CharSequence s, int start, int count, int after) {}
                @Override public void onTextChanged(CharSequence s, int start, int before, int count) {}
                @Override public void afterTextChanged(android.text.Editable s) {
                    if (gamesAdapter != null) gamesAdapter.setFilter(s != null ? s.toString() : "");
                }
            });
        }
        // FAB actions: convert ISO to CHD 
        com.google.android.material.floatingactionbutton.FloatingActionButton fab = findViewById(R.id.fab_actions);
        if (fab != null) {
            fab.setOnClickListener(v -> {
                androidx.appcompat.widget.PopupMenu pm = new androidx.appcompat.widget.PopupMenu(this, v);
                pm.getMenuInflater().inflate(R.menu.menu_fab_actions, pm.getMenu());
                pm.setOnMenuItemClickListener(item -> {
                    if (item.getItemId() == R.id.menu_convert_iso_chd) {
                        startPickIsoForChd();
                        return true;
                    }
                    return false;
                });
                pm.show();
            });
        }
        MaterialButton btnChooseFolder = findViewById(R.id.btn_choose_folder);
        if (btnChooseFolder != null) btnChooseFolder.setOnClickListener(v -> pickGamesFolder());
        androidx.appcompat.widget.Toolbar toolbar = findViewById(R.id.toolbar);
        if (toolbar != null) {
            String displayName = DeviceProfiles.getProductDisplayName(this, getString(R.string.app_name));
            toolbar.setTitle(getString(R.string.home_game_selector_title_format, displayName));
            try {
                androidx.appcompat.graphics.drawable.DrawerArrowDrawable dd = new androidx.appcompat.graphics.drawable.DrawerArrowDrawable(this);
                dd.setProgress(0f); 
                toolbar.setNavigationIcon(dd);
            } catch (Throwable ignored) {}
            toolbar.setNavigationOnClickListener(v -> {
                if (drawerLayout != null) drawerLayout.openDrawer(GravityCompat.START);
            });
            try {
                toolbar.inflateMenu(R.menu.menu_toolbar_home);
                Menu menu = toolbar.getMenu();
                if (menu != null) {
                    MenuItem rnItem = menu.findItem(R.id.action_open_rn);
                    if (rnItem != null) {
                        rnItem.setVisible(BuildConfig.ENABLE_RN);
                        rnItem.setEnabled(BuildConfig.ENABLE_RN);
                    }
                }
                toolbar.setOnMenuItemClickListener(item -> {
                    int itemId = item.getItemId();
                    if (itemId == R.id.action_toggle_search) {
                        toggleSearchBar();
                        return true;
                    } else if (itemId == R.id.action_toggle_view) {
                        listMode = !listMode;
                        if (rvGames != null) {
                            if (listMode) {
                                rvGames.setLayoutManager(new androidx.recyclerview.widget.LinearLayoutManager(this));
                                item.setIcon(R.drawable.ic_view_grid_24);
                            } else {
                                if (gamesGridLayoutManager == null) {
                                    gamesGridLayoutManager = new GridLayoutManager(this, getGameGridSpanCount());
                                }
                                gamesGridLayoutManager.setSpanCount(getGameGridSpanCount());
                                rvGames.setLayoutManager(gamesGridLayoutManager);
                                item.setIcon(R.drawable.ic_view_list_24);
                            }
                            if (gamesAdapter != null) gamesAdapter.setListMode(listMode);
                        }
                        return true;
                    } else if (itemId == R.id.action_open_rn) {
                        if (!BuildConfig.ENABLE_RN) {
                            return true;
                        }
                        try {
                            Class<?> rnClass = Class.forName("kr.co.iefriends.pcsx2.RNActivity");
                            startActivity(new Intent(this, rnClass));
                        } catch (Throwable t) {
                            try { Toast.makeText(this, R.string.home_react_native_unavailable, Toast.LENGTH_SHORT).show(); } catch (Throwable ignored) {}
                        }
                        return true;
                    }
                    return false;
                });
            } catch (Throwable ignored) {}
        }
    // Navigation drawer menus
        NavigationView navStart = findViewById(R.id.nav_view_start);
        NavigationView.OnNavigationItemSelectedListener listener = item -> {
            int id = item.getItemId();
            if (id == R.id.menu_boot_bios) {
                bootBios();
            } else if (id == R.id.menu_manage_bios) {
                showBiosManagerDialog();
            } else if (id == R.id.menu_open_settings) {
                Intent si = new Intent(this, SettingsActivity.class);
                startActivityForResult(si, 7722);
            } else if (id == R.id.menu_choose_folder) {
                pickGamesFolder();
        } else if (id == R.id.menu_refresh) {
            if (gamesFolderUri != null) scanGamesFolder(gamesFolderUri);
            else try { Toast.makeText(this, R.string.home_choose_folder_first, Toast.LENGTH_SHORT).show(); } catch (Throwable ignored) {}
        } else if (id == R.id.menu_covers) {
            promptForCoversUrl();
        } else if (id == R.id.menu_clear_cover_url) {
            setCoversUrlTemplate("");
            try { Toast.makeText(this, R.string.home_cover_url_cleared, Toast.LENGTH_SHORT).show(); } catch (Throwable ignored) {}
            if (gamesFolderUri != null) scanGamesFolder(gamesFolderUri);
        } else if (id == R.id.menu_bg_landscape) {
            pickBackgroundImage(false);
        } else if (id == R.id.menu_bg_portrait) {
            pickBackgroundImage(true);
        } else if (id == R.id.menu_bg_clear) {
            clearBackgroundImages();
        }
        if (drawerLayout != null) drawerLayout.closeDrawers();
        return true;
    };
    if (navStart != null) navStart.setNavigationItemSelectedListener(listener);
    try { if (drawerLayout != null) drawerLayout.setDrawerLockMode(DrawerLayout.LOCK_MODE_LOCKED_CLOSED, GravityCompat.END); } catch (Throwable ignored) {}

    try {
        if (navStart != null && navStart.getHeaderCount() > 0) {
            View header = navStart.getHeaderView(0);
            View img = header.findViewById(R.id.header_image);
            View imgBlur = header.findViewById(R.id.header_image_blur);
            android.graphics.Bitmap bmp = loadHeaderBitmapFromAssets();
            android.graphics.Bitmap blurBmp = loadHeaderBlurBitmapFromAssets();
            if (img instanceof android.widget.ImageView && bmp != null) {
                ((android.widget.ImageView) img).setImageBitmap(bmp);
            }
            android.graphics.Bitmap useForBlur = blurBmp != null ? blurBmp : bmp;
            if (imgBlur instanceof android.widget.ImageView && useForBlur != null) {
                ((android.widget.ImageView) imgBlur).setImageBitmap(useForBlur);
                if (android.os.Build.VERSION.SDK_INT >= 31) {
                    try {
                        imgBlur.setRenderEffect(android.graphics.RenderEffect.createBlurEffect(18f, 18f, android.graphics.Shader.TileMode.CLAMP));
                    } catch (Throwable ignored) {}
                }
            }
        }
    } catch (Throwable ignored) {}

    showHome(true);
    if (tvEmpty != null) tvEmpty.setVisibility(View.VISIBLE);

    try {
        android.content.SharedPreferences sp = getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        String saved = sp.getString(PREF_GAMES_URI, null);
        if (saved != null) {
            gamesFolderUri = Uri.parse(saved);
            scanGamesFolder(gamesFolderUri);
        }
        applySavedBackground();
    } catch (Throwable ignored) {}

    boolean handledLaunch = false;
    try {
        handledLaunch = handleLaunchIntent(getIntent());
    } catch (Throwable ignored) {}
    if (!handledLaunch) {
        try {
            if (getIntent() != null && getIntent().getBooleanExtra("BOOT_BIOS", false)) {
                bootBios();
            }
        } catch (Throwable ignored) {}
    }
    }
    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        setIntent(intent);
        try {
            handleLaunchIntent(intent);
        } catch (Throwable ignored) {}
    }

    private boolean handleLaunchIntent(@Nullable Intent intent) {
        if (intent == null) {
            return false;
        }
        Uri dataUri = null;
        try {
            dataUri = intent.getData();
        } catch (Throwable ignored) {}
        if (dataUri == null) {
            try {
                Object stream = intent.getParcelableExtra(Intent.EXTRA_STREAM);
                if (stream instanceof Uri) {
                    dataUri = (Uri) stream;
                } else if (stream instanceof String) {
                    dataUri = Uri.parse((String) stream);
                }
            } catch (Throwable ignored) {}
            if (dataUri == null) {
                String streamText = intent.getStringExtra(Intent.EXTRA_STREAM);
                if (!TextUtils.isEmpty(streamText)) {
                    try {
                        dataUri = Uri.parse(streamText);
                    } catch (Throwable ignored) {}
                }
            }
        }
        if (dataUri == null) {
            ClipData clipData = intent.getClipData();
            if (clipData != null && clipData.getItemCount() > 0) {
                ClipData.Item item = clipData.getItemAt(0);
                if (item != null) {
                    dataUri = item.getUri();
                    if (dataUri == null && item.getIntent() != null) {
                        dataUri = item.getIntent().getData();
                    }
                }
            }
        }
        if (dataUri == null) {
            try {
                java.util.ArrayList<Uri> streams = intent.getParcelableArrayListExtra(Intent.EXTRA_STREAM);
                if (streams != null && !streams.isEmpty()) {
                    dataUri = streams.get(0);
                }
            } catch (Throwable ignored) {}
        }
        if (dataUri == null) {
            String extraText = intent.getStringExtra(Intent.EXTRA_TEXT);
            if (!TextUtils.isEmpty(extraText)) {
                try {
                    dataUri = Uri.parse(extraText);
                } catch (Throwable ignored) {}
            }
        }
        if (dataUri == null) {
            return false;
        }
        String action = intent.getAction();
        if (!TextUtils.isEmpty(action)) {
            if (!(Intent.ACTION_VIEW.equals(action)
                    || Intent.ACTION_SEND.equals(action)
                    || Intent.ACTION_SEND_MULTIPLE.equals(action)
                    || Intent.ACTION_MAIN.equals(action))) {
                return false;
            }
        }
        if (TextUtils.isEmpty(dataUri.getScheme())) {
            String path = dataUri.getPath();
            if (!TextUtils.isEmpty(path)) {
                try {
                    dataUri = Uri.fromFile(new File(path));
                } catch (Throwable ignored) {}
            }
        }
        launchGameWithPreflight(dataUri);
        return true;
    }
    private void toggleSearchBar() {
        if (etSearch == null) return;
        boolean nowVisible = etSearch.getVisibility() != View.VISIBLE;
        etSearch.setVisibility(nowVisible ? View.VISIBLE : View.GONE);
        if (nowVisible) {
            getOnBackPressedDispatcher().addCallback(onSearchBackPressCallback);
            etSearch.requestFocus();
            try {
                android.view.inputmethod.InputMethodManager imm = (android.view.inputmethod.InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
                if (imm != null) imm.showSoftInput(etSearch, android.view.inputmethod.InputMethodManager.SHOW_IMPLICIT);
            } catch (Throwable ignored) {}
        } else {
            onSearchBackPressCallback.remove();
            try {
                android.view.inputmethod.InputMethodManager imm = (android.view.inputmethod.InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
                if (imm != null) imm.hideSoftInputFromWindow(etSearch.getWindowToken(), 0);
            } catch (Throwable ignored) {}
            etSearch.clearFocus();
        }
    }

    // region Covers
    private static final String PREF_COVERS_URL = "covers_url_template";
    private static final String PREF_MANUAL_COVER_PREFIX = "manual_cover:"; 
    private String getCoversUrlTemplate() {
        return getSharedPreferences(PREFS, MODE_PRIVATE).getString(PREF_COVERS_URL, "");
    }
    private void setCoversUrlTemplate(String s) {
        getSharedPreferences(PREFS, MODE_PRIVATE).edit().putString(PREF_COVERS_URL, s == null ? "" : s).apply();
    }
    private String getManualCoverUri(String gameKey) {
        try { return getSharedPreferences(PREFS, MODE_PRIVATE).getString(PREF_MANUAL_COVER_PREFIX + gameKey, null); } catch (Throwable ignored) { return null; }
    }
    private void setManualCoverUri(String gameKey, String uri) {
        try {
            getSharedPreferences(PREFS, MODE_PRIVATE).edit().putString(PREF_MANUAL_COVER_PREFIX + gameKey, uri).apply();
            GamesAdapter.clearLocalCoverCache();
        } catch (Throwable ignored) {}
    }
    private void removeManualCoverUri(String gameKey) {
        try {
            getSharedPreferences(PREFS, MODE_PRIVATE).edit().remove(PREF_MANUAL_COVER_PREFIX + gameKey).apply();
            GamesAdapter.clearLocalCoverCache();
        } catch (Throwable ignored) {}
    }
    private void promptForCoversUrl() {
        View dialogView = LayoutInflater.from(this).inflate(R.layout.dialog_cover_template, null);
        TextInputLayout inputLayout = dialogView.findViewById(R.id.input_layout_cover_template);
        TextInputEditText input = dialogView.findViewById(R.id.input_cover_template);
        String previous = getCoversUrlTemplate();
        if (previous != null && input != null) {
            input.setText(previous);
            input.setSelection(previous.length());
        }

        MaterialAlertDialogBuilder builder = new MaterialAlertDialogBuilder(this)
                .setTitle(R.string.cover_template_dialog_title)
                .setView(dialogView)
                .setNegativeButton(android.R.string.cancel, (d, which) -> d.dismiss())
                .setPositiveButton(R.string.action_save, null);

        AlertDialog dialog = builder.create();
        dialog.setOnShowListener(dlg -> {
            dialog.getButton(DialogInterface.BUTTON_POSITIVE).setOnClickListener(v -> {
                String value = "";
                if (input != null && input.getText() != null) {
                    value = input.getText().toString().trim();
                }
                setCoversUrlTemplate(value);
                try { Toast.makeText(this, R.string.cover_template_saved_toast, Toast.LENGTH_SHORT).show(); } catch (Throwable ignored) {}
                if (gamesFolderUri != null) {
                    scanGamesFolder(gamesFolderUri);
                }
                dialog.dismiss();
                if (!TextUtils.isEmpty(value) && !TextUtils.equals(previous, value)) {
                    prefetchCoversAsync(value);
                }
            });
        });
        dialog.show();
        if (inputLayout != null) {
            inputLayout.requestFocus();
        }
    }

    private void prefetchCoversAsync(String template) {
        if (TextUtils.isEmpty(template)) {
            return;
        }
        LinkedHashSet<Uri> roots = collectGameRootUris();
        GamesAdapter.clearLocalCoverCache();
        File cacheDir = getCoversCacheDir();
        if (cacheDir == null) {
            try { Toast.makeText(this, R.string.cover_prefetch_none, Toast.LENGTH_SHORT).show(); } catch (Throwable ignored) {}
            return;
        }
        if (roots.isEmpty()) {
            try { Toast.makeText(this, R.string.cover_prefetch_none, Toast.LENGTH_SHORT).show(); } catch (Throwable ignored) {}
            return;
        }
        if (!hasInternetConnection()) {
            try { Toast.makeText(this, R.string.cover_prefetch_no_connection, Toast.LENGTH_SHORT).show(); } catch (Throwable ignored) {}
            return;
        }
        synchronized (coverPrefetchLock) {
            if (coverPrefetchRunning) {
                try { Toast.makeText(this, R.string.cover_prefetch_running, Toast.LENGTH_SHORT).show(); } catch (Throwable ignored) {}
                return;
            }
            coverPrefetchRunning = true;
        }
        try { Toast.makeText(this, R.string.cover_prefetch_start, Toast.LENGTH_SHORT).show(); } catch (Throwable ignored) {}
            new Thread(() -> {
                int downloaded = 0;
                try {
                    for (Uri root : roots) {
                    downloaded += prefetchCoversForRoot(root, template, cacheDir);
                    }
                } finally {
                    synchronized (coverPrefetchLock) {
                        coverPrefetchRunning = false;
                    }
            }
            final int total = downloaded;
            runOnUiThread(() -> {
                try {
                    if (total > 0) {
                        Toast.makeText(this, getString(R.string.cover_prefetch_done, total), Toast.LENGTH_SHORT).show();
                    } else {
                        Toast.makeText(this, R.string.cover_prefetch_none, Toast.LENGTH_SHORT).show();
                    }
                } catch (Throwable ignored) {}
            });
        }, "CoverPrefetch").start();
    }

    private int prefetchCoversForRoot(Uri root, String template, File cacheDir) {
        if (root == null) {
            return 0;
        }
        if (cacheDir == null) {
            return 0;
        }
        List<GameEntry> entries = GameScanner.scanFolder(this, root);
        if (entries == null || entries.isEmpty()) {
            return 0;
        }
        resolveMetadataForEntries(entries);
        int downloaded = 0;
        Set<String> attempted = new HashSet<>();
        for (GameEntry entry : entries) {
            if (ensureCoverCachedForEntry(cacheDir, entry, template, attempted)) {
                downloaded++;
            }
        }
        return downloaded;
    }

    private void resolveMetadataForEntries(List<GameEntry> entries) {
        if (entries == null || entries.isEmpty()) {
            return;
        }
        android.content.ContentResolver cr = getContentResolver();
        for (GameEntry ge : entries) {
            if (ge == null || ge.uri == null) {
                continue;
            }
            try {
                if (isChdEntry(ge.uri, ge.title)) {
                    Pair<String, String> cached = getPersistedChdMetadata(this, ge.uri);
                    if (cached != null) {
                        if (TextUtils.isEmpty(ge.serial) && !TextUtils.isEmpty(cached.first)) {
                            ge.serial = cached.first;
                        }
                        if (TextUtils.isEmpty(ge.gameTitle) && !TextUtils.isEmpty(cached.second)) {
                            ge.gameTitle = cached.second;
                        }
                    }
                }
                boolean needsSerial = TextUtils.isEmpty(ge.serial);
                boolean needsTitle = TextUtils.isEmpty(ge.gameTitle);
                if (!needsSerial && !needsTitle) {
                    continue;
                }
                RedumpDB.Result rd = RedumpDB.lookupByFile(cr, ge.uri);
                if (rd != null) {
                    if (needsSerial && !TextUtils.isEmpty(rd.serial)) {
                        ge.serial = rd.serial;
                    }
                    if (needsTitle && !TextUtils.isEmpty(rd.name)) {
                        ge.gameTitle = rd.name;
                    }
                    if (isChdEntry(ge.uri, ge.title)) {
                        persistChdMetadata(ge.uri, ge.serial, ge.gameTitle);
                    }
                }
            } catch (Throwable ignored) {}
        }
    }

    private static List<String> buildCoverCandidateUrls(GameEntry entry, String template) {
        if (entry == null || TextUtils.isEmpty(template)) {
            return Collections.emptyList();
        }
        String fileBase = entry.fileTitleNoExt();
        String hyphenized = hyphenizeAlphaDigits(fileBase);
        List<String> variants = makeTitleVariants(fileBase);
        java.util.LinkedHashSet<String> urls = new java.util.LinkedHashSet<>();
        if (template.contains("${filetitle}")) {
            for (String v : variants) {
                urls.add(template.replace("${filetitle}", safeUrlPart(v))
                        .replace("${serial}", "")
                        .replace("${title}", ""));
            }
        }
        if (template.contains("${serial}")) {
            if (!TextUtils.isEmpty(entry.serial)) {
                urls.add(template.replace("${serial}", safeUrlPart(entry.serial))
                        .replace("${filetitle}", "")
                        .replace("${title}", ""));
            }
            if (!TextUtils.isEmpty(hyphenized) && !hyphenized.equals(fileBase)) {
                urls.add(template.replace("${serial}", safeUrlPart(hyphenized))
                        .replace("${filetitle}", "")
                        .replace("${title}", ""));
            }
            for (String v : variants) {
                urls.add(template.replace("${serial}", safeUrlPart(v))
                        .replace("${filetitle}", "")
                        .replace("${title}", ""));
            }
        }
        if (template.contains("${title}")) {
            String resolvedTitle = !TextUtils.isEmpty(entry.gameTitle) ? entry.gameTitle : fileBase;
            java.util.LinkedHashSet<String> titleVariants = new java.util.LinkedHashSet<>(makeTitleVariants(resolvedTitle));
            if (!TextUtils.isEmpty(entry.gameTitle) && !TextUtils.isEmpty(fileBase) && !entry.gameTitle.equals(fileBase)) {
                titleVariants.addAll(makeTitleVariants(fileBase));
            }
            for (String v : titleVariants) {
                urls.add(template.replace("${title}", safeUrlPart(v))
                        .replace("${serial}", "")
                        .replace("${filetitle}", ""));
            }
        }
        return new ArrayList<>(urls);
    }

    private static String safeUrlPart(String s) {
        if (s == null) {
            return "";
        }
        try {
            return URLEncoder.encode(s, "UTF-8");
        } catch (Exception e) {
            return s;
        }
    }

    private static String hyphenizeAlphaDigits(String s) {
        if (s == null) {
            return "";
        }
        try {
            java.util.regex.Matcher m = java.util.regex.Pattern.compile("^([A-Za-z]+)[-_]?([0-9]{3,})$").matcher(s);
            if (m.find()) {
                return (m.group(1).toUpperCase(Locale.US) + "-" + m.group(2));
            }
        } catch (Exception ignored) {}
        return s;
    }

    private static List<String> makeTitleVariants(String base) {
        java.util.LinkedHashSet<String> set = new java.util.LinkedHashSet<>();
        if (base == null) base = "";
        String b0 = base.trim();
        if (!b0.isEmpty()) set.add(b0);
        String b1 = b0.replace('_', ' ').trim(); if (!b1.isEmpty()) set.add(b1);
        String b1Sanitized = b1
                .replaceAll("\\[[^\\]]*\\]", " ")
                .replaceAll("\\([^\\)]*\\)", " ")
                .replaceAll("\\{[^\\}]*\\}", " ")
                .replaceAll("\\s+", " ")
                .trim();
        if (!b1Sanitized.isEmpty()) set.add(b1Sanitized);
        String b2 = b1.replace(":", " - ").replaceAll("\\s+", " ").trim(); if (!b2.isEmpty()) set.add(b2);
        try {
            String b3 = b1.replaceAll("(?i)(?<=\\w) \\–|\\u2014| - (?=\\w)", ": ");
            b3 = b3.replace(" - ", ": ");
            b3 = b3.replaceAll("\\s+", " ").trim();
            if (!b3.isEmpty()) set.add(b3);
            String b4 = b1Sanitized.replace(" - ", ": ").replaceAll("\\s+", " ").trim();
            if (!b4.isEmpty()) set.add(b4);
        } catch (Throwable ignored) {}
        return new ArrayList<>(set);
    }

    private static String sanitizeCoverFileComponent(String input) {
        if (TextUtils.isEmpty(input)) {
            return "";
        }
        String normalized = input.trim();
        normalized = normalized.replaceAll("[\\\\/:*?\"<>|]", " ");
        normalized = normalized.replaceAll("[^A-Za-z0-9._-]", "_");
        normalized = normalized.replaceAll("_+", "_");
        normalized = normalized.replaceAll("^_+|_+$", "");
        return normalized;
    }

    private static String computeCoverBaseName(GameEntry entry) {
        String candidate = entry != null ? entry.serial : null;
        if (TextUtils.isEmpty(candidate) && entry != null) {
            candidate = entry.gameTitle;
        }
        if (TextUtils.isEmpty(candidate) && entry != null) {
            candidate = entry.fileTitleNoExt();
        }
        String sanitized = sanitizeCoverFileComponent(candidate);
        if (TextUtils.isEmpty(sanitized) && entry != null) {
            String fallback = entry.title != null ? entry.title : "cover";
            sanitized = sanitizeCoverFileComponent("cover_" + Integer.toHexString(fallback.hashCode()));
        }
        if (TextUtils.isEmpty(sanitized)) {
            sanitized = "cover";
        }
        return sanitized;
    }

    private static File findExistingCoverFile(File dir, String baseName) {
        if (dir == null || TextUtils.isEmpty(baseName)) {
            return null;
        }
        String prefix = baseName.toLowerCase(Locale.US);
        File[] files = dir.listFiles();
        if (files == null) {
            return null;
        }
        for (File child : files) {
            if (child == null || !child.isFile()) continue;
            String name = child.getName();
            if (name == null) continue;
            String lower = name.toLowerCase(Locale.US);
            if (lower.equals(prefix) || lower.startsWith(prefix + ".")) {
                return child;
            }
        }
        return null;
    }

    private static String guessImageExtension(String url, String contentType) {
        if (contentType != null) {
            String type = contentType.toLowerCase(Locale.US);
            if (type.contains("png")) return ".png";
            if (type.contains("webp")) return ".webp";
            if (type.contains("gif")) return ".gif";
            if (type.contains("jpeg") || type.contains("jpg")) return ".jpg";
        }
        if (url != null) {
            String path = url;
            int query = path.indexOf('?');
            if (query >= 0) {
                path = path.substring(0, query);
            }
            int dot = path.lastIndexOf('.');
            if (dot >= 0 && dot > path.lastIndexOf('/')) {
                String ext = path.substring(dot).toLowerCase(Locale.US);
                if (ext.matches("\\.(jpg|jpeg|png|webp|gif)")) {
                    return ext.equals(".jpeg") ? ".jpg" : ext;
                }
            }
        }
        return ".jpg";
    }

    private boolean downloadCoverToDirectory(File coversDir, String url, String baseName) {
        HttpURLConnection connection = null;
        InputStream in = null;
        OutputStream out = null;
        try {
            connection = (HttpURLConnection) new URL(url).openConnection();
            connection.setConnectTimeout(4000);
            connection.setReadTimeout(6000);
            connection.setInstanceFollowRedirects(true);
            connection.setRequestMethod("GET");
            int code = connection.getResponseCode();
            if (code != HttpURLConnection.HTTP_OK) {
                return false;
            }
            String extension = guessImageExtension(url, connection.getContentType());
            String fileName = baseName + extension;
            File existing = findExistingCoverFile(coversDir, baseName);
            if (existing != null && existing.length() > 0) {
                return false;
            }
            if (existing != null) {
                if (!existing.delete()) {
                    return false;
                }
            }
            File file = new File(coversDir, fileName);
            File parent = file.getParentFile();
            if (parent != null && !parent.exists() && !parent.mkdirs()) {
                return false;
            }
            out = new FileOutputStream(file);
            in = connection.getInputStream();
            byte[] buffer = new byte[8192];
            int read;
            while ((read = in.read(buffer)) != -1) {
                out.write(buffer, 0, read);
            }
            out.flush();
            return true;
        } catch (Exception ignored) {
            return false;
        } finally {
            if (in != null) {
                try { in.close(); } catch (IOException ignored) {}
            }
            if (out != null) {
                try { out.close(); } catch (IOException ignored) {}
            }
            if (connection != null) {
                connection.disconnect();
            }
        }
    }

    private boolean ensureCoverCachedForEntry(File coversDir, GameEntry entry, String template, Set<String> attemptedUrls) {
        List<String> urls = buildCoverCandidateUrls(entry, template);
        if (urls.isEmpty()) {
            return false;
        }
        String baseName = computeCoverBaseName(entry);
        if (TextUtils.isEmpty(baseName)) {
            return false;
        }
        File existing = findExistingCoverFile(coversDir, baseName);
        if (existing != null && existing.length() > 0) {
            return false;
        }
        for (String url : urls) {
            if (TextUtils.isEmpty(url) || url.contains("${")) {
                continue;
            }
            if (attemptedUrls != null && !attemptedUrls.add(url)) {
                continue;
            }
            if (downloadCoverToDirectory(coversDir, url, baseName)) {
                File stored = MainActivity.findExistingCoverFile(coversDir, baseName);
                if (stored != null && stored.isFile()) {
                    GamesAdapter.registerCachedCover(entry, stored);
                }
                try { DebugLog.d("Covers", "Cached cover for " + baseName + " from " + url); } catch (Throwable ignored) {}
                return true;
            }
        }
        return false;
    }

    private LinkedHashSet<Uri> collectGameRootUris() {
        LinkedHashSet<Uri> roots = new LinkedHashSet<>();
        if (gamesFolderUri != null) {
            roots.add(gamesFolderUri);
        }
        android.content.SharedPreferences prefs = getSharedPreferences(PREFS, MODE_PRIVATE);
        String savedPrimary = prefs.getString(PREF_GAMES_URI, null);
        if (gamesFolderUri == null && !TextUtils.isEmpty(savedPrimary)) {
            try { roots.add(Uri.parse(savedPrimary)); } catch (Exception ignored) {}
        }
        java.util.Set<String> secondary = prefs.getStringSet("secondary_game_dirs", null);
        if (secondary != null) {
            for (String uriString : secondary) {
                if (TextUtils.isEmpty(uriString)) continue;
                try { roots.add(Uri.parse(uriString)); } catch (Exception ignored) {}
            }
        }
        return roots;
    }

    private static boolean hasInternetConnection(Context context) {
        if (context == null) {
            return false;
        }
        try {
            android.net.ConnectivityManager cm = (android.net.ConnectivityManager) context.getSystemService(Context.CONNECTIVITY_SERVICE);
            if (cm == null) return false;
            if (android.os.Build.VERSION.SDK_INT >= 23) {
                android.net.Network nw = cm.getActiveNetwork();
                if (nw == null) return false;
                android.net.NetworkCapabilities nc = cm.getNetworkCapabilities(nw);
                return nc != null && (nc.hasTransport(android.net.NetworkCapabilities.TRANSPORT_WIFI)
                        || nc.hasTransport(android.net.NetworkCapabilities.TRANSPORT_CELLULAR)
                        || nc.hasTransport(android.net.NetworkCapabilities.TRANSPORT_ETHERNET));
            } else {
                android.net.NetworkInfo ni = cm.getActiveNetworkInfo();
                return ni != null && ni.isConnected();
            }
        } catch (Throwable ignored) {
            return false;
        }
    }

    private boolean hasInternetConnection() {
        return hasInternetConnection(this);
    }

    // endregion Covers

    // region Manual cover selection
    private static String gameKeyFromEntry(GameEntry e) {
        if (e == null) return "";
        String key = (e.uri != null ? e.uri.toString() : ("file://" + e.title));
        return key;
    }

    private String pendingManualCoverGameKey;

    private final ActivityResultLauncher<Intent> startActivityResultPickImage = registerForActivityResult(
            new ActivityResultContracts.StartActivityForResult(), result -> {
                if (result.getResultCode() == Activity.RESULT_OK && result.getData() != null) {
                    Intent data = result.getData();
                    Uri img = data.getData();
                    if (img != null) {
                        try {
                            final int takeFlags = data.getFlags() & (Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
                            getContentResolver().takePersistableUriPermission(img, takeFlags);
                        } catch (SecurityException ignored) {}
                        String pendingKey = pendingManualCoverGameKey;
                        pendingManualCoverGameKey = null;
                        if (pendingKey != null) {
                            setManualCoverUri(pendingKey, img.toString());
                            if (gamesFolderUri != null) scanGamesFolder(gamesFolderUri);
                        }
                    }
                }
            });

    private final ActivityResultLauncher<Intent> startActivityResultSaveChd = registerForActivityResult(
            new ActivityResultContracts.StartActivityForResult(), result -> {
                if (pendingChdCachePath == null) {
                    android.util.Log.w("ARMSX2_CHD", "Save handler invoked with no pending CHD path");
                    return;
                }

                File chdFile = new File(pendingChdCachePath);
                String cachePath = pendingChdCachePath;
                pendingChdCachePath = null;
                String displayName = pendingChdDisplayName;
                pendingChdDisplayName = null;
                Uri sourceUri = pendingChdSourceUri;
                pendingChdSourceUri = null;
                String sourceSerial = pendingChdSourceSerial;
                pendingChdSourceSerial = null;
                String sourceTitle = pendingChdSourceTitle;
                pendingChdSourceTitle = null;

                if (!chdFile.exists()) {
                    android.util.Log.e("ARMSX2_CHD", "Pending CHD file missing from cache: " + cachePath);
                    showConversionResult(false, "Could not locate the converted CHD file. Please try converting again.");
                    return;
                }

                if (result.getResultCode() == Activity.RESULT_OK && result.getData() != null && result.getData().getData() != null) {
                    Uri destinationUri = result.getData().getData();
                    android.util.Log.d("ARMSX2_CHD", "User selected destination URI: " + destinationUri);
                    boolean saved = saveChdToUri(chdFile, destinationUri);
                    if (saved) {
                        String destinationDisplayName = queryOpenableDisplayName(destinationUri);
                        String persistedTitle = !TextUtils.isEmpty(sourceTitle) ? sourceTitle : stripFileExtension(destinationDisplayName);
                        persistChdMetadata(destinationUri, sourceSerial, persistedTitle);
                        carryCoverAssociationAfterChdSave(
                                sourceUri, destinationUri, displayName, destinationDisplayName, sourceSerial, persistedTitle);
                        if (!chdFile.delete()) {
                            android.util.Log.w("ARMSX2_CHD", "Failed to delete cached CHD after saving: " + cachePath);
                        } else {
                            android.util.Log.d("ARMSX2_CHD", "Deleted cached CHD after successful save");
                        }
                        showConversionResult(true, "CHD saved to the selected location.");
                    } else {
                        showConversionResult(false, "Failed to save CHD. The converted file is still available in the app cache:\n" + cachePath);
                    }
                } else {
                    android.util.Log.i("ARMSX2_CHD", "User cancelled CHD save dialog");
                    showConversionResult(false, "Save cancelled. The converted CHD remains in the app cache:\n" + cachePath);
                }
            });

    private void carryCoverAssociationAfterChdSave(@Nullable Uri sourceUri,
                                                   @Nullable Uri destinationUri,
                                                   @Nullable String sourceDisplayName,
                                                   @Nullable String destinationDisplayName,
                                                   @Nullable String sourceSerial,
                                                   @Nullable String sourceTitle) {
        if (sourceUri == null || destinationUri == null) {
            return;
        }

        String srcName = !TextUtils.isEmpty(sourceDisplayName) ? sourceDisplayName : sourceUri.getLastPathSegment();
        String dstName = !TextUtils.isEmpty(destinationDisplayName) ? destinationDisplayName : destinationUri.getLastPathSegment();
        if (TextUtils.isEmpty(srcName)) srcName = "source.iso";
        if (TextUtils.isEmpty(dstName)) dstName = "destination.chd";

        GameEntry sourceEntry = new GameEntry(srcName, sourceUri);
        sourceEntry.serial = sourceSerial;
        sourceEntry.gameTitle = sourceTitle;

        GameEntry destinationEntry = new GameEntry(dstName, destinationUri);
        destinationEntry.serial = sourceSerial;
        destinationEntry.gameTitle = sourceTitle;

        String sourceGameKey = gameKeyFromEntry(sourceEntry);
        String destinationGameKey = gameKeyFromEntry(destinationEntry);
        if (!TextUtils.isEmpty(sourceGameKey) && !TextUtils.isEmpty(destinationGameKey)
                && !sourceGameKey.equals(destinationGameKey)) {
            String manualCoverUri = getManualCoverUri(sourceGameKey);
            if (!TextUtils.isEmpty(manualCoverUri)) {
                setManualCoverUri(destinationGameKey, manualCoverUri);
            }
        }

        copyCachedCoverBetweenEntries(sourceEntry, destinationEntry);
    }

    private void copyCachedCoverBetweenEntries(@NonNull GameEntry sourceEntry, @NonNull GameEntry destinationEntry) {
        File coversDir = getCoversCacheDir();
        if (coversDir == null) {
            return;
        }
        String sourceBase = computeCoverBaseName(sourceEntry);
        String destinationBase = computeCoverBaseName(destinationEntry);
        if (TextUtils.isEmpty(sourceBase) || TextUtils.isEmpty(destinationBase) || sourceBase.equals(destinationBase)) {
            return;
        }

        File sourceCover = findExistingCoverFile(coversDir, sourceBase);
        if (sourceCover == null || !sourceCover.isFile() || sourceCover.length() <= 0L) {
            return;
        }
        if (findExistingCoverFile(coversDir, destinationBase) != null) {
            return;
        }

        String sourceName = sourceCover.getName();
        int extIndex = sourceName.lastIndexOf('.');
        String ext = extIndex >= 0 ? sourceName.substring(extIndex) : ".jpg";
        File destinationCover = new File(coversDir, destinationBase + ext);
        try (FileInputStream in = new FileInputStream(sourceCover);
             FileOutputStream out = new FileOutputStream(destinationCover)) {
            byte[] buffer = new byte[8192];
            int read;
            while ((read = in.read(buffer)) != -1) {
                out.write(buffer, 0, read);
            }
            out.flush();
            GamesAdapter.registerCachedCover(destinationEntry, destinationCover);
        } catch (IOException ignored) {}
    }

    private void showGameOptionsPopup(View anchorView, GameEntry e) {
        if (e == null) return;
        String key = gameKeyFromEntry(e);
        String existing = getManualCoverUri(key);
        PopupMenu menu = new PopupMenu(this, anchorView);
        menu.inflate(R.menu.game_options_menu);
        MenuItem removeChosenCoverMenuItem = menu.getMenu().findItem(R.id.remove_chosen_cover);
        removeChosenCoverMenuItem.setVisible(existing != null);
        menu.setOnMenuItemClickListener(item -> {
            if (item.getItemId() == R.id.choose_cover) {
                launchCoverImagePicker(key);
                return true;
            }
            if (item.getItemId() == R.id.remove_chosen_cover) {
                removeManualCoverUri(key);
                if (gamesFolderUri != null) scanGamesFolder(gamesFolderUri);
                return true;
            }
            if (item.getItemId() == R.id.per_game_settings) {
                showPerGameSettingsDialog(e);
                return true;
            }
            return false;
        });
        menu.show();
    }

    private void launchCoverImagePicker(String key) {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("image/*");
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
        pendingManualCoverGameKey = key;
        startActivityResultPickImage.launch(intent);
    }

    private void showPerGameSettingsDialog(GameEntry entry) {
        if (entry == null) return;
        String gameKey = gameKeyFromEntry(entry);
        View dialogView = LayoutInflater.from(this).inflate(R.layout.dialog_game_specific_settings, null);

        MaterialSwitch switchEnabled = dialogView.findViewById(R.id.per_game_switch_enabled);
        ViewGroup settingsGroup = dialogView.findViewById(R.id.per_game_settings_group);
        Spinner rendererSpinner = dialogView.findViewById(R.id.per_game_spinner_renderer);
        Spinner aspectSpinner = dialogView.findViewById(R.id.per_game_spinner_aspect_ratio);
        MaterialSwitch switchWidescreen = dialogView.findViewById(R.id.per_game_switch_widescreen);
        MaterialSwitch switchCheats = dialogView.findViewById(R.id.per_game_switch_enable_cheats);
        MaterialSwitch switchNoInterlacing = dialogView.findViewById(R.id.per_game_switch_no_interlacing);
        MaterialSwitch switchLoadTextures = dialogView.findViewById(R.id.per_game_switch_load_textures);
        MaterialSwitch switchAsyncTextures = dialogView.findViewById(R.id.per_game_switch_async_textures);
        MaterialSwitch switchPrecache = dialogView.findViewById(R.id.per_game_switch_precache_textures);
        MaterialSwitch switchShowFps = dialogView.findViewById(R.id.per_game_switch_show_fps);

        boolean globalCheats = readBoolSetting("EmuCore", "EnableCheats", false);
        boolean globalWidescreen = readBoolSetting("EmuCore", "EnableWideScreenPatches", false);
        boolean globalNoInterlacing = readBoolSetting("EmuCore", "EnableNoInterlacingPatches", false);
        boolean globalLoadTextures = readBoolSetting("EmuCore/GS", "LoadTextureReplacements", false);
        boolean globalAsyncTextures = readBoolSetting("EmuCore/GS", "LoadTextureReplacementsAsync", false);
        boolean globalPrecache = readBoolSetting("EmuCore/GS", "PrecacheTextureReplacements", false);
        boolean globalShowFps = readBoolSetting("EmuCore/GS", "OsdShowFPS", false);
        int globalRenderer = getCurrentRendererValue();
        String globalAspect = getCurrentAspectRatioValue();

        GameSpecificSettingsManager.GameSettings existing = GameSpecificSettingsManager.getSettings(this, gameKey);

        boolean initialEnabled = existing != null;
        boolean initialCheats = existing != null && existing.enableCheats != null ? existing.enableCheats : globalCheats;
        boolean initialWidescreen = existing != null && existing.widescreen != null ? existing.widescreen : globalWidescreen;
        boolean initialNoInterlacing = existing != null && existing.noInterlacing != null ? existing.noInterlacing : globalNoInterlacing;
        boolean initialLoadTextures = existing != null && existing.loadTextures != null ? existing.loadTextures : globalLoadTextures;
        boolean initialAsyncTextures = existing != null && existing.asyncTextures != null ? existing.asyncTextures : globalAsyncTextures;
        boolean initialPrecache = existing != null && existing.precacheTextures != null ? existing.precacheTextures : globalPrecache;
        boolean initialShowFps = existing != null && existing.showFps != null ? existing.showFps : globalShowFps;
        int initialRenderer = existing != null && existing.renderer != null ? existing.renderer : globalRenderer;
        String initialAspect = existing != null && !TextUtils.isEmpty(existing.aspectRatio) ? existing.aspectRatio : globalAspect;

        switchEnabled.setChecked(initialEnabled);
        switchCheats.setChecked(initialCheats);
        switchWidescreen.setChecked(initialWidescreen);
        switchNoInterlacing.setChecked(initialNoInterlacing);
        switchLoadTextures.setChecked(initialLoadTextures);
        switchAsyncTextures.setChecked(initialAsyncTextures);
        switchPrecache.setChecked(initialPrecache);
        switchShowFps.setChecked(initialShowFps);

        rendererSpinner.setSelection(rendererSpinnerPositionForValue(initialRenderer), false);

        String[] aspectOptions = getResources().getStringArray(R.array.aspect_ratios);
        int aspectIndex = 0;
        for (int i = 0; i < aspectOptions.length; i++) {
            if (TextUtils.equals(aspectOptions[i], initialAspect)) {
                aspectIndex = i;
                break;
            }
        }
        aspectSpinner.setSelection(aspectIndex, false);

        setGroupEnabled(settingsGroup, initialEnabled);

        switchEnabled.setOnCheckedChangeListener((button, isChecked) -> setGroupEnabled(settingsGroup, isChecked));

        MaterialAlertDialogBuilder builder = new MaterialAlertDialogBuilder(this)
                .setTitle(entry.gameTitle != null ? entry.gameTitle : entry.title)
                .setView(dialogView)
                .setNegativeButton(android.R.string.cancel, (d, w) -> d.dismiss())
                .setPositiveButton(R.string.action_save, null);

        AlertDialog dialog = builder.create();
        dialog.setOnShowListener(dlg -> {
            android.widget.Button saveButton = dialog.getButton(DialogInterface.BUTTON_POSITIVE);
            if (saveButton == null) {
                return;
            }
            saveButton.setOnClickListener(v -> {
                if (!switchEnabled.isChecked()) {
                    GameSpecificSettingsManager.removeSettings(this, gameKey);
                    try { Toast.makeText(this, R.string.per_game_settings_cleared_toast, Toast.LENGTH_SHORT).show(); } catch (Throwable ignored) {}
                    dialog.dismiss();
                    return;
                }

                GameSpecificSettingsManager.GameSettings toSave = new GameSpecificSettingsManager.GameSettings();

                boolean cheatsValue = switchCheats.isChecked();
                if (cheatsValue != globalCheats) toSave.enableCheats = cheatsValue;

                boolean widescreenValue = switchWidescreen.isChecked();
                if (widescreenValue != globalWidescreen) toSave.widescreen = widescreenValue;

                boolean noInterlacingValue = switchNoInterlacing.isChecked();
                if (noInterlacingValue != globalNoInterlacing) toSave.noInterlacing = noInterlacingValue;

                boolean loadTexturesValue = switchLoadTextures.isChecked();
                if (loadTexturesValue != globalLoadTextures) toSave.loadTextures = loadTexturesValue;

                boolean asyncTexturesValue = switchAsyncTextures.isChecked();
                if (asyncTexturesValue != globalAsyncTextures) toSave.asyncTextures = asyncTexturesValue;

                boolean precacheValue = switchPrecache.isChecked();
                if (precacheValue != globalPrecache) toSave.precacheTextures = precacheValue;

                boolean showFpsValue = switchShowFps.isChecked();
                if (showFpsValue != globalShowFps) toSave.showFps = showFpsValue;

                int rendererValue = rendererValueForSpinnerPosition(rendererSpinner.getSelectedItemPosition());
                if (rendererValue != globalRenderer) toSave.renderer = rendererValue;

                String aspectValue = aspectOptions[aspectSpinner.getSelectedItemPosition()];
                if (!TextUtils.equals(aspectValue, globalAspect)) toSave.aspectRatio = aspectValue;

                if (toSave.hasOverrides()) {
                    GameSpecificSettingsManager.saveSettings(this, gameKey, toSave);
                    try { Toast.makeText(this, R.string.per_game_settings_saved_toast, Toast.LENGTH_SHORT).show(); } catch (Throwable ignored) {}
                } else {
                    GameSpecificSettingsManager.removeSettings(this, gameKey);
                    try { Toast.makeText(this, R.string.per_game_settings_cleared_toast, Toast.LENGTH_SHORT).show(); } catch (Throwable ignored) {}
                }
                dialog.dismiss();
            });
        });

        dialog.show();
    }

    private void setGroupEnabled(@Nullable ViewGroup group, boolean enabled) {
        if (group == null) {
            return;
        }
        group.setEnabled(enabled);
        group.setAlpha(enabled ? 1f : 0.38f);
        for (int i = 0; i < group.getChildCount(); i++) {
            View child = group.getChildAt(i);
            child.setEnabled(enabled);
            if (child instanceof ViewGroup) {
                setGroupEnabled((ViewGroup) child, enabled);
            }
        }
    }

    private int rendererSpinnerPositionForValue(int value) {
        switch (value) {
            case 12:
                return 1;
            case 13:
                return 2;
            case 14:
                return 3;
            default:
                return 0;
        }
    }

    private int rendererValueForSpinnerPosition(int position) {
        switch (position) {
            case 1:
                return 12;
            case 2:
                return 13;
            case 3:
                return 14;
            default:
                return -1;
        }
    }

    private int getCurrentRendererValue() {
        int initialValue = -1;
        try {
            String renderer = NativeApp.getSetting("EmuCore/GS", "Renderer", "int");
            if (!TextUtils.isEmpty(renderer)) {
                initialValue = Integer.parseInt(renderer);
            }
        } catch (Exception ignored) {}
        return initialValue;
    }

    private String getCurrentAspectRatioValue() {
        String[] aspectOptions = getResources().getStringArray(R.array.aspect_ratios);
        String defaultValue = aspectOptions.length > 1 ? aspectOptions[1] : aspectOptions[0];
        try {
            String aspect = NativeApp.getSetting("EmuCore/GS", "AspectRatio", "string");
            if (!TextUtils.isEmpty(aspect)) {
                return aspect;
            }
        } catch (Exception ignored) {}
        return defaultValue;
    }

    private void applyPerGameSettingsForEntry(@Nullable GameEntry entry) {
        if (entry == null) {
            return;
        }
        applyPerGameSettingsForKey(gameKeyFromEntry(entry));
    }

    private void applyPerGameSettingsForUri(@Nullable Uri uri) {
        applyPerGameSettingsForKey(uri != null ? uri.toString() : null);
    }

    private void applyPerGameSettingsForKey(@Nullable String gameKey) {
        restorePerGameOverrides();
        if (TextUtils.isEmpty(gameKey)) {
            return;
        }
        GameSpecificSettingsManager.GameSettings settings = GameSpecificSettingsManager.getSettings(this, gameKey);
        if (settings == null || !settings.hasOverrides()) {
            return;
        }

        PerGameOverrideSnapshot snapshot = captureCurrentPerGameSnapshot();
        boolean applied = false;

        if (settings.enableCheats != null) {
            setNativeSetting("EmuCore", "EnableCheats", "bool", boolToString(settings.enableCheats));
            applied = true;
        }
        if (settings.widescreen != null) {
            setNativeSetting("EmuCore", "EnableWideScreenPatches", "bool", boolToString(settings.widescreen));
            applied = true;
        }
        if (settings.noInterlacing != null) {
            setNativeSetting("EmuCore", "EnableNoInterlacingPatches", "bool", boolToString(settings.noInterlacing));
            applied = true;
        }
        if (settings.loadTextures != null) {
            setNativeSetting("EmuCore/GS", "LoadTextureReplacements", "bool", boolToString(settings.loadTextures));
            applied = true;
        }
        if (settings.asyncTextures != null) {
            setNativeSetting("EmuCore/GS", "LoadTextureReplacementsAsync", "bool", boolToString(settings.asyncTextures));
            applied = true;
        }
        if (settings.precacheTextures != null) {
            setNativeSetting("EmuCore/GS", "PrecacheTextureReplacements", "bool", boolToString(settings.precacheTextures));
            applied = true;
        }
        if (settings.showFps != null) {
            setNativeSetting("EmuCore/GS", "OsdShowFPS", "bool", boolToString(settings.showFps));
            applied = true;
        }
        if (settings.renderer != null) {
            setNativeSetting("EmuCore/GS", "Renderer", "int", Integer.toString(settings.renderer));
            applied = true;
        }
        if (!TextUtils.isEmpty(settings.aspectRatio)) {
            setNativeSetting("EmuCore/GS", "AspectRatio", "string", settings.aspectRatio);
            applied = true;
        }

        if (applied) {
            perGameOverridesActive = true;
            lastPerGameOverrideSnapshot = snapshot;
            lastPerGameOverrideKey = gameKey;
        }
    }

    private void restorePerGameOverrides() {
        if (!perGameOverridesActive) {
            lastPerGameOverrideSnapshot = null;
            lastPerGameOverrideKey = null;
            return;
        }
        PerGameOverrideSnapshot snapshot = lastPerGameOverrideSnapshot;
        perGameOverridesActive = false;
        lastPerGameOverrideSnapshot = null;
        lastPerGameOverrideKey = null;
        if (snapshot == null) {
            return;
        }

        setNativeSetting("EmuCore", "EnableCheats", "bool", snapshot.enableCheats);
        setNativeSetting("EmuCore", "EnableWideScreenPatches", "bool", snapshot.widescreen);
        setNativeSetting("EmuCore", "EnableNoInterlacingPatches", "bool", snapshot.noInterlacing);
        setNativeSetting("EmuCore/GS", "LoadTextureReplacements", "bool", snapshot.loadTextures);
        setNativeSetting("EmuCore/GS", "LoadTextureReplacementsAsync", "bool", snapshot.asyncTextures);
        setNativeSetting("EmuCore/GS", "PrecacheTextureReplacements", "bool", snapshot.precacheTextures);
        setNativeSetting("EmuCore/GS", "OsdShowFPS", "bool", snapshot.showFps);
        setNativeSetting("EmuCore/GS", "Renderer", "int", snapshot.renderer);
        setNativeSetting("EmuCore/GS", "AspectRatio", "string", snapshot.aspectRatio);
    }

    private PerGameOverrideSnapshot captureCurrentPerGameSnapshot() {
        String cheats = safeGetSetting("EmuCore", "EnableCheats", "bool");
        if (cheats == null) {
            cheats = boolToString(readBoolSetting("EmuCore", "EnableCheats", false));
        }

        String widescreen = safeGetSetting("EmuCore", "EnableWideScreenPatches", "bool");
        if (widescreen == null) {
            widescreen = boolToString(readBoolSetting("EmuCore", "EnableWideScreenPatches", false));
        }

        String noInterlacing = safeGetSetting("EmuCore", "EnableNoInterlacingPatches", "bool");
        if (noInterlacing == null) {
            noInterlacing = boolToString(readBoolSetting("EmuCore", "EnableNoInterlacingPatches", false));
        }

        String loadTextures = safeGetSetting("EmuCore/GS", "LoadTextureReplacements", "bool");
        if (loadTextures == null) {
            loadTextures = boolToString(readBoolSetting("EmuCore/GS", "LoadTextureReplacements", false));
        }

        String asyncTextures = safeGetSetting("EmuCore/GS", "LoadTextureReplacementsAsync", "bool");
        if (asyncTextures == null) {
            asyncTextures = boolToString(readBoolSetting("EmuCore/GS", "LoadTextureReplacementsAsync", false));
        }

        String precache = safeGetSetting("EmuCore/GS", "PrecacheTextureReplacements", "bool");
        if (precache == null) {
            precache = boolToString(readBoolSetting("EmuCore/GS", "PrecacheTextureReplacements", false));
        }

        String showFps = safeGetSetting("EmuCore/GS", "OsdShowFPS", "bool");
        if (showFps == null) {
            showFps = boolToString(readBoolSetting("EmuCore/GS", "OsdShowFPS", false));
        }

        String renderer = safeGetSetting("EmuCore/GS", "Renderer", "int");
        if (renderer == null) {
            renderer = Integer.toString(getCurrentRendererValue());
        }

        String aspect = safeGetSetting("EmuCore/GS", "AspectRatio", "string");
        if (aspect == null) {
            aspect = getCurrentAspectRatioValue();
        }

        return new PerGameOverrideSnapshot(cheats, widescreen, noInterlacing, loadTextures, asyncTextures, precache, showFps, renderer, aspect);
    }

    @Nullable
    private static String safeGetSetting(String section, String key, String type) {
        try {
            return NativeApp.getSetting(section, key, type);
        } catch (Exception ignored) {
            return null;
        }
    }

    private static void setNativeSetting(String section, String key, String type, @Nullable String value) {
        if (value == null) {
            return;
        }
        try {
            NativeApp.setSetting(section, key, type, value);
        } catch (Exception ignored) {
        }
    }

    private static String boolToString(boolean value) {
        return value ? "true" : "false";
    }

    private static final class PerGameOverrideSnapshot {
        @Nullable final String enableCheats;
        @Nullable final String widescreen;
        @Nullable final String noInterlacing;
        @Nullable final String loadTextures;
        @Nullable final String asyncTextures;
        @Nullable final String precacheTextures;
        @Nullable final String showFps;
        @Nullable final String renderer;
        @Nullable final String aspectRatio;

        PerGameOverrideSnapshot(@Nullable String enableCheats,
                                @Nullable String widescreen,
                                @Nullable String noInterlacing,
                                @Nullable String loadTextures,
                                @Nullable String asyncTextures,
                                @Nullable String precacheTextures,
                                @Nullable String showFps,
                                @Nullable String renderer,
                                @Nullable String aspectRatio) {
            this.enableCheats = enableCheats;
            this.widescreen = widescreen;
            this.noInterlacing = noInterlacing;
            this.loadTextures = loadTextures;
            this.asyncTextures = asyncTextures;
            this.precacheTextures = precacheTextures;
            this.showFps = showFps;
            this.renderer = renderer;
            this.aspectRatio = aspectRatio;
        }
    }

    // endregion Manual cover selection

    

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) applyFullscreen();
        if (Build.VERSION.SDK_INT >= 33 && hasFocus) {
            try {
                GameManager gm = (GameManager) getSystemService(Context.GAME_SERVICE);
                if (gm != null) gm.setGameState(new GameState(false, GameState.MODE_GAMEPLAY_INTERRUPTIBLE));
            } catch (Throwable ignored) {}
        }
    }

    private void applyFullscreen() {
        // 1️⃣ Determine if emulation UI is visible
        boolean emulationVisible = !isHomeVisible();
        boolean fullscreen = emulationVisible || isFullscreenUiModeEnabled();

        // 2️⃣ Edge-to-edge: disable system padding
        WindowCompat.setDecorFitsSystemWindows(getWindow(), false);

        // 3️⃣ Handle display cutout (notch/punch-hole)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            WindowManager.LayoutParams attrs = getWindow().getAttributes();
            attrs.layoutInDisplayCutoutMode = emulationVisible
                    ? WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES
                    : WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT;
            getWindow().setAttributes(attrs);
        }

        // 4️⃣ Get decor view
        View decorView = getWindow().getDecorView();
        applyLegacyImmersiveFlags(decorView, fullscreen);

        // 5️⃣ Disable contrast enforcement on Android Q+
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            getWindow().setNavigationBarContrastEnforced(false);
            getWindow().setStatusBarContrastEnforced(false);
        }

        // 6️⃣ Hide system bars
        WindowInsetsControllerCompat controller = new WindowInsetsControllerCompat(getWindow(), decorView);
        if (fullscreen) {
            controller.hide(WindowInsetsCompat.Type.systemBars()); // status + nav bars
            controller.setSystemBarsBehavior(WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
        } else {
            controller.show(WindowInsetsCompat.Type.systemBars());
            controller.setSystemBarsBehavior(WindowInsetsControllerCompat.BEHAVIOR_DEFAULT);
        }

        // 7️⃣ Consume all insets on root layout so no padding is added
        View root = findViewById(R.id.in_game_root);
        if (root != null) {
            ViewCompat.setOnApplyWindowInsetsListener(root, (v, insets) -> {
                v.setPadding(0, 0, 0, 0); // remove any padding for status/nav/cutout
                return WindowInsetsCompat.CONSUMED;
            });
        }

        // 8️⃣ Optional: touch listener for on-screen controls
        if (fullscreen) {
            decorView.setOnTouchListener((v, e) -> {
                if (disableTouchControls) return false;
                if (e.getAction() == MotionEvent.ACTION_DOWN || e.getAction() == MotionEvent.ACTION_MOVE) {
                    lastInput = InputSource.TOUCH;
                    lastTouchTimeMs = System.currentTimeMillis();
                    if (mEmulationThread != null) {
                        setOnScreenControlsVisible(true);
                        maybeAutoHideControls();
                    }
                    v.performClick();
                }
                return false;
            });
        } else {
            decorView.setOnTouchListener(null);
        }
    }

    private boolean isFullscreenUiModeEnabled() {
        try {
            String value = NativeApp.getSetting("UI", "EnableFullscreenUI", "bool");
            if (!TextUtils.isEmpty(value)) {
                return "true".equalsIgnoreCase(value);
            }
        } catch (Exception ignored) {}
        try {
            return NativeApp.isFullscreenUIEnabled();
        } catch (Throwable ignored) {
            return false;
        }
    }

    private void applyDisplayCutoutMode(boolean emulationVisible) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.P) {
            return;
        }

        final WindowManager.LayoutParams attrs = getWindow().getAttributes();
        final int targetMode;
        if (!emulationVisible) {
            targetMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT;
        } else if (isDisplayCutoutExpansionEnabled()) {
            targetMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
        } else {
            targetMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_NEVER;
        }

        if (attrs.layoutInDisplayCutoutMode != targetMode) {
            attrs.layoutInDisplayCutoutMode = targetMode;
            getWindow().setAttributes(attrs);
        }
    }

    private boolean isDisplayCutoutExpansionEnabled() {
        try {
            String value = NativeApp.getSetting("UI", "ExpandIntoDisplayCutout", "bool");
            return "true".equalsIgnoreCase(value);
        } catch (Exception ignored) {
            return true;
        }
    }

    @SuppressWarnings("deprecation")
    private static void applyLegacyImmersiveFlags(View decorView, boolean fullscreen) {
        int flags = View.SYSTEM_UI_FLAG_LAYOUT_STABLE;
        if (fullscreen) {
            flags |= View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                    | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                    | View.SYSTEM_UI_FLAG_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY;
        }
        decorView.setSystemUiVisibility(flags);
    }

    public void onSurfaceReady() {
    }

    private void ensureBiosPresent() {
    if (!hasBios()) {
        Toast.makeText(this, R.string.home_bios_missing_toast, Toast.LENGTH_LONG).show();
        new MaterialAlertDialogBuilder(this)
            .setMessage(R.string.home_bios_missing_message)
            .setCancelable(true)
            .setNegativeButton(R.string.home_close, (d, w) -> d.dismiss())
            .setPositiveButton(R.string.onboarding_bios_select, (d, w) -> openBiosPicker())
            .show();
        } else {
            // BIOS is present, signal we’re gameplay-ready.
            if (Build.VERSION.SDK_INT >= 33) {
                try {
                    GameManager gm = (GameManager) getSystemService(Context.GAME_SERVICE);
                    if (gm != null) gm.setGameState(new GameState(false, GameState.MODE_GAMEPLAY_INTERRUPTIBLE));
                } catch (Throwable ignored) {}
            }
        }
    }

    private void openBiosPicker() {
        Intent intent = new Intent(Intent.ACTION_GET_CONTENT);
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        intent.setType("application/octet-stream");
        String[] mimeTypes = new String[]{"application/octet-stream", "application/x-binary"};
        intent.putExtra(Intent.EXTRA_MIME_TYPES, mimeTypes);
        startActivityResultPickBios.launch(intent);
    }

    private boolean hasBios() {
        File base = DataDirectoryManager.getDataRoot(getApplicationContext());
        File biosDir = new File(base, "bios");
        if (!biosDir.exists()) return false;
        File[] files = biosDir.listFiles((dir, name) -> name != null && name.toLowerCase().endsWith(".bin"));
        return files != null && files.length > 0;
    }

    private void saveBiosFromUri(Uri uri) {
        Context ctx = getApplicationContext();
        File base = DataDirectoryManager.getDataRoot(ctx);
        File biosDir = new File(base, "bios");
        if (!biosDir.exists()) biosDir.mkdirs();

        String outName = "ps2_bios.bin"; 
        File outFile = new File(biosDir, outName);

        try (InputStream in = getContentResolver().openInputStream(uri);
             OutputStream out = new FileOutputStream(outFile)) {
            if (in == null) throw new IOException("Unable to open BIOS Uri");
            byte[] buf = new byte[8192];
            int n;
            while ((n = in.read(buf)) > 0) out.write(buf, 0, n);
            out.flush();
            Toast.makeText(this, R.string.onboarding_bios_saved, Toast.LENGTH_SHORT).show();
        } catch (Exception e) {
            Toast.makeText(this, R.string.onboarding_bios_write_error, Toast.LENGTH_LONG).show();
        }
    }

    private void importBiosFromUri(Uri uri) {
        Context ctx = getApplicationContext();
        File base = DataDirectoryManager.getDataRoot(ctx);
        File biosDir = new File(base, "bios");
        if (!biosDir.exists()) biosDir.mkdirs();

        String name = "ps2_bios.bin";
        try {
            if ("content".equalsIgnoreCase(uri.getScheme())) {
                try (android.database.Cursor c = getContentResolver().query(uri, new String[]{android.provider.OpenableColumns.DISPLAY_NAME}, null, null, null)) {
                    if (c != null && c.moveToFirst()) {
                        String dn = c.getString(0);
                        if (dn != null && !dn.trim().isEmpty()) name = dn.trim();
                    }
                }
            } else {
                String p = uri.getPath();
                if (p != null) {
                    int idx = p.lastIndexOf('/');
                    if (idx >= 0 && idx + 1 < p.length()) name = p.substring(idx + 1);
                }
            }
        } catch (Throwable ignored) {}
        if (!name.toLowerCase().endsWith(".bin")) name = name + ".bin";

        // Avoid overwrite
        File outFile = new File(biosDir, name);
        int suffix = 1;
        while (outFile.exists()) {
            String baseName = name;
            String stem = baseName;
            String ext = "";
            int dot = baseName.lastIndexOf('.');
            if (dot > 0) { stem = baseName.substring(0, dot); ext = baseName.substring(dot); }
            outFile = new File(biosDir, stem + " (" + suffix + ")" + ext);
            suffix++;
        }

        try (InputStream in = getContentResolver().openInputStream(uri);
             OutputStream out = new FileOutputStream(outFile)) {
            if (in == null) throw new IOException("Unable to open BIOS Uri");
            byte[] buf = new byte[8192];
            int n;
            while ((n = in.read(buf)) > 0) out.write(buf, 0, n);
            out.flush();
            Toast.makeText(this, getString(R.string.home_bios_imported, outFile.getName()), Toast.LENGTH_SHORT).show();
        } catch (Exception e) {
            Toast.makeText(this, R.string.home_bios_import_failed, Toast.LENGTH_LONG).show();
        }
    }

    private void showBiosManagerDialog() {
        Context ctx = getApplicationContext();
        File base = DataDirectoryManager.getDataRoot(ctx);
        File biosDir = new File(base, "bios");
        if (!biosDir.exists()) biosDir.mkdirs();
        File[] files = biosDir.listFiles((dir, name) -> name != null && name.toLowerCase().endsWith(".bin"));
        java.util.List<File> biosList = new java.util.ArrayList<>();
        if (files != null) java.util.Collections.addAll(biosList, files);

        final String[] names = new String[biosList.size()];
        for (int i = 0; i < biosList.size(); i++) names[i] = biosList.get(i).getName();
        int checked = -1;
        try {
            String cur = NativeApp.getSetting("Filenames", "BIOS", "string");
            if (cur != null && !cur.isEmpty()) {
                for (int i = 0; i < biosList.size(); i++) {
                    if (new File(cur).getAbsolutePath().equals(biosList.get(i).getAbsolutePath())) {
                        checked = i;
                        break;
                    }
                }
            }
        } catch (Throwable ignored) {}

        MaterialAlertDialogBuilder b = new MaterialAlertDialogBuilder(this)
                .setTitle(R.string.home_bios_selection_title)
                .setSingleChoiceItems(names, checked, (d, which) -> {
                    try {
                        String path = biosList.get(which).getAbsolutePath();
                        NativeApp.setSetting("Filenames", "BIOS", "string", path);
                        Toast.makeText(this, getString(R.string.home_bios_current, biosList.get(which).getName()), Toast.LENGTH_SHORT).show();
                    } catch (Throwable ignored) {}
                })
                .setNegativeButton(R.string.home_close, (d, w) -> d.dismiss())
                .setPositiveButton(R.string.home_import, (d, w) -> openBiosImportForManager());
        b.show();
    }

    private void openBiosImportForManager() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
        intent.setType("application/octet-stream");
        String[] mimeTypes = new String[]{"application/octet-stream", "application/x-binary"};
        intent.putExtra(Intent.EXTRA_MIME_TYPES, mimeTypes);
        startActivityResultImportBios.launch(intent);
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

    private void setupInGameDrawer() {
        inGameDrawer = findViewById(R.id.drawer_in_game);
        drawerToggle = findViewById(R.id.btn_drawer_toggle);
        if (drawerToggle != null) {
            drawerToggle.setVisibility(View.GONE);
            drawerToggle.setOnClickListener(v -> {
                hideDrawerToggle();
                toggleInGameDrawer();
            });
        }
        if (inGameDrawer != null) {
            try {
                inGameDrawer.setDrawerElevation(0f);
            } catch (Throwable ignored) {}
            inGameDrawer.setDrawerLockMode(disableTouchControls ? DrawerLayout.LOCK_MODE_LOCKED_CLOSED : DrawerLayout.LOCK_MODE_UNLOCKED);
            inGameDrawer.addDrawerListener(new DrawerLayout.SimpleDrawerListener() {
                @Override
                public void onDrawerOpened(@NonNull View drawerView) {
                    lastInput = InputSource.TOUCH;
                    lastTouchTimeMs = System.currentTimeMillis();
                    setOnScreenControlsVisible(true);
                    hideDrawerToggle();
                    try {
                        getWindow().getDecorView().removeCallbacks(hideRunnable);
                    } catch (Throwable ignored) {}
                    updateWidescreenToggleVisibility();
                }

                @Override
                public void onDrawerClosed(@NonNull View drawerView) {
                    lastInput = InputSource.TOUCH;
                    lastTouchTimeMs = System.currentTimeMillis();
                    maybeAutoHideControls();
                }
            });
        }

        drawerPauseButton = findViewById(R.id.drawer_btn_pause);
        if (drawerPauseButton != null) {
            drawerPauseButton.setOnClickListener(v -> toggleVmPause());
        }

        drawerFastForwardButton = findViewById(R.id.drawer_btn_fast_forward);
        if (drawerFastForwardButton != null) {
            drawerFastForwardButton.setOnClickListener(v -> toggleFastForward());
            updateFastForwardButtonState();
        }

        FloatingActionButton btnReboot = findViewById(R.id.drawer_btn_reboot);
        if (btnReboot != null) {
            btnReboot.setOnClickListener(v -> {
                restartEmuThread();
                isVmPaused = false;
                updatePauseButtonIcon();
                closeInGameDrawer();
            });
        }

        FloatingActionButton btnPower = findViewById(R.id.drawer_btn_power);
        if (btnPower != null) {
            btnPower.setOnClickListener(v -> {
                shutdownVmToHome();
                closeInGameDrawer();
            });
        }

        MaterialButton btnGames = findViewById(R.id.drawer_btn_games);
        if (btnGames != null) {
            btnGames.setOnClickListener(v -> {
                NativeApp.pause();
                isVmPaused = true;
                updatePauseButtonIcon();
                showHome(true);
                closeInGameDrawer();
            });
        }

        MaterialButton btnGameState = findViewById(R.id.drawer_btn_game_state);
        if (btnGameState != null) {
            btnGameState.setOnClickListener(v -> {
                closeInGameDrawer();
                showGameStateDialog();
            });
        }

        MaterialButton btnChangeDisc = findViewById(R.id.drawer_btn_change_disc);
        if (btnChangeDisc != null) {
            btnChangeDisc.setOnClickListener(v -> {
                closeInGameDrawer();
                SwapDisc();
            });
        }

        MaterialButton btnTestController = findViewById(R.id.drawer_btn_test_controller);
        if (btnTestController != null) {
            btnTestController.setOnClickListener(v -> {
                closeInGameDrawer();
                new ControllerMappingDialog().show(getSupportFragmentManager(), "controller_mapping");
            });
        }

        MaterialButton btnSettingsDrawer = findViewById(R.id.drawer_btn_settings);
        if (btnSettingsDrawer != null) {
            btnSettingsDrawer.setOnClickListener(v -> {
                closeInGameDrawer();
                startActivityForResult(new Intent(this, SettingsActivity.class), 7722);
            });
        }

        MaterialButton btnAbout = findViewById(R.id.drawer_btn_about);
        if (btnAbout != null) {
            btnAbout.setOnClickListener(v -> {
                closeInGameDrawer();
                showAboutDialog();
            });
        }

        MaterialButton btnImportCheats = findViewById(R.id.drawer_btn_import_cheats);
        if (btnImportCheats != null) {
            btnImportCheats.setOnClickListener(v -> {
                closeInGameDrawer();
                openPnachPicker();
            });
        }

        MaterialButton btnImportTextures = findViewById(R.id.drawer_btn_import_textures);
        if (btnImportTextures != null) {
            btnImportTextures.setOnClickListener(v -> {
                closeInGameDrawer();
                launchTextureImportPicker();
            });
        }

        setupRetroAchievementsDrawerSection();
        setupRendererToggleGroup();
        setupDrawerSpinners();
        setupControllerModeSpinner();
        setupDrawerSwitches();
        Slider uiScaleSlider = findViewById(R.id.drawer_slider_ui_scale);
        TextView uiScaleValue = findViewById(R.id.drawer_ui_scale_value);
        if (uiScaleSlider != null) {
            uiScaleSlider.setValue(onScreenUiScaleMultiplier);
            updateOnScreenUiScaleLabel(uiScaleValue);
            uiScaleSlider.addOnChangeListener((slider, value, fromUser) -> {
                float clamped = Math.max(ONSCREEN_UI_SCALE_MIN, Math.min(ONSCREEN_UI_SCALE_MAX, value));
                if (Math.abs(clamped - value) > 0.001f) {
                    slider.setValue(clamped);
                }
                if (Math.abs(onScreenUiScaleMultiplier - clamped) > 0.001f) {
                    onScreenUiScaleMultiplier = clamped;
                    saveOnScreenUiScalePreference(clamped);
                    updateOnScreenUiScaleLabel(uiScaleValue);
                    applyUserUiScale();
                }
            });
        } else {
            updateOnScreenUiScaleLabel(uiScaleValue);
        }
        updatePauseButtonIcon();
    }

    private void setupTouchRevealOverlay() {
        View root = findViewById(R.id.in_game_root);
        if (root == null) {
            return;
        }
        root.setOnTouchListener((v, event) -> {
            if (disableTouchControls) {
                return false;
            }
            if (event.getAction() == MotionEvent.ACTION_DOWN) {
                lastInput = InputSource.TOUCH;
                lastTouchTimeMs = System.currentTimeMillis();
                setOnScreenControlsVisible(true);
                maybeAutoHideControls();
                showDrawerToggleTemporarily();
            }
            return false;
        });
    }

    private void toggleVmPause() {
        if (isVmPaused) {
            NativeApp.resume();
            isVmPaused = false;
        } else {
            NativeApp.pause();
            isVmPaused = true;
        }
        updatePauseButtonIcon();
    }

    private void toggleFastForward() {
        setFastForwardEnabled(!isFastForwardEnabled);
    }

    private void setFastForwardEnabled(boolean enabled) {
        if (isFastForwardEnabled == enabled) {
            updateFastForwardButtonState();
            return;
        }
        isFastForwardEnabled = enabled;
        try {
            NativeApp.speedhackLimitermode(enabled ? 3 : 0);
        } catch (Throwable ignored) {}
        updateFastForwardButtonState();
    }

    private void toggleInGameDrawer() {
        if (inGameDrawer == null) {
            return;
        }
        if (inGameDrawer.isDrawerOpen(GravityCompat.START)) {
            inGameDrawer.closeDrawer(GravityCompat.START);
        } else {
            lastInput = InputSource.TOUCH;
            lastTouchTimeMs = System.currentTimeMillis();
            setOnScreenControlsVisible(true);
            inGameDrawer.openDrawer(GravityCompat.START);
        }
    }

    private void closeInGameDrawer() {
        if (inGameDrawer != null && inGameDrawer.isDrawerOpen(GravityCompat.START)) {
            inGameDrawer.closeDrawer(GravityCompat.START);
        }
    }

    private void updatePauseButtonIcon() {
        if (drawerPauseButton == null) {
            return;
        }
        if (isVmPaused) {
            drawerPauseButton.setImageResource(R.drawable.ic_play_circle);
            drawerPauseButton.setContentDescription(getString(R.string.drawer_resume_content_description));
        } else {
            drawerPauseButton.setImageResource(R.drawable.ic_pause_circle);
            drawerPauseButton.setContentDescription(getString(R.string.drawer_pause_content_description));
        }
    }

    private void updateFastForwardButtonState() {
        if (drawerFastForwardButton == null) {
            return;
        }
        int surfaceVariant = resolveThemeColor(android.R.attr.colorBackground);
        int onSurface = resolveThemeColor(android.R.attr.textColorPrimary);
        int primary = resolveThemeColor(android.R.attr.colorPrimary);
        int onPrimary = resolveThemeColor(android.R.attr.textColorPrimary);

        if (isFastForwardEnabled) {
            drawerFastForwardButton.setBackgroundTintList(ColorStateList.valueOf(primary));
            drawerFastForwardButton.setImageTintList(ColorStateList.valueOf(onPrimary));
            drawerFastForwardButton.setContentDescription(getString(R.string.drawer_fast_forward_on_content_description));
        } else {
            drawerFastForwardButton.setBackgroundTintList(ColorStateList.valueOf(surfaceVariant));
            drawerFastForwardButton.setImageTintList(ColorStateList.valueOf(onSurface));
            drawerFastForwardButton.setContentDescription(getString(R.string.drawer_fast_forward_content_description));
        }
    }

    private int resolveThemeColor(int attrRes) {
        TypedValue value = new TypedValue();
        if (getTheme().resolveAttribute(attrRes, value, true)) {
            if (value.type >= TypedValue.TYPE_FIRST_COLOR_INT && value.type <= TypedValue.TYPE_LAST_COLOR_INT) {
                return value.data;
            }
            if (value.resourceId != 0) {
                return ContextCompat.getColor(this, value.resourceId);
            }
        }
        return Color.WHITE;
    }

    private void setupRetroAchievementsDrawerSection() {
        drawerRaSection = findViewById(R.id.drawer_ra_section);
        drawerRaLabel = findViewById(R.id.drawer_ra_label);
        if (drawerRaSection == null) {
            return;
        }

        drawerRaTitle = drawerRaSection.findViewById(R.id.drawer_ra_title);
        drawerRaSubtitle = drawerRaSection.findViewById(R.id.drawer_ra_subtitle);
        drawerRaIcon = drawerRaSection.findViewById(R.id.drawer_ra_icon);

        drawerRaSection.setOnClickListener(v -> showRetroAchievementsGameDialog());
        updateRetroAchievementsDrawer(currentRetroAchievementsState);
    }

    private void handleRetroAchievementsStateChanged(RetroAchievementsBridge.State state) {
        currentRetroAchievementsState = state;
        updateRetroAchievementsDrawer(state);

        if (state == null) {
            lastRetroAchievementsLoggedIn = false;
            lastRetroAchievementsGameId = -1;
            lastRetroAchievementsIconPath = "";
            return;
        }

        if (state.loggedIn && !lastRetroAchievementsLoggedIn) {
            String name = !TextUtils.isEmpty(state.displayName) ? state.displayName : state.username;
            if (!TextUtils.isEmpty(name)) {
                showRetroAchievementsToast(getString(R.string.drawer_ra_toast_connected, name));
            } else {
                showRetroAchievementsToast(getString(R.string.drawer_ra_toast_tracking_generic));
            }
        }

        if (state.hasActiveGame && state.gameId != 0 && state.gameId != lastRetroAchievementsGameId) {
            if (!TextUtils.isEmpty(state.gameTitle)) {
                showRetroAchievementsToast(getString(R.string.drawer_ra_toast_tracking, state.gameTitle));
            } else {
                showRetroAchievementsToast(getString(R.string.drawer_ra_toast_tracking_generic));
            }
        }

        lastRetroAchievementsLoggedIn = state.loggedIn;
        lastRetroAchievementsGameId = state.hasActiveGame ? state.gameId : -1;
    }

    private void updateRetroAchievementsDrawer(RetroAchievementsBridge.State state) {
        if (drawerRaSection == null) {
            return;
        }

        boolean shouldShow = state != null && state.achievementsEnabled && state.loggedIn && state.hasActiveGame
                && !TextUtils.isEmpty(state.gameTitle);
        int visibility = shouldShow ? View.VISIBLE : View.GONE;
        drawerRaSection.setVisibility(visibility);
        if (drawerRaLabel != null) {
            drawerRaLabel.setVisibility(visibility);
        }

        if (!shouldShow) {
            if (drawerRaIcon != null) {
                drawerRaIcon.setImageDrawable(null);
                drawerRaIcon.setVisibility(View.GONE);
            }
            lastRetroAchievementsIconPath = "";
            return;
        }

        if (drawerRaTitle != null) {
            drawerRaTitle.setText(state.gameTitle);
        }

        if (drawerRaSubtitle != null) {
            String subtitle = null;
            if (!TextUtils.isEmpty(state.richPresence)) {
                subtitle = state.richPresence;
            } else if (state.totalAchievements > 0) {
                subtitle = getString(R.string.drawer_ra_dialog_progress, state.unlockedAchievements, state.totalAchievements);
            }
            if (!TextUtils.isEmpty(subtitle)) {
                drawerRaSubtitle.setText(subtitle);
                drawerRaSubtitle.setVisibility(View.VISIBLE);
            } else {
                drawerRaSubtitle.setText("");
                drawerRaSubtitle.setVisibility(View.GONE);
            }
        }

        if (drawerRaIcon != null) {
            boolean iconVisible = false;
            if (!TextUtils.isEmpty(state.gameIconPath)) {
                File iconFile = new File(state.gameIconPath);
                if (iconFile.exists() && iconFile.isFile()) {
                    if (!state.gameIconPath.equals(lastRetroAchievementsIconPath)) {
                        Bitmap bitmap = BitmapFactory.decodeFile(iconFile.getAbsolutePath());
                        if (bitmap != null) {
                            drawerRaIcon.setImageBitmap(bitmap);
                            lastRetroAchievementsIconPath = state.gameIconPath;
                        } else {
                            drawerRaIcon.setImageDrawable(null);
                            lastRetroAchievementsIconPath = "";
                        }
                    }
                    iconVisible = drawerRaIcon.getDrawable() != null;
                } else {
                    drawerRaIcon.setImageDrawable(null);
                    lastRetroAchievementsIconPath = "";
                }
            } else {
                drawerRaIcon.setImageDrawable(null);
                lastRetroAchievementsIconPath = "";
            }
            drawerRaIcon.setVisibility(iconVisible ? View.VISIBLE : View.GONE);
        }
    }

    private void showRetroAchievementsGameDialog() {
        RetroAchievementsBridge.State state = currentRetroAchievementsState;
        if (state == null || !state.achievementsEnabled || !state.loggedIn || !state.hasActiveGame) {
            return;
        }

        String title = !TextUtils.isEmpty(state.gameTitle) ? state.gameTitle : getString(R.string.drawer_ra_dialog_title);
        MaterialAlertDialogBuilder builder = new MaterialAlertDialogBuilder(this)
                .setTitle(title)
                .setPositiveButton(android.R.string.ok, null);

        StringBuilder message = new StringBuilder();
        if (!TextUtils.isEmpty(state.richPresence)) {
            message.append(state.richPresence.trim());
        }
        if (state.totalAchievements > 0) {
            if (message.length() > 0) {
                message.append("\n\n");
            }
            message.append(getString(R.string.drawer_ra_dialog_progress, state.unlockedAchievements, state.totalAchievements));
        }
        if (state.totalPoints > 0) {
            if (message.length() > 0) {
                message.append("\n");
            }
            message.append(getString(R.string.drawer_ra_dialog_points, state.unlockedPoints, state.totalPoints));
        }
        if (state.hasLeaderboards) {
            if (message.length() > 0) {
                message.append("\n");
            }
            message.append(getString(R.string.drawer_ra_dialog_leaderboards));
        }

        if (message.length() > 0) {
            builder.setMessage(message.toString());
        }

        if (drawerRaIcon != null) {
            Drawable drawable = drawerRaIcon.getDrawable();
            if (drawable != null) {
                builder.setIcon(drawable);
            }
        }

        builder.show();
    }

    private void showRetroAchievementsToast(String message) {
        if (TextUtils.isEmpty(message)) {
            return;
        }
        try {
            Toast.makeText(this, message, Toast.LENGTH_SHORT).show();
        } catch (Throwable ignored) {}
    }

    private void setupRendererToggleGroup() {
        MaterialButtonToggleGroup rendererGroup = findViewById(R.id.drawer_tg_renderer);
        if (rendererGroup == null) {
            return;
        }

        int initialValue = -1;
        try {
            String renderer = NativeApp.getSetting("EmuCore/GS", "Renderer", "int");
            if (renderer != null && !renderer.isEmpty()) {
                initialValue = Integer.parseInt(renderer);
            }
        } catch (Exception ignored) {}

        int initialButton = rendererButtonForValue(initialValue);
        rendererGroup.check(initialButton);
        rendererGroup.addOnButtonCheckedListener((group, checkedId, isChecked) -> {
            if (!isChecked) {
                return;
            }
            int value = rendererValueForButton(checkedId);
            NativeApp.renderGpu(value);
        });
    }

    private void setupDrawerSpinners() {
		Spinner aspectSpinner = findViewById(R.id.drawer_sp_aspect_ratio);
		if (aspectSpinner != null) {
			ArrayAdapter<CharSequence> aspectAdapter = ArrayAdapter.createFromResource(this, R.array.aspect_ratios, android.R.layout.simple_spinner_item);
			aspectAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
			aspectSpinner.setAdapter(aspectAdapter);
			final String[] aspectChoices = getResources().getStringArray(R.array.aspect_ratios);
			int current = 0;
			try {
				String aspect = NativeApp.getSetting("EmuCore/GS", "AspectRatio", "string");
				if (aspect != null && !aspect.isEmpty()) {
					for (int i = 0; i < aspectChoices.length; i++) {
						if (aspect.equalsIgnoreCase(aspectChoices[i])) {
							current = i;
							break;
						}
					}
				}
			} catch (Exception ignored) {}
			if (current < 0 || current >= aspectAdapter.getCount()) current = 0;
			aspectSpinner.setSelection(current, false);
			aspectSpinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
				@Override
				public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
					if (position < 0 || position >= aspectChoices.length)
						return;
					String value = aspectChoices[position];
					NativeApp.setSetting("EmuCore/GS", "AspectRatio", "string", value);
					NativeApp.setAspectRatio(position);
				}

				@Override
				public void onNothingSelected(AdapterView<?> parent) {
				}
            });
        }

        Spinner scaleSpinner = findViewById(R.id.drawer_sp_scale);
        if (scaleSpinner != null) {
            ArrayAdapter<CharSequence> scaleAdapter = ArrayAdapter.createFromResource(this, R.array.resolution_scales, android.R.layout.simple_spinner_item);
            scaleAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
            scaleSpinner.setAdapter(scaleAdapter);
            final String[] scales = getResources().getStringArray(R.array.resolution_scales);
            int current = 2;
            try {
                String value = NativeApp.getSetting("EmuCore/GS", "upscale_multiplier", "float");
                if (value != null && !value.isEmpty()) {
                    float f = Float.parseFloat(value);
                    String search = (f == (int)f) ? (int)f + "x" : f + "x";
                    for (int i = 0; i < scales.length; i++) {
                        if (scales[i].equalsIgnoreCase(search)) {
                            current = i;
                            break;
                        }
                    }
                }
            } catch (Exception ignored) {}
            scaleSpinner.setSelection(current, false);
            scaleSpinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
                @Override
                public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                    String val = scales[position].replace("x", "");
                    NativeApp.setSetting("EmuCore/GS", "upscale_multiplier", "float", val);
                }
                @Override public void onNothingSelected(AdapterView<?> parent) {}
            });
        }

        Spinner blendSpinner = findViewById(R.id.drawer_sp_blending_accuracy);
        if (blendSpinner != null) {
            ArrayAdapter<CharSequence> blendAdapter = ArrayAdapter.createFromResource(this, R.array.acc_blending, android.R.layout.simple_spinner_item);
            blendAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
            blendSpinner.setAdapter(blendAdapter);
            int current = 0;
            try {
                String value = NativeApp.getSetting("EmuCore/GS", "accurate_blending_unit", "int");
                if (value != null && !value.isEmpty()) {
                    current = Integer.parseInt(value);
                }
            } catch (Exception ignored) {}
            if (current < 0 || current >= blendAdapter.getCount()) current = 0;
            blendSpinner.setSelection(current, false);
            blendSpinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
                @Override
                public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                    NativeApp.setSetting("EmuCore/GS", "accurate_blending_unit", "int", Integer.toString(position));
                }

                @Override
                public void onNothingSelected(AdapterView<?> parent) {
                }
            });
        }
    }

    private void setupControllerModeSpinner() {
        Spinner modeSpinner = findViewById(R.id.drawer_sp_controller_mode);
        if (modeSpinner != null) {
            String[] modes = new String[]{"2 Sticks", "1 Stick + Face Buttons", "D-Pad Only"};
            ArrayAdapter<String> adapter = new ArrayAdapter<>(this, android.R.layout.simple_spinner_item, modes);
            adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
            modeSpinner.setAdapter(adapter);
            
            // Load saved mode (default is 0 = 2 Sticks)
            int savedMode = getSharedPreferences(PREFS, MODE_PRIVATE).getInt("controller_mode", 0);
            modeSpinner.setSelection(savedMode, false);
            
            modeSpinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
                @Override
                public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                    getSharedPreferences(PREFS, MODE_PRIVATE).edit().putInt("controller_mode", position).apply();
                    applyControllerMode(position);
                }

                @Override
                public void onNothingSelected(AdapterView<?> parent) {
                }
            });
            
            applyControllerMode(savedMode);
        }
    }

    private void applyControllerMode(int mode) {
        currentControllerMode = mode;
        
        JoystickView joystickLeft = findViewById(R.id.joystick_left);
        JoystickView joystickRight = findViewById(R.id.joystick_right);
        DPadView dpadView = findViewById(R.id.dpad_view);
        View llPadRight = findViewById(R.id.ll_pad_right);
        
        if (joystickLeft == null || joystickRight == null || dpadView == null || llPadRight == null) {
            return;
        }
        
        switch (mode) {
            case 0: // 2 Sticks 
                joystickLeft.setVisibility(View.VISIBLE);
                joystickRight.setVisibility(View.VISIBLE);
                dpadView.setVisibility(View.VISIBLE);
                llPadRight.setVisibility(View.VISIBLE);
                
                ViewGroup.LayoutParams leftParams = joystickLeft.getLayoutParams();
                leftParams.width = dpToPx(140);
                leftParams.height = dpToPx(140);
                joystickLeft.setLayoutParams(leftParams);
                
                ViewGroup.LayoutParams rightParams = joystickRight.getLayoutParams();
                rightParams.width = dpToPx(140);
                rightParams.height = dpToPx(140);
                joystickRight.setLayoutParams(rightParams);
                
                ViewGroup.LayoutParams dpadParams = dpadView.getLayoutParams();
                dpadParams.width = dpToPx(105);
                dpadParams.height = dpToPx(105);
                dpadView.setLayoutParams(dpadParams);
                
                llPadRight.setScaleX(1.0f);
                llPadRight.setScaleY(1.0f);
                if (llPadRight.getLayoutParams() instanceof ViewGroup.MarginLayoutParams) {
                    ViewGroup.MarginLayoutParams faceParams = (ViewGroup.MarginLayoutParams) llPadRight.getLayoutParams();
                    faceParams.bottomMargin = dpToPx(1); 
                    llPadRight.setLayoutParams(faceParams);
                }
                faceButtonsBaseScale = 1.0f;
                break;
                
            case 1: // 1 Stick + Face Buttons 
                joystickLeft.setVisibility(View.VISIBLE);
                joystickRight.setVisibility(View.GONE); 
                dpadView.setVisibility(View.GONE); 
                llPadRight.setVisibility(View.VISIBLE);
                
                ViewGroup.LayoutParams leftParams1 = joystickLeft.getLayoutParams();
                leftParams1.width = dpToPx(140);
                leftParams1.height = dpToPx(140);
                joystickLeft.setLayoutParams(leftParams1);
                
                ViewGroup.LayoutParams dpadParams1 = dpadView.getLayoutParams();
                dpadParams1.width = dpToPx(105);
                dpadParams1.height = dpToPx(105);
                dpadView.setLayoutParams(dpadParams1);
                
                llPadRight.setScaleX(1.4f);
                llPadRight.setScaleY(1.4f);
                
                if (llPadRight.getLayoutParams() instanceof ViewGroup.MarginLayoutParams) {
                    ViewGroup.MarginLayoutParams faceParams1 = (ViewGroup.MarginLayoutParams) llPadRight.getLayoutParams();
                    faceParams1.bottomMargin = dpToPx(6) + dpToPx(11); 
                    llPadRight.setLayoutParams(faceParams1);
                }
                faceButtonsBaseScale = 1.4f;
                break;
                
            case 2: // D-Pad Only 
                joystickLeft.setVisibility(View.GONE);
                joystickRight.setVisibility(View.GONE);
                dpadView.setVisibility(View.VISIBLE);
                llPadRight.setVisibility(View.VISIBLE);
                
                ViewGroup.LayoutParams dpadParams2 = dpadView.getLayoutParams();
                dpadParams2.width = dpToPx(140);
                dpadParams2.height = dpToPx(140);
                dpadView.setLayoutParams(dpadParams2);
                
                ViewGroup.LayoutParams rightParams2 = joystickRight.getLayoutParams();
                rightParams2.width = dpToPx(140);
                rightParams2.height = dpToPx(140);
                joystickRight.setLayoutParams(rightParams2);
                
                llPadRight.setScaleX(1.4f);
                llPadRight.setScaleY(1.4f);
                
                if (llPadRight.getLayoutParams() instanceof ViewGroup.MarginLayoutParams) {
                    ViewGroup.MarginLayoutParams faceParams2 = (ViewGroup.MarginLayoutParams) llPadRight.getLayoutParams();
                    faceParams2.bottomMargin = dpToPx(6) + dpToPx(11); 
                    llPadRight.setLayoutParams(faceParams2);
                }
                faceButtonsBaseScale = 1.4f;
                break;
        }
        applyJoystickStyle(joystickLeft);
        applyJoystickStyle(joystickRight);
        applyDpadStyle(dpadView);
        applyUserUiScale();
    }

    private void setupDrawerSwitches() {
        MaterialSwitch swEnableCheats = findViewById(R.id.drawer_sw_enable_cheats);
        if (swEnableCheats != null) {
            swEnableCheats.setChecked(readBoolSetting("EmuCore", "EnableCheats", false));
            swEnableCheats.setOnCheckedChangeListener((buttonView, isChecked) ->
            {
                    NativeApp.setEnableCheats(isChecked);
                    try {
                        DebugLog.d("Cheats", "EnableCheats=" + isChecked);
                    } catch (Throwable ignored) {}
            });
        }

        drawerWidescreenSwitch = findViewById(R.id.drawer_sw_widescreen);
        updateWidescreenToggleVisibility();

        MaterialSwitch swNoInterlacing = findViewById(R.id.drawer_sw_no_interlacing);
        if (swNoInterlacing != null) {
            swNoInterlacing.setChecked(readBoolSetting("EmuCore", "EnableNoInterlacingPatches", false));
            swNoInterlacing.setOnCheckedChangeListener((buttonView, isChecked) ->
                    NativeApp.setSetting("EmuCore", "EnableNoInterlacingPatches", "bool", isChecked ? "true" : "false"));
        }

        MaterialSwitch swLoadTextures = findViewById(R.id.drawer_sw_load_textures);
        if (swLoadTextures != null) {
            swLoadTextures.setChecked(readBoolSetting("EmuCore/GS", "LoadTextureReplacements", false));
            swLoadTextures.setOnCheckedChangeListener((buttonView, isChecked) -> {
                NativeApp.setSetting("EmuCore/GS", "LoadTextureReplacements", "bool", isChecked ? "true" : "false");
                try {
                    DebugLog.d("Textures", "LoadTextureReplacements=" + isChecked);
                } catch (Throwable ignored) {}
            });
        }

        MaterialSwitch swAsyncTextures = findViewById(R.id.drawer_sw_async_textures);
        if (swAsyncTextures != null) {
            swAsyncTextures.setChecked(readBoolSetting("EmuCore/GS", "LoadTextureReplacementsAsync", false));
            swAsyncTextures.setOnCheckedChangeListener((buttonView, isChecked) -> {
                NativeApp.setSetting("EmuCore/GS", "LoadTextureReplacementsAsync", "bool", isChecked ? "true" : "false");
                try {
                    DebugLog.d("Textures", "LoadTextureReplacementsAsync=" + isChecked);
                } catch (Throwable ignored) {}
            });
        }

        MaterialSwitch swPrecacheTextures = findViewById(R.id.drawer_sw_precache_textures);
        if (swPrecacheTextures != null) {
            swPrecacheTextures.setChecked(readBoolSetting("EmuCore/GS", "PrecacheTextureReplacements", false));
            swPrecacheTextures.setOnCheckedChangeListener((buttonView, isChecked) -> {
                NativeApp.setSetting("EmuCore/GS", "PrecacheTextureReplacements", "bool", isChecked ? "true" : "false");
                try {
                    DebugLog.d("Textures", "PrecacheTextureReplacements=" + isChecked);
                } catch (Throwable ignored) {}
            });
        }

        MaterialSwitch swDevHud = findViewById(R.id.drawer_sw_dev_hud);
        if (swDevHud != null) {
            swDevHud.setChecked(readBoolSetting("EmuCore/GS", "OsdShowFPS", false));
            swDevHud.setOnCheckedChangeListener((buttonView, isChecked) ->
                    NativeApp.setSetting("EmuCore/GS", "OsdShowFPS", "bool", isChecked ? "true" : "false"));
        }
    }

    private void updateWidescreenToggleVisibility() {
        if (drawerWidescreenSwitch == null) {
            return;
        }
        boolean hasPatch = false;
        try {
            hasPatch = NativeApp.hasWidescreenPatch();
        } catch (Throwable ignored) {}
        if (!hasPatch) {
            drawerWidescreenSwitch.setVisibility(View.GONE);
            drawerWidescreenSwitch.setOnCheckedChangeListener(null);
            return;
        }
        drawerWidescreenSwitch.setVisibility(View.VISIBLE);
        drawerWidescreenSwitch.setText(R.string.drawer_apply_widescreen_patch);
        drawerWidescreenSwitch.setOnCheckedChangeListener(null);
        drawerWidescreenSwitch.setChecked(readBoolSetting("EmuCore", "EnableWideScreenPatches", false));
        drawerWidescreenSwitch.setOnCheckedChangeListener(drawerWidescreenListener);
    }

    private boolean readBoolSetting(String section, String key, boolean defaultValue) {
        try {
            String value = NativeApp.getSetting(section, key, "bool");
            if (value == null || value.isEmpty()) {
                return defaultValue;
            }
            return "1".equals(value) || "true".equalsIgnoreCase(value);
        } catch (Exception ignored) {
            return defaultValue;
        }
    }

    private @IdRes int rendererButtonForValue(int value) {
        switch (value) {
            case 12:
                return R.id.drawer_tb_gl;
            case 13:
                return R.id.drawer_tb_sw;
            case 14:
                return R.id.drawer_tb_vk;
            default:
                return R.id.drawer_tb_at;
        }
    }

    private int rendererValueForButton(@IdRes int buttonId) {
        if (buttonId == R.id.drawer_tb_gl) return 12;
        if (buttonId == R.id.drawer_tb_sw) return 13;
        if (buttonId == R.id.drawer_tb_vk) return 14;
        return -1;
    }

    private void applyRendererSelection(int rendererValue) {
        MaterialButtonToggleGroup rendererGroup = findViewById(R.id.drawer_tg_renderer);
        if (rendererGroup != null) {
            rendererGroup.check(rendererButtonForValue(rendererValue));
        } else {
            NativeApp.renderGpu(rendererValue);
        }
    }

    private void showGameStateDialog() {
        CharSequence[] items = new CharSequence[]{
                getString(R.string.home_game_state_save_slot_1),
                getString(R.string.home_game_state_load_slot_1)
        };
    new MaterialAlertDialogBuilder(this)
        .setTitle(R.string.home_game_state_title)
                .setItems(items, (dialog, which) -> {
                    if (which == 0) {
                        pauseVmForStateOperation();
                        boolean ok = NativeApp.saveStateToSlot(1);
                        try {
                            Toast.makeText(this, ok ? R.string.home_game_state_saved : R.string.home_game_state_save_failed, Toast.LENGTH_SHORT).show();
                        } catch (Throwable ignored) {}
                        resumeVmAfterStateOperation();
                    } else if (which == 1) {
                        pauseVmForStateOperation();
                        boolean ok = NativeApp.loadStateFromSlot(1);
                        try {
                            Toast.makeText(this, ok ? R.string.home_game_state_loaded : R.string.home_game_state_load_failed, Toast.LENGTH_SHORT).show();
                        } catch (Throwable ignored) {}
                        if (!ok) {
                            resumeVmAfterStateOperation();
                            return;
                        }
                        resumeVmAfterStateOperation();
                    }
                })
                .setNegativeButton(android.R.string.cancel, null)
                .show();
    }

    private void showAboutDialog() {
        String versionName = "";
        try {
            versionName = getPackageManager().getPackageInfo(getPackageName(), 0).versionName;
        } catch (Exception ignored) {}
    String message = "ARMSX2 (" + versionName + ")\n" +
        "by ARMSX2 team\n\n" +
        "Core contributors:\n" +
        "- MoonPower — App developer\n" +
        "- jpolo — Management\n" +
        "- Medievalshell — Web developer\n" +
        "- set l — Web developer\n" +
        "- Alex — QA tester\n" +
        "- Yua — QA tester\n\n" +
        "Thanks to:\n" +
        "- pontos2024 (emulator base)\n" +
        "- PCSX2 v2.3.430 (core emulator)\n" +
        "- SDL (SDL3)\n" +
        "- Fffathur (icon design)\n" +
        "- vivimagic0 (icon design)";
    new MaterialAlertDialogBuilder(this)
        .setTitle("About")
        .setMessage(message)
        .setPositiveButton(android.R.string.ok, (d, w) -> d.dismiss())
        .show();
    }

    private String resolveOnScreenUiStylePreference() {
        String value = getSharedPreferences(PREFS, MODE_PRIVATE).getString(PREF_ONSCREEN_UI_STYLE, STYLE_DEFAULT);
        if (STYLE_NETHER.equalsIgnoreCase(value)) {
            return STYLE_NETHER;
        }
        return STYLE_DEFAULT;
    }

    private void refreshOnScreenUiStyleIfNeeded() {
        String pref = resolveOnScreenUiStylePreference();
        if (!pref.equals(currentOnScreenUiStyle)) {
            currentOnScreenUiStyle = pref;
            makeButtonTouch();
        }
    }

    private Drawable loadNetherDrawable(String assetName) {
        try (InputStream is = getAssets().open("app_icons/controller_icons_nether/" + assetName)) {
            Drawable drawable = Drawable.createFromStream(is, assetName);
            if (drawable != null) {
                drawable = drawable.mutate();
            }
            return drawable;
        } catch (IOException e) {
            try { DebugLog.e("OnScreenUI", "Failed to load Nether icon " + assetName + ": " + e.getMessage()); } catch (Throwable ignored) {}
            return null;
        }
    }

    private void applyButtonIcon(PSButtonView view, @DrawableRes int defaultResId, String netherAssetName) {
        if (view == null) {
            return;
        }
        if (STYLE_NETHER.equals(currentOnScreenUiStyle)) {
            Drawable drawable = loadNetherDrawable(netherAssetName);
            if (drawable != null) {
                view.setIconDrawable(drawable);
                return;
            }
        }
        view.setIconResource(defaultResId);
    }

    private void applyShoulderIcon(PSShoulderButtonView view, @DrawableRes int defaultResId, String netherAssetName) {
        if (view == null) {
            return;
        }
        if (STYLE_NETHER.equals(currentOnScreenUiStyle)) {
            Drawable drawable = loadNetherDrawable(netherAssetName);
            if (drawable != null) {
                view.setIconDrawable(drawable);
                return;
            }
        }
        view.setIconResource(defaultResId);
    }

    private void applyJoystickStyle(JoystickView joystick) {
        if (joystick == null) {
            return;
        }
        if (STYLE_NETHER.equals(currentOnScreenUiStyle)) {
            Drawable base = loadNetherDrawable("ic_controller_analog_base.png");
            Drawable knob = loadNetherDrawable("ic_controller_analog_stick.png");
            if (base != null && knob != null) {
                joystick.setDrawables(base, knob);
                joystick.setKnobScaleFactor(1.2f);
                return;
            }
        }
        joystick.setDrawables(null, null);
        joystick.setKnobScaleFactor(1.0f);
    }

    private void applyDpadStyle(DPadView dpadView) {
        if (dpadView == null) {
            return;
        }
        if (STYLE_NETHER.equals(currentOnScreenUiStyle)) {
            Drawable up = loadNetherDrawable("ic_controller_up_button.png");
            Drawable down = loadNetherDrawable("ic_controller_down_button.png");
            Drawable left = loadNetherDrawable("ic_controller_left_button.png");
            Drawable right = loadNetherDrawable("ic_controller_right_button.png");
            dpadView.setDrawables(null, null);
            dpadView.setDirectionalDrawables(up, down, left, right);
        } else {
            dpadView.setDrawables(null, null);
            dpadView.setDirectionalDrawables(null, null, null, null);
        }
    }

    private void loadOnScreenUiScalePreference() {
        float value = 1.0f;
        try {
            value = getSharedPreferences(PREFS, MODE_PRIVATE).getFloat(PREF_UI_SCALE_MULTIPLIER, 1.0f);
        } catch (Exception ignored) {}
        if (value < ONSCREEN_UI_SCALE_MIN) value = ONSCREEN_UI_SCALE_MIN;
        if (value > ONSCREEN_UI_SCALE_MAX) value = ONSCREEN_UI_SCALE_MAX;
        onScreenUiScaleMultiplier = value;
    }

    private void saveOnScreenUiScalePreference(float value) {
        getSharedPreferences(PREFS, MODE_PRIVATE).edit().putFloat(PREF_UI_SCALE_MULTIPLIER, value).apply();
    }

    private void applyUserUiScale() {
        float multiplier = Math.max(ONSCREEN_UI_SCALE_MIN, Math.min(ONSCREEN_UI_SCALE_MAX, onScreenUiScaleMultiplier));
        onScreenUiScaleMultiplier = multiplier;
        applyScaleWithPivot(llPadSelectStart, multiplier, multiplier, 0.5f, 1f);
        View padRight = llPadRight != null ? llPadRight : findViewById(R.id.ll_pad_right);
        float faceScale = faceButtonsBaseScale * multiplier;
        applyScaleWithPivot(padRight, faceScale, faceScale, 1f, 1f);
        View leftShoulders = findViewById(R.id.ll_pad_shoulders_left);
        applyScaleWithPivot(leftShoulders, multiplier, multiplier, 0f, 0f);
        View rightShoulders = findViewById(R.id.ll_pad_shoulders_right);
        applyScaleWithPivot(rightShoulders, multiplier, multiplier, 1f, 0f);
        JoystickView joystickLeft = findViewById(R.id.joystick_left);
        applyScaleWithPivot(joystickLeft, multiplier, multiplier, 0f, 1f);
        JoystickView joystickRight = findViewById(R.id.joystick_right);
        applyScaleWithPivot(joystickRight, multiplier, multiplier, 1f, 1f);
        DPadView dpadView = findViewById(R.id.dpad_view);
        applyScaleWithPivot(dpadView, multiplier, multiplier, 0f, 1f);
    }

    private void applyScaleWithPivot(View view, float scaleX, float scaleY, float pivotXF, float pivotYF) {
        if (view == null) {
            return;
        }
        Runnable apply = () -> {
            float pivotX = view.getWidth() * pivotXF;
            float pivotY = view.getHeight() * pivotYF;
            view.setPivotX(pivotX);
            view.setPivotY(pivotY);
            view.setScaleX(scaleX);
            view.setScaleY(scaleY);
        };
        if (view.getWidth() == 0 || view.getHeight() == 0) {
            view.post(apply);
        } else {
            apply.run();
        }
    }

    private void pauseVmForStateOperation() {
        try {
            NativeApp.pause();
            SystemClock.sleep(50);
            NativeApp.resetKeyStatus();
        } catch (Throwable ignored) {}
    }

    private void resumeVmAfterStateOperation() {
        try {
            SystemClock.sleep(30);
            NativeApp.resume();
            isVmPaused = false;
            updatePauseButtonIcon();
        } catch (Throwable ignored) {}
    }

    private void updateOnScreenUiScaleLabel(TextView label) {
        if (label != null) {
            label.setText(getString(R.string.drawer_ui_scale_value, onScreenUiScaleMultiplier));
        }
    }

    private void refreshOnScreenUiScaleIfNeeded() {
        float stored = 1.0f;
        try {
            stored = getSharedPreferences(PREFS, MODE_PRIVATE).getFloat(PREF_UI_SCALE_MULTIPLIER, 1.0f);
        } catch (Exception ignored) {}
        if (stored < ONSCREEN_UI_SCALE_MIN) stored = ONSCREEN_UI_SCALE_MIN;
        if (stored > ONSCREEN_UI_SCALE_MAX) stored = ONSCREEN_UI_SCALE_MAX;
        if (Math.abs(stored - onScreenUiScaleMultiplier) > 0.001f) {
            onScreenUiScaleMultiplier = stored;
            applyUserUiScale();
            Slider slider = findViewById(R.id.drawer_slider_ui_scale);
            if (slider != null && Math.abs(slider.getValue() - stored) > 0.001f) {
                slider.setValue(stored);
            }
            TextView label = findViewById(R.id.drawer_ui_scale_value);
            updateOnScreenUiScaleLabel(label);
        }
    }

    private void makeButtonTouch() {
        boolean isNether = STYLE_NETHER.equals(currentOnScreenUiStyle);
        PSButtonView btn_pad_select = findViewById(R.id.btn_pad_select);
        if (btn_pad_select != null) {
            applyButtonIcon(btn_pad_select, R.drawable.ic_ps_select, "ic_controller_select_button.png");
            btn_pad_select.setRectangular(true);
            float selectScale = isNether ? 0.75f : 1.0f;
            btn_pad_select.setScaleX(selectScale);
            btn_pad_select.setScaleY(selectScale);
            btn_pad_select.setOnPSButtonListener(pressed -> NativeApp.setPadButton(KeyEvent.KEYCODE_BUTTON_SELECT, 0, pressed));
        }

        PSButtonView btn_pad_start = findViewById(R.id.btn_pad_start);
        if (btn_pad_start != null) {
            applyButtonIcon(btn_pad_start, R.drawable.ic_ps_start, "ic_controller_start_button.png");
            float selectScale = isNether ? 0.75f : 1.0f;
            btn_pad_start.setScaleX(selectScale);
            btn_pad_start.setScaleY(selectScale);
            btn_pad_start.setOnPSButtonListener(pressed -> NativeApp.setPadButton(KeyEvent.KEYCODE_BUTTON_START, 0, pressed));
        }

        float faceScale = isNether ? 0.9f : 1.0f;

        PSButtonView btn_pad_a = findViewById(R.id.btn_pad_a);
        if (btn_pad_a != null) {
            applyButtonIcon(btn_pad_a, R.drawable.ic_ps_cross, "ic_controller_cross_button.png");
            btn_pad_a.setScaleX(faceScale);
            btn_pad_a.setScaleY(faceScale);
            btn_pad_a.setOnPSButtonListener(pressed -> {
                int action = pressed ? MotionEvent.ACTION_DOWN : MotionEvent.ACTION_UP;
                sendKeyAction(btn_pad_a, action, KeyEvent.KEYCODE_BUTTON_A);
            });
        }

        PSButtonView btn_pad_b = findViewById(R.id.btn_pad_b);
        if (btn_pad_b != null) {
            applyButtonIcon(btn_pad_b, R.drawable.ic_ps_circle, "ic_controller_circle_button.png");
            btn_pad_b.setScaleX(faceScale);
            btn_pad_b.setScaleY(faceScale);
            btn_pad_b.setOnPSButtonListener(pressed -> {
                int action = pressed ? MotionEvent.ACTION_DOWN : MotionEvent.ACTION_UP;
                sendKeyAction(btn_pad_b, action, KeyEvent.KEYCODE_BUTTON_B);
            });
        }

        PSButtonView btn_pad_x = findViewById(R.id.btn_pad_x);
        if (btn_pad_x != null) {
            applyButtonIcon(btn_pad_x, R.drawable.ic_ps_square, "ic_controller_square_button.png");
            btn_pad_x.setScaleX(faceScale);
            btn_pad_x.setScaleY(faceScale);
            btn_pad_x.setOnPSButtonListener(pressed -> {
                int action = pressed ? MotionEvent.ACTION_DOWN : MotionEvent.ACTION_UP;
                sendKeyAction(btn_pad_x, action, KeyEvent.KEYCODE_BUTTON_X);
            });
        }

        PSButtonView btn_pad_y = findViewById(R.id.btn_pad_y);
        if (btn_pad_y != null) {
            applyButtonIcon(btn_pad_y, R.drawable.ic_ps_triangle, "ic_controller_triangle_button.png");
            btn_pad_y.setScaleX(faceScale);
            btn_pad_y.setScaleY(faceScale);
            btn_pad_y.setOnPSButtonListener(pressed -> {
                int action = pressed ? MotionEvent.ACTION_DOWN : MotionEvent.ACTION_UP;
                sendKeyAction(btn_pad_y, action, KeyEvent.KEYCODE_BUTTON_Y);
            });
        }

        PSShoulderButtonView btn_pad_l1 = findViewById(R.id.btn_pad_l1);
        if (btn_pad_l1 != null) {
            applyShoulderIcon(btn_pad_l1, R.drawable.ic_ps_l1, "ic_controller_l1_button.png");
            btn_pad_l1.setScaleX(1.0f);
            btn_pad_l1.setScaleY(isNether ? 0.6f : 1.0f);
            btn_pad_l1.setOnPSShoulderButtonListener(pressed -> NativeApp.setPadButton(KeyEvent.KEYCODE_BUTTON_L1, 0, pressed));
        }

        PSShoulderButtonView btn_pad_r1 = findViewById(R.id.btn_pad_r1);
        if (btn_pad_r1 != null) {
            applyShoulderIcon(btn_pad_r1, R.drawable.ic_ps_r1, "ic_controller_r1_button.png");
            btn_pad_r1.setScaleX(1.0f);
            btn_pad_r1.setScaleY(isNether ? 0.6f : 1.0f);
            btn_pad_r1.setOnPSShoulderButtonListener(pressed -> NativeApp.setPadButton(KeyEvent.KEYCODE_BUTTON_R1, 0, pressed));
        }

        PSShoulderButtonView btn_pad_l2 = findViewById(R.id.btn_pad_l2);
        if (btn_pad_l2 != null) {
            applyShoulderIcon(btn_pad_l2, R.drawable.ic_ps_l2, "ic_controller_l2_button.png");
            btn_pad_l2.setScaleX(1.0f);
            btn_pad_l2.setScaleY(isNether ? 0.6f : 1.0f);
            btn_pad_l2.setOnPSShoulderButtonListener(pressed -> NativeApp.setPadButton(KeyEvent.KEYCODE_BUTTON_L2, 0, pressed));
        }

        PSShoulderButtonView btn_pad_r2 = findViewById(R.id.btn_pad_r2);
        if (btn_pad_r2 != null) {
            applyShoulderIcon(btn_pad_r2, R.drawable.ic_ps_r2, "ic_controller_r2_button.png");
            btn_pad_r2.setScaleX(1.0f);
            btn_pad_r2.setScaleY(isNether ? 0.6f : 1.0f);
            btn_pad_r2.setOnPSShoulderButtonListener(pressed -> NativeApp.setPadButton(KeyEvent.KEYCODE_BUTTON_R2, 0, pressed));
        }

        PSButtonView btn_pad_l3 = findViewById(R.id.btn_pad_l3);
        if (btn_pad_l3 != null) {
            applyButtonIcon(btn_pad_l3, R.drawable.ic_ps_l3, "ic_controller_l3_button.png");
            btn_pad_l3.setOnPSButtonListener(pressed -> NativeApp.setPadButton(KeyEvent.KEYCODE_BUTTON_THUMBL, 0, pressed));
        }

        PSButtonView btn_pad_r3 = findViewById(R.id.btn_pad_r3);
        if (btn_pad_r3 != null) {
            applyButtonIcon(btn_pad_r3, R.drawable.ic_ps_r3, "ic_controller_r3_button.png");
            btn_pad_r3.setOnPSButtonListener(pressed -> NativeApp.setPadButton(KeyEvent.KEYCODE_BUTTON_THUMBR, 0, pressed));
        }

        applyUserUiScale();

        JoystickView joystickLeft = findViewById(R.id.joystick_left);
        if (joystickLeft != null) {
            applyJoystickStyle(joystickLeft);
            joystickLeft.setOnJoystickMoveListener((x, y) -> {
                float clampedX = Math.max(-1f, Math.min(1f, x));
                float clampedY = Math.max(-1f, Math.min(1f, y));
                sendAnalog(111, Math.max(0f, clampedX));
                sendAnalog(113, Math.max(0f, -clampedX));
                sendAnalog(112, Math.max(0f, clampedY));
                sendAnalog(110, Math.max(0f, -clampedY));
                lastInput = InputSource.TOUCH;
                lastTouchTimeMs = System.currentTimeMillis();
                maybeAutoHideControls();
            });
        }

        JoystickView joystickRight = findViewById(R.id.joystick_right);
        if (joystickRight != null) {
            applyJoystickStyle(joystickRight);
            joystickRight.setOnJoystickMoveListener((x, y) -> {
                float clampedX = Math.max(-1f, Math.min(1f, x));
                float clampedY = Math.max(-1f, Math.min(1f, y));
                sendAnalog(121, Math.max(0f, clampedX));
                sendAnalog(123, Math.max(0f, -clampedX));
                sendAnalog(122, Math.max(0f, clampedY));
                sendAnalog(120, Math.max(0f, -clampedY));
                lastInput = InputSource.TOUCH;
                lastTouchTimeMs = System.currentTimeMillis();
                maybeAutoHideControls();
            });
        }

        DPadView dpadView = findViewById(R.id.dpad_view);
        if (dpadView != null) {
            applyDpadStyle(dpadView);
            dpadView.setOnDPadListener((direction, pressed) -> {
                int keycode = -1;
                switch (direction) {
                    case UP:
                        keycode = KeyEvent.KEYCODE_DPAD_UP;
                        break;
                    case DOWN:
                        keycode = KeyEvent.KEYCODE_DPAD_DOWN;
                        break;
                    case LEFT:
                        keycode = KeyEvent.KEYCODE_DPAD_LEFT;
                        break;
                    case RIGHT:
                        keycode = KeyEvent.KEYCODE_DPAD_RIGHT;
                        break;
                }

                if (keycode != -1) {
                    int action = pressed ? MotionEvent.ACTION_DOWN : MotionEvent.ACTION_UP;
                    sendKeyAction(dpadView, action, keycode);
                }
            });
        }
        applyUserUiScale();
    }

    private boolean importMemcardToSlot1(Uri uri) {
        try {
            File base = DataDirectoryManager.getDataRoot(getApplicationContext());
            File memDir = new File(base, "memcards");
            if (!memDir.exists() && !memDir.mkdirs()) return false;
            File out = new File(memDir, "Mcd001.ps2");
            try (InputStream in = getContentResolver().openInputStream(uri);
                 OutputStream os = new FileOutputStream(out)) {
                if (in == null) return false;
                byte[] buf = new byte[8192];
                int n;
                while ((n = in.read(buf)) > 0) os.write(buf, 0, n);
                os.flush();
            }
            return true;
        } catch (Exception e) {
            return false;
        }
    }

    private void importCheatFile(Uri uri) {
        if (uri == null) {
            return;
        }
        new Thread(() -> {
            boolean success = false;
            String targetName = null;
            String errorReason = null;
            File dataRoot = DataDirectoryManager.getDataRoot(getApplicationContext());
            if (dataRoot == null) {
                errorReason = "Cheat import unavailable: data directory not resolved.";
            } else {
                File cheatsDir = new File(dataRoot, "cheats");
                if (!cheatsDir.exists() && !cheatsDir.mkdirs()) {
                    errorReason = "Unable to create cheats directory: " + cheatsDir;
                    try { DebugLog.e("Cheats", errorReason); } catch (Throwable ignored) {}
                } else {
                    String displayName = getDisplayNameForUri(uri);
                    if (TextUtils.isEmpty(displayName)) {
                        displayName = "custom_cheats.pnach";
                    }
                    if (!displayName.toLowerCase(Locale.US).endsWith(".pnach")) {
                        displayName = displayName + ".pnach";
                    }
                    File destination = createUniqueFile(cheatsDir, displayName);
                    try (InputStream in = getContentResolver().openInputStream(uri);
                         OutputStream out = new FileOutputStream(destination)) {
                        if (in == null) {
                            throw new IOException("Cheat source stream unavailable.");
                        }
                        byte[] buffer = new byte[8192];
                        int read;
                        while ((read = in.read(buffer)) != -1) {
                            out.write(buffer, 0, read);
                        }
                        out.flush();
                        success = true;
                        targetName = destination.getName();
                    } catch (Exception e) {
                        errorReason = e.getMessage();
                        if (errorReason == null || errorReason.trim().isEmpty()) {
                            errorReason = e.getClass().getSimpleName();
                        }
                        try { DebugLog.e("Cheats", "Import failed: " + errorReason); } catch (Throwable ignored) {}
                    }
                }
            }

            boolean finalSuccess = success;
            String finalName = targetName;
            String finalError = errorReason;
            runOnUiThread(() -> {
                Toast.makeText(MainActivity.this,
                        finalSuccess
                                ? getString(R.string.drawer_toast_cheats_import_success, finalName)
                                : getString(R.string.drawer_toast_cheats_import_failed),
                        Toast.LENGTH_SHORT).show();
                if (finalSuccess) {
                    try {
                        NativeApp.setEnableCheats(true);
                    } catch (Throwable ignored) {}
                } else {
                    showDrawerImportFailureDialog(R.string.drawer_error_import_cheats_title, finalError);
                }
            });
        }).start();
    }

    private void importTextureArchive(Uri uri) {
        if (uri == null) {
            return;
        }
        new Thread(() -> {
            boolean success = false;
            String errorReason = null;
            File dataRoot = DataDirectoryManager.getDataRoot(getApplicationContext());
            if (dataRoot == null) {
                errorReason = "Texture import unavailable: data directory not resolved.";
            } else {
                File texturesDir = new File(dataRoot, "textures");
                if (!texturesDir.exists() && !texturesDir.mkdirs()) {
                    errorReason = "Unable to create textures directory: " + texturesDir;
                    try { DebugLog.e("Textures", errorReason); } catch (Throwable ignored) {}
                } else {
                    try (InputStream inputStream = getContentResolver().openInputStream(uri)) {
                        if (inputStream == null) {
                            throw new IOException("Texture archive stream unavailable.");
                        }
                        try (ZipInputStream zis = new ZipInputStream(new BufferedInputStream(inputStream))) {
                            byte[] buffer = new byte[8192];
                            ZipEntry entry;
                            while ((entry = zis.getNextEntry()) != null) {
                                File outFile = new File(texturesDir, entry.getName());
                                if (!isFileInsideBase(texturesDir, outFile)) {
                                    zis.closeEntry();
                                    continue;
                                }
                                if (entry.isDirectory()) {
                                    if (!outFile.exists() && !outFile.mkdirs()) {
                                        throw new IOException("Failed to create directory " + outFile);
                                    }
                                } else {
                                    File parent = outFile.getParentFile();
                                    if (parent != null && !parent.exists() && !parent.mkdirs()) {
                                        throw new IOException("Failed to create parent " + parent);
                                    }
                                    try (OutputStream out = new BufferedOutputStream(new FileOutputStream(outFile))) {
                                        int count;
                                        while ((count = zis.read(buffer)) != -1) {
                                            out.write(buffer, 0, count);
                                        }
                                        out.flush();
                                    }
                                }
                                zis.closeEntry();
                            }
                            success = true;
                        }
                    } catch (Exception e) {
                        errorReason = e.getMessage();
                        if (errorReason == null || errorReason.trim().isEmpty()) {
                            errorReason = e.getClass().getSimpleName();
                        }
                        try { DebugLog.e("Textures", "Import failed: " + errorReason); } catch (Throwable ignored) {}
                    }
                }
            }

            boolean finalSuccess = success;
            String finalError = errorReason;
            runOnUiThread(() -> {
                Toast.makeText(MainActivity.this,
                        finalSuccess
                                ? getString(R.string.drawer_toast_textures_import_success)
                                : getString(R.string.drawer_toast_textures_import_failed),
                        Toast.LENGTH_SHORT).show();
                if (finalSuccess) {
                    try {
                        NativeApp.setSetting("EmuCore/GS", "LoadTextureReplacements", "bool", "true");
                        NativeApp.setSetting("EmuCore/GS", "LoadTextureReplacementsAsync", "bool", "true");
                    } catch (Throwable ignored) {}
                } else {
                    showDrawerImportFailureDialog(R.string.drawer_error_import_textures_title, finalError);
                }
            });
        }).start();
    }

    private String getDisplayNameForUri(Uri uri) {
        if (uri == null) {
            return null;
        }
        try (Cursor cursor = getContentResolver().query(uri, null, null, null, null)) {
            if (cursor != null && cursor.moveToFirst()) {
                int index = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME);
                if (index >= 0) {
                    return cursor.getString(index);
                }
            }
        } catch (Exception ignored) {}
        return null;
    }

    private File createUniqueFile(File directory, String name) {
        File candidate = new File(directory, name);
        if (!candidate.exists()) {
            return candidate;
        }
        String baseName = name;
        String extension = "";
        int dot = name.lastIndexOf('.');
        if (dot >= 0) {
            baseName = name.substring(0, dot);
            extension = name.substring(dot);
        }
        int index = 1;
        while (candidate.exists()) {
            candidate = new File(directory, baseName + "_" + index + extension);
            index++;
        }
        return candidate;
    }

    private boolean isFileInsideBase(File base, File target) {
        try {
            String basePath = base.getCanonicalPath();
            String targetPath = target.getCanonicalPath();
            return targetPath.startsWith(basePath + File.separator);
        } catch (IOException e) {
            return false;
        }
    }

    private void persistUriPermission(Uri uri) {
        if (uri == null) {
            return;
        }
        try {
            getContentResolver().takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
        } catch (SecurityException ignored) {}
    }

    private void showDrawerImportFailureDialog(@StringRes int titleRes, String details) {
        if (isFinishing()) {
            return;
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1 && isDestroyed()) {
            return;
        }
        String message = (details != null && !details.trim().isEmpty())
                ? details.trim()
                : getString(R.string.drawer_error_import_unknown);
        new MaterialAlertDialogBuilder(this)
                .setTitle(titleRes)
                .setMessage(message)
                .setPositiveButton(android.R.string.ok, null)
                .show();
    }

    private void showSettingsDialog() {
        View view = getLayoutInflater().inflate(R.layout.dialog_settings, null);
        AlertDialog dialog = new MaterialAlertDialogBuilder(this)
                .setView(view)
                .create();

        View btnClose = view.findViewById(R.id.btn_close);
        if (btnClose != null) btnClose.setOnClickListener(v -> dialog.dismiss());

        // Aspect ratio options
        View rg = view.findViewById(R.id.rg_aspect);
        if (rg instanceof android.widget.RadioGroup) {
            ((android.widget.RadioGroup) rg).setOnCheckedChangeListener((group, checkedId) -> {
                int type = 1; // default to Auto
                if (checkedId == R.id.rb_ar_4_3) type = 2;
                else if (checkedId == R.id.rb_ar_16_9) type = 3;
                else type = 1;
                NativeApp.setAspectRatio(type);
            });
        }

        // OSD toggles
        View swFps = view.findViewById(R.id.switch_osd_fps);
        if (swFps instanceof android.widget.Switch) {
            ((android.widget.Switch) swFps).setChecked(false);
            ((android.widget.Switch) swFps).setOnCheckedChangeListener((buttonView, isChecked) ->
                    NativeApp.setSetting("EmuCore/GS", "OsdShowFPS", "bool", isChecked ? "true" : "false"));
        }
        View swRes = view.findViewById(R.id.switch_osd_res);
        if (swRes != null) swRes.setVisibility(View.GONE);
        View swStats = view.findViewById(R.id.switch_osd_stats);
        if (swStats != null) swStats.setVisibility(View.GONE);

        View swHw = view.findViewById(R.id.switch_hw_readbacks);
        if (swHw instanceof android.widget.Switch) {
            ((android.widget.Switch) swHw).setChecked(true);
            ((android.widget.Switch) swHw).setOnCheckedChangeListener((buttonView, isChecked) ->
                    NativeApp.setSetting("EmuCore/GS", "HardwareReadbacks", "bool", isChecked ? "true" : "false"));
        }

        View btnImportMc = view.findViewById(R.id.btn_import_memcard);
        if (btnImportMc != null) {
            btnImportMc.setOnClickListener(v -> {
                Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
                i.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
                i.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, false);
                i.setType("application/octet-stream");
                String[] types = new String[]{"application/octet-stream", "application/x-binary"};
                i.putExtra(Intent.EXTRA_MIME_TYPES, types);
                startActivityForResult(Intent.createChooser(i, "Select memory card"), 9911);
            });
        }

    View btnAbout = view.findViewById(R.id.btn_about);
    if (btnAbout != null) {
        btnAbout.setOnClickListener(v -> {
        String versionName = "";
        try { versionName = getPackageManager().getPackageInfo(getPackageName(), 0).versionName; } catch (Exception ignored) {}
        String message = "ARMSX2 (" + versionName + ")\n" +
            "by ARMSX2 team\n\n" +
            "Core contributors:\n" +
            "- MoonPower — App developer\n" +
            "- jpolo — Management\n" +
            "- Medievalshell — Web developer\n" +
            "- set l — Web developer\n" +
            "- Alex — QA tester\n" +
            "- Yua — QA tester\n\n" +
            "Thanks to:\n" +
            "- pontos2024 (emulator base)\n" +
            "- PCSX2 v2.3.430 (core emulator)\n" +
            "- SDL (SDL3)\n" +
            "- Fffathur (icon design)\n" +
            "- vivimagic0 (icon design)";
        new MaterialAlertDialogBuilder(this)
            .setTitle("About")
            .setMessage(message)
            .setPositiveButton("OK", (d, w) -> d.dismiss())
            .show();
        });
    }

        dialog.show();
    }

    public final ActivityResultLauncher<Intent> startActivityResultLocalFilePlay = registerForActivityResult(
            new ActivityResultContracts.StartActivityForResult(),
            result -> {
                if (result.getResultCode() == Activity.RESULT_OK) {
                    try {
                        Intent _intent = result.getData();
                        if(_intent != null) {
                            Uri picked = _intent.getData();
                            if (picked != null) {
                                applyPerGameSettingsForUri(picked);
                                m_szGamefile = picked.toString();
                                if(!TextUtils.isEmpty(m_szGamefile)) {
                                    handleSelectedGameUri(picked);
                                }
                            }
                        }
                    } catch (Exception ignored) {}
                }
            });

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == 7722 && resultCode == Activity.RESULT_OK && data != null) {
            if (data.hasExtra("SET_RENDERER")) {
                int r = data.getIntExtra("SET_RENDERER", -1000);
                if (r != -1000) {
                    applyRendererSelection(r);
                }
            }
            if (data.getBooleanExtra(EXTRA_SETTINGS_LAYOUT_CHANGED, false)) {
                applyFullscreen();
            }
            if (data.hasExtra(EXTRA_SETTINGS_GPU_PROFILE_OVERRIDE)) {
                String selected = data.getStringExtra(EXTRA_SETTINGS_GPU_PROFILE_OVERRIDE);
                boolean persisted = data.getBooleanExtra(EXTRA_SETTINGS_GPU_PROFILE_PERSISTED, true);
                if (!TextUtils.isEmpty(selected) && !persisted) {
                    boolean recovered = false;
                    try {
                        NativeApp.setSetting("EmuCore/GS", "AndroidGpuProfileOverride", "string", selected);
                        String verify = NativeApp.getSetting("EmuCore/GS", "AndroidGpuProfileOverride", "string");
                        recovered = selected.equalsIgnoreCase(verify);
                    } catch (Throwable ignored) {}
                    int msg = recovered
                            ? R.string.settings_gpu_profile_persist_recovered
                            : R.string.settings_gpu_profile_persist_failed;
                    try { Toast.makeText(this, msg, Toast.LENGTH_LONG).show(); } catch (Throwable ignored) {}
                } else if (!TextUtils.isEmpty(selected)) {
                    try { Toast.makeText(this, R.string.settings_gpu_profile_saved_hint, Toast.LENGTH_SHORT).show(); } catch (Throwable ignored) {}
                }
            }
        }
        if (requestCode == 9911 && resultCode == Activity.RESULT_OK && data != null && data.getData() != null) {
            Uri uri = data.getData();
            try { getContentResolver().takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION); } catch (Exception ignored) {}
            if (importMemcardToSlot1(uri)) {
                NativeApp.setSetting("MemoryCards", "Slot1_Enable", "bool", "false");
                NativeApp.setSetting("MemoryCards", "Slot1_Filename", "string", "Mcd001.ps2");
                NativeApp.setSetting("MemoryCards", "Slot1_Enable", "bool", "true");
                Toast.makeText(this, R.string.settings_memory_card_inserted_slot1, Toast.LENGTH_SHORT).show();
            } else {
                Toast.makeText(this, R.string.settings_memory_card_import_failed, Toast.LENGTH_LONG).show();
            }
        }
    }

    private final ActivityResultLauncher<Intent> startActivityResultPickBios = registerForActivityResult(
            new ActivityResultContracts.StartActivityForResult(), result -> {
                if (result.getResultCode() == Activity.RESULT_OK && result.getData() != null && result.getData().getData() != null) {
                    saveBiosFromUri(result.getData().getData());
                }
            });

    private final ActivityResultLauncher<Intent> startActivityResultImportBios = registerForActivityResult(
            new ActivityResultContracts.StartActivityForResult(), result -> {
                if (result.getResultCode() == Activity.RESULT_OK && result.getData() != null && result.getData().getData() != null) {
                    Uri uri = result.getData().getData();
                    try { getContentResolver().takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION); } catch (Exception ignored) {}
                    importBiosFromUri(uri);
                    showBiosManagerDialog();
                }
            });
    private final ActivityResultLauncher<Intent> startActivityResultOnboarding =
            registerForActivityResult(new ActivityResultContracts.StartActivityForResult(), result -> {
                onboardingLaunched = false;
                if (result.getResultCode() == Activity.RESULT_OK) {
                    setOnboardingComplete();
                    runPostOnboardingPrompts();
                } else if (!isOnboardingComplete()) {
                    maybeStartOnboardingFlow();
                }
            });

    @Override
    public void onConfigurationChanged(@NonNull Configuration p_newConfig) {
        super.onConfigurationChanged(p_newConfig);
        applyGameGridConfig();
    }

    @Override
    protected void onPause() {
        RetroAchievementsBridge.setListener(null);
        NativeApp.pause();
        isVmPaused = true;
        updatePauseButtonIcon();
        super.onPause();
        ////
        if (mHIDDeviceManager != null) {
            mHIDDeviceManager.setFrozen(true);
        }
    }

    @Override
    protected void onResume() {
        NativeApp.resume();
        isVmPaused = false;
        updatePauseButtonIcon();
        super.onResume();
        RetroAchievementsBridge.setListener(retroAchievementsListener);
        RetroAchievementsBridge.refreshState();
        DiscordBridge.updateEngineActivity(this);
        ////
        if (mHIDDeviceManager != null) {
            mHIDDeviceManager.setFrozen(false);
        }
        // Re-apply immersive fullscreen when resuming
        applyFullscreen();
        loadHideTimeoutFromPrefs();
        refreshOnScreenUiStyleIfNeeded();
        refreshOnScreenUiScaleIfNeeded();
    }

	@Override
	protected void onDestroy() {
		stopEmuThread();
		LogcatRecorder.shutdown();
		super.onDestroy();
		////
		if (mHIDDeviceManager != null) {
			HIDDeviceManager.release(mHIDDeviceManager);
			mHIDDeviceManager = null;
        }
        ////
        mEmulationThread = null;
        sInstanceRef = new WeakReference<>(null);
    }

    /// ///////////////////////////////////////////////////////////////////////////////////////////

    public void Initialize() {
        File dataDir = DataDirectoryManager.getDataRoot(getApplicationContext());
        if (dataDir != null) {
            NativeApp.setDataRootOverride(dataDir.getAbsolutePath());
        }
        NativeApp.initializeOnce(getApplicationContext());
        
        // Restore custom GPU driver if one was previously selected
        restoreGpuDriver();
        
        LogcatRecorder.initialize(getApplicationContext());
        boolean recordLogs = false;
        try {
            String current = NativeApp.getSetting("Logging", "RecordAndroidLog", "bool");
            recordLogs = "true".equalsIgnoreCase(current);
        } catch (Exception ignored) {}
        LogcatRecorder.setEnabled(recordLogs);

		// Set up JNI
		SDLControllerManager.nativeSetupJNI();

		// Initialize state
        SDLControllerManager.initialize();

        mHIDDeviceManager = HIDDeviceManager.acquire(this);
    }
    
    private void restoreGpuDriver() {
        try {
            // Set native library directory for libadrenotools
            String nativeLibDir = getApplicationInfo().nativeLibraryDir;
            if (!TextUtils.isEmpty(nativeLibDir)) {
                NativeApp.setNativeLibraryDir(nativeLibDir);
            }
        } catch (Exception e) {
            // If there's any error, just continue with defaults
        }
    }

    private boolean isOnboardingComplete() {
        return getSharedPreferences(PREFS, MODE_PRIVATE).getBoolean(PREF_ONBOARDING_COMPLETE, false);
    }

    private void setOnboardingComplete() {
        getSharedPreferences(PREFS, MODE_PRIVATE).edit().putBoolean(PREF_ONBOARDING_COMPLETE, true).apply();
    }

    private void maybeStartOnboardingFlow() {
        if (postOnboardingChecksRun) {
            return;
        }
        if (isOnboardingComplete()) {
            runPostOnboardingPrompts();
            return;
        }
        if (onboardingLaunched) {
            return;
        }
        try {
            Intent onboardingIntent = new Intent(this, OnboardingActivity.class);
            startActivityResultOnboarding.launch(onboardingIntent);
            onboardingLaunched = true;
        } catch (Throwable t) {
            setOnboardingComplete();
            runPostOnboardingPrompts();
        }
    }

    private void runPostOnboardingPrompts() {
        if (postOnboardingChecksRun) {
            return;
        }
        postOnboardingChecksRun = true;
        ensureBiosPresent();
        maybeShowDataDirectoryPrompt();
    }

    private void maybeShowDataDirectoryPrompt() {
        if (storagePromptShown) {
            return;
        }
        if (DataDirectoryManager.isPromptDone(this)) {
            return;
        }
        storagePromptShown = true;
        new MaterialAlertDialogBuilder(this)
                .setTitle("Storage location")
                .setMessage("Do you wish to change where the emulator stores its data?")
                .setNegativeButton("Cancel", (dialog, which) -> {
                    DataDirectoryManager.markPromptDone(this);
                    dialog.dismiss();
                })
                .setPositiveButton("Choose", (dialog, which) -> {
                    dialog.dismiss();
                    launchDataDirectoryPicker();
                })
                .setOnDismissListener(dialog -> storagePromptShown = true)
                .show();
    }

    private void launchDataDirectoryPicker() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
        startActivityResultPickDataDir.launch(intent);
    }

    private void handleDataDirectorySelection(@NonNull Uri tree) {
        String resolvedPath = DataDirectoryManager.resolveTreeUriToPath(this, tree);
        if (resolvedPath == null || resolvedPath.trim().isEmpty()) {
            try {
                Toast.makeText(this, R.string.onboarding_storage_unusable, Toast.LENGTH_LONG).show();
            } catch (Throwable ignored) {}
            storagePromptShown = false;
            maybeShowDataDirectoryPrompt();
            return;
        }
        File targetDir = new File(resolvedPath);
        if (!targetDir.exists() && !targetDir.mkdirs()) {
            try {
                Toast.makeText(this, R.string.onboarding_storage_create_failed, Toast.LENGTH_LONG).show();
            } catch (Throwable ignored) {}
            storagePromptShown = false;
            maybeShowDataDirectoryPrompt();
            return;
        }
        if (!DataDirectoryManager.canUseDirectFileAccess(targetDir)) {
            showStorageAccessError(targetDir);
            storagePromptShown = false;
            maybeShowDataDirectoryPrompt();
            return;
        }
        File currentDir = DataDirectoryManager.getDataRoot(getApplicationContext());
        if (currentDir != null && currentDir.getAbsolutePath().equals(targetDir.getAbsolutePath())) {
            DataDirectoryManager.storeCustomDataRoot(getApplicationContext(), targetDir.getAbsolutePath(), tree.toString());
            NativeApp.setDataRootOverride(targetDir.getAbsolutePath());
            DataDirectoryManager.markPromptDone(this);
            storagePromptShown = true;
            try {
                Toast.makeText(this, R.string.onboarding_storage_already_using, Toast.LENGTH_SHORT).show();
            } catch (Throwable ignored) {}
            return;
        }
        beginDataDirectoryMigration(currentDir, targetDir, tree.toString());
    }

    private void beginDataDirectoryMigration(@NonNull File currentDir, @NonNull File targetDir, @NonNull String uriString) {
        showDataDirProgressDialog();
        NativeApp.pause();
        NativeApp.shutdown();
        new Thread(() -> {
            boolean success = DataDirectoryManager.migrateData(currentDir, targetDir);
			if (success) {
				DataDirectoryManager.storeCustomDataRoot(getApplicationContext(), targetDir.getAbsolutePath(), uriString);
				NativeApp.setDataRootOverride(targetDir.getAbsolutePath());
				NativeApp.reinitializeDataRoot(targetDir.getAbsolutePath());
				LogcatRecorder.handleDataRootChanged();
				DataDirectoryManager.copyAssetAll(getApplicationContext(), "resources");
			}
			runOnUiThread(() -> {
                dismissDataDirProgressDialog();
                if (success) {
                    DataDirectoryManager.markPromptDone(this);
                    storagePromptShown = true;
                    try {
                        Toast.makeText(this, R.string.onboarding_storage_moved, Toast.LENGTH_LONG).show();
                    } catch (Throwable ignored) {}
                } else {
                    try {
                        Toast.makeText(this, R.string.onboarding_storage_move_failed, Toast.LENGTH_LONG).show();
                    } catch (Throwable ignored) {}
                    storagePromptShown = false;
                    maybeShowDataDirectoryPrompt();
                }
            });
        }, "DataDirMigration").start();
    }

    private void showDataDirProgressDialog() {
        runOnUiThread(() -> {
            if (dataDirProgressDialog != null && dataDirProgressDialog.isShowing()) {
                return;
            }
            ProgressBar progressBar = new ProgressBar(this);
            int padding = dpToPx(24);
            progressBar.setPadding(padding, padding, padding, padding);
            dataDirProgressDialog = new MaterialAlertDialogBuilder(this)
                    .setTitle("Moving data")
                    .setMessage("Moving emulator data to the selected folder…")
                    .setView(progressBar)
                    .setCancelable(false)
                    .create();
            dataDirProgressDialog.show();
        });
    }

    private void dismissDataDirProgressDialog() {
        runOnUiThread(() -> {
            if (dataDirProgressDialog != null) {
                dataDirProgressDialog.dismiss();
                dataDirProgressDialog = null;
            }
        });
    }

    private void setSurfaceView(Object p_value) {
        FrameLayout fl_board = findViewById(R.id.fl_board);
        if (fl_board != null) {
            if (fl_board.getChildCount() > 0) {
                fl_board.removeAllViews();
            }
            ////
            if (p_value instanceof SDLSurface) {
                fl_board.addView((SDLSurface) p_value);
            }
        }
    }

    public synchronized void startEmuThread() {
        if (!hasBios()) {
            ensureBiosPresent();
            return;
        }
        stopEmuThread(false);
        for (int attempts = 0; attempts < 40 && NativeApp.hasValidVm(); attempts++) {
            SystemClock.sleep(50);
        }
        if (NativeApp.hasValidVm()) {
            NativeApp.shutdown();
            SystemClock.sleep(100);
            if (NativeApp.hasValidVm()) {
                DebugLog.w("VM", "VM still reporting active after shutdown; proceeding with clean boot");
            }
        }
        try { NativeApp.resetKeyStatus(); } catch (Throwable ignored) {}
        if (isThread()) {
            return;
        }
        isVmPaused = false;
        updatePauseButtonIcon();
        mEmulationThread = new Thread(() -> {
            runOnUiThread(() -> {
                try { if (NativeApp.isFullscreenUIEnabled()) setOnScreenControlsVisible(true); } catch (Throwable ignored) {}
                try {
                    String p = m_szGamefile;
                    if (p != null && !p.isEmpty()) {
                        Toast.makeText(this, getString(R.string.home_launching_game, p), Toast.LENGTH_SHORT).show();
                    }
                } catch (Throwable ignored) {}
            });
            NativeApp.runVMThread(m_szGamefile);
        });
        mEmulationThread.start();
    }

    private void stopEmuThread() {
        stopEmuThread(true);
    }

    private synchronized void stopEmuThread(boolean forceShutdown) {
        if (mEmulationThread != null) {
            NativeApp.shutdown();
            try {
                mEmulationThread.join();
            } catch (InterruptedException ignored) {
                Thread.currentThread().interrupt();
            }
            mEmulationThread = null;
        } else if (forceShutdown) {
            NativeApp.shutdown();
        }
        try { NativeApp.resetKeyStatus(); } catch (Throwable ignored) {}
        setFastForwardEnabled(false);
        isVmPaused = false;
        updatePauseButtonIcon();
    }

    private void restartEmuThread() {
        startEmuThread();
    }

    //////////////////////////////////////////////////////////////////////////////////////////////

    private void handleSelectedGameUri(@NonNull Uri uri) {
        String scheme = uri.getScheme();
        String lastSeg = uri.getLastPathSegment();
        String mime = null;
        try { mime = getContentResolver().getType(uri); } catch (Exception ignored) {}
        boolean hasChdSuffix = (lastSeg != null && lastSeg.toLowerCase().endsWith(".chd")) ||
                (m_szGamefile.toLowerCase().endsWith(".chd"));

        boolean headerSaysChd = false;
        byte[] header = readFirstBytes(uri, 16);
        if (header != null && header.length >= 8) {
            String hv = new String(header, 0, 8);
            headerSaysChd = "MComprHD".equals(hv);
        }

        if ("content".equals(scheme)) {
            try { getContentResolver().takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION); } catch (Throwable ignored) {}
            m_szGamefile = uri.toString();
            try { lastInput = InputSource.TOUCH; lastTouchTimeMs = System.currentTimeMillis(); setOnScreenControlsVisible(true); } catch (Throwable ignored) {}
            showHome(false);
            restartEmuThread();
            return;
        }

        m_szGamefile = uri.toString();
        try { lastInput = InputSource.TOUCH; lastTouchTimeMs = System.currentTimeMillis(); setOnScreenControlsVisible(true); } catch (Throwable ignored) {}
        showHome(false);
        restartEmuThread();
    }

    private String queryOpenableDisplayName(@NonNull Uri uri) {
        try (android.database.Cursor c = getContentResolver().query(uri, new String[]{android.provider.OpenableColumns.DISPLAY_NAME}, null, null, null)) {
            if (c != null && c.moveToFirst()) {
                int idx = c.getColumnIndex(android.provider.OpenableColumns.DISPLAY_NAME);
                if (idx >= 0) return c.getString(idx);
            }
        } catch (Exception ignored) {}
        return null;
    }

    private String copyToCache(@NonNull Uri uri, @NonNull String fileName) {
        java.io.InputStream in = null;
        java.io.FileOutputStream out = null;
        try {
            java.io.File dir = new java.io.File(getCacheDir(), "games");
            if (!dir.exists()) 
                dir.mkdirs();
            String ext = "";
            int dot = fileName.lastIndexOf('.');
            if (dot > 0) ext = fileName.substring(dot);
            String base = java.util.Objects.toString(Integer.toHexString(uri.toString().hashCode()));
            java.io.File dst = new java.io.File(dir, base + ext);
            in = getContentResolver().openInputStream(uri);
            if (in == null) return null;
            out = new java.io.FileOutputStream(dst, false);
            byte[] buf = new byte[1024 * 1024];
            int n;
            while ((n = in.read(buf)) > 0) out.write(buf, 0, n);
            out.flush();
            return dst.getAbsolutePath();
        } catch (Exception ignored) {
            return null;
        } finally {
            try { if (in != null) in.close(); } catch (Exception ignored) {}
            try { if (out != null) out.close(); } catch (Exception ignored) {}
        }
    }

    private byte[] readFirstBytes(Uri uri, int count) {
        try {
            InputStream in = getContentResolver().openInputStream(uri);
            if (in == null) return null;
            byte[] buf = new byte[count];
            int read = in.read(buf);
            in.close();
            if (read <= 0) return null;
            if (read < count) {
                byte[] cut = new byte[read];
                System.arraycopy(buf, 0, cut, 0, read);
                return cut;
            }
            return buf;
        } catch (Exception ignored) { return null; }
    }


    @Override
    public boolean onGenericMotionEvent(MotionEvent event) {
        updateLastControllerDeviceId(event.getDeviceId());
        if (SDLControllerManager.isDeviceSDLJoystick(event.getDeviceId())) {
            SDLControllerManager.handleJoystickMotionEvent(event);
            handleGamepadMotion(event);
            lastInput = InputSource.CONTROLLER;
            lastControllerTimeMs = System.currentTimeMillis();
            maybeAutoHideControls();
            return true;
        }
        return super.onGenericMotionEvent(event);
    }

    @Override
    public boolean onKeyDown(int p_keyCode, KeyEvent p_event) {
        if ((p_event.getSource() & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD) {
            if (p_event.getRepeatCount() == 0) {
                updateLastControllerDeviceId(p_event.getDeviceId());
                SDLControllerManager.onNativePadDown(p_event.getDeviceId(), p_keyCode);
                forwardKeyToPad(true, p_keyCode);
                lastInput = InputSource.CONTROLLER;
                lastControllerTimeMs = System.currentTimeMillis();
                maybeAutoHideControls();
                return true;
            }
        }
        return super.onKeyDown(p_keyCode, p_event);
    }

    @Override
    public boolean onKeyUp(int p_keyCode, KeyEvent p_event) {
        if ((p_event.getSource() & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD) {
            if (p_event.getRepeatCount() == 0) {
                updateLastControllerDeviceId(p_event.getDeviceId());
                SDLControllerManager.onNativePadUp(p_event.getDeviceId(), p_keyCode);
                forwardKeyToPad(false, p_keyCode);
                lastInput = InputSource.CONTROLLER;
                lastControllerTimeMs = System.currentTimeMillis();
                maybeAutoHideControls();
                return true;
            }
        }
        return super.onKeyUp(p_keyCode, p_event);
    }

    private void sendKeyAction(View p_view, int p_action, int p_keycode) {
        if(p_action == MotionEvent.ACTION_DOWN) {
            p_view.setPressed(true);
            int pad_force = 0;
            if (p_keycode >= 110) {
                float _abs = 90; 
                _abs = Math.min(_abs, 100);
                pad_force = (int) (_abs * 32766.0f / 100);
            }
            NativeApp.setPadButton(p_keycode, pad_force, true);
            lastInput = InputSource.TOUCH;
            lastTouchTimeMs = System.currentTimeMillis();
            maybeAutoHideControls();
        } else if(p_action == MotionEvent.ACTION_UP || p_action == MotionEvent.ACTION_CANCEL) {
            p_view.setPressed(false);
            NativeApp.setPadButton(p_keycode, 0, false);
        }
    }

    private void maybeAutoHideControls() {
        if (disableTouchControls) {
            setOnScreenControlsVisible(false);
            return;
        }
        if (lastInput == InputSource.CONTROLLER) {
            setOnScreenControlsVisible(false);
        } else {
            setOnScreenControlsVisible(true);
            getWindow().getDecorView().removeCallbacks(hideRunnable);
            if (hideDelayMs != 0L)
                getWindow().getDecorView().postDelayed(hideRunnable, hideDelayMs);
        }
    }

    private final Runnable hideRunnable = new Runnable() {
        @Override public void run() {
            if (hideDelayMs != 0L && lastInput == InputSource.TOUCH) {
                long dt = System.currentTimeMillis() - lastTouchTimeMs;
                if (dt >= hideDelayMs) setOnScreenControlsVisible(false);
            }
        }
    };

    private void setOnScreenControlsVisible(boolean visible) {
        if (disableTouchControls) {
            visible = false;
        }
        int vis = visible ? View.VISIBLE : View.GONE;
        if (llPadSelectStart != null) llPadSelectStart.setVisibility(vis);
        if (llPadRight != null) llPadRight.setVisibility(vis);
        View leftShoulders = findViewById(R.id.ll_pad_shoulders_left);
        if (leftShoulders != null) leftShoulders.setVisibility(vis);
        View rightShoulders = findViewById(R.id.ll_pad_shoulders_right);
        if (rightShoulders != null) rightShoulders.setVisibility(vis);
        
        JoystickView joystickLeft = findViewById(R.id.joystick_left);
        JoystickView joystickRight = findViewById(R.id.joystick_right);
        DPadView dpadView = findViewById(R.id.dpad_view);
        
        if (joystickLeft != null) {
            if (currentControllerMode == 2) {
                joystickLeft.setVisibility(View.GONE);
            } else {
                joystickLeft.setVisibility(vis);
            }
        }
        
        if (joystickRight != null) {
            if (currentControllerMode == 1 || currentControllerMode == 2) {
                joystickRight.setVisibility(View.GONE);
            } else {
                joystickRight.setVisibility(vis);
            }
        }
        
        if (dpadView != null) {
            if (currentControllerMode == 1) {
                dpadView.setVisibility(View.GONE);
            } else {
                dpadView.setVisibility(vis);
            }
        }
        
        if (!visible) {
            hideDrawerToggle();
        }
        if (!disableTouchControls && visible) {
            try {
                getWindow().getDecorView().removeCallbacks(hideRunnable);
                if (hideDelayMs != 0L && lastInput == InputSource.TOUCH) {
                    getWindow().getDecorView().postDelayed(hideRunnable, hideDelayMs);
                }
            } catch (Throwable ignored) {}
        } else if (disableTouchControls) {
            try {
                getWindow().getDecorView().removeCallbacks(hideRunnable);
            } catch (Throwable ignored) {}
        }
    }

    private void showDrawerToggleTemporarily() {
        if (disableTouchControls) {
            return;
        }
        if (drawerToggle == null) {
            return;
        }
        drawerToggle.removeCallbacks(hideDrawerToggleRunnable);
        drawerToggle.setVisibility(View.VISIBLE);
        long delay = hideDelayMs != 0L ? hideDelayMs : 2000L;
        drawerToggle.postDelayed(hideDrawerToggleRunnable, delay);
    }

    private void hideDrawerToggle() {
        if (drawerToggle == null) {
            return;
        }
        drawerToggle.removeCallbacks(hideDrawerToggleRunnable);
        drawerToggle.setVisibility(View.GONE);
    }

    static class SpacingDecoration extends RecyclerView.ItemDecoration {
        private int spacePx;
        SpacingDecoration(int spacePx) { this.spacePx = spacePx; }
        void updateSpacing(int spacePx) { this.spacePx = spacePx; }
        @Override public void getItemOffsets(@NonNull android.graphics.Rect outRect, @NonNull View view, @NonNull RecyclerView parent, @NonNull RecyclerView.State state) {
            outRect.left = spacePx;
            outRect.right = spacePx;
            outRect.top = spacePx;
            outRect.bottom = spacePx;
        }
    }

    private void applyGameGridConfig() {
        if (rvGames == null) return;
        final int span = getGameGridSpanCount();
        if (!listMode) {
            if (gamesGridLayoutManager == null) {
                gamesGridLayoutManager = new GridLayoutManager(this, span);
                rvGames.setLayoutManager(gamesGridLayoutManager);
            } else {
                gamesGridLayoutManager.setSpanCount(span);
                if (rvGames.getLayoutManager() != gamesGridLayoutManager) {
                    rvGames.setLayoutManager(gamesGridLayoutManager);
                }
            }
        }
        if (gameSpacingDecoration != null) {
            gameSpacingDecoration.updateSpacing(getResources().getDimensionPixelSize(R.dimen.game_selector_tile_spacing));
            rvGames.invalidateItemDecorations();
        }
        final int padding = getResources().getDimensionPixelSize(R.dimen.game_selector_grid_padding);
        rvGames.setPadding(padding, padding, padding, padding);
    }

    private int getGameGridSpanCount() {
        return getResources().getInteger(R.integer.game_selector_span_count);
    }

    private int dpToPx(int dp) { return Math.round(dp * getResources().getDisplayMetrics().density); }

    private void showStorageAccessError(File targetDir) {
        boolean canGrant = Build.VERSION.SDK_INT >= Build.VERSION_CODES.R && !DataDirectoryManager.hasAllFilesAccess();
        String message = "Android denied direct file access for:\n" + targetDir.getAbsolutePath() +
                "\n\nGrant 'Allow access to all files' in system settings or choose a folder inside ARMSX2's storage.";
        MaterialAlertDialogBuilder builder = new MaterialAlertDialogBuilder(this)
            .setTitle("Permission required")
            .setMessage(message)
            .setNegativeButton("OK", (d, w) -> d.dismiss());
        if (canGrant) {
            builder.setPositiveButton("Open settings", (d, w) -> {
                d.dismiss();
                openAllFilesAccessSettings();
            });
        }
        builder.show();
    }

    private void openAllFilesAccessSettings() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            try {
                Intent intent = new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION);
                intent.setData(Uri.parse("package:" + getPackageName()));
                startActivity(intent);
            } catch (Exception ignored) {
                try {
                    Intent intent = new Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION);
                    startActivity(intent);
                } catch (Exception ignored2) {}
            }
        }
    }

    private final ActivityResultLauncher<Intent> startActivityResultPickIso = registerForActivityResult(
            new ActivityResultContracts.StartActivityForResult(), result -> {
                if (result.getResultCode() == Activity.RESULT_OK && result.getData() != null && result.getData().getData() != null) {
                    Uri uri = result.getData().getData();
                    String name = queryOpenableDisplayName(uri);
                    String low = name != null ? name.toLowerCase() : uri.toString().toLowerCase();
                    if (!low.endsWith(".iso")) {
                        try { new MaterialAlertDialogBuilder(this).setTitle("Not an ISO").setMessage("Please select a .iso file.").setPositiveButton("OK", (d,w)-> d.dismiss()).show(); } catch (Throwable ignored) {}
                        return;
                    }
                    performIsoToChd(uri, name);
                }
            });

    private void startPickIsoForChd() {
        try {
            Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            i.addCategory(Intent.CATEGORY_OPENABLE);
            // ENSURE we find this shit
            i.setType("*/*");
            String[] mimeTypes = {
                "application/octet-stream",
                "application/x-iso9660-image", 
                "application/x-cd-image",
                "application/x-raw-disk-image"
            };
            i.putExtra(Intent.EXTRA_MIME_TYPES, mimeTypes);
            startActivityResultPickIso.launch(i);
        } catch (Throwable t) {
            try { Toast.makeText(this, R.string.home_unable_open_file_picker, Toast.LENGTH_SHORT).show(); } catch (Throwable ignored) {}
        }
    }

    private void performIsoToChd(Uri isoUri, String isoDisplayName) {
        if (!NativeApp.hasNativeTools) {
            String errorMsg = "ARMSX2 Native Tools library could not be called, it was probably not bundled with the app please rebuild the app with the library in place.";
            android.util.Log.e("ARMSX2_CHD", "Library not available: " + errorMsg);
            try {
                new MaterialAlertDialogBuilder(this)
                        .setTitle("Library Not Available")
                        .setMessage(errorMsg)
                        .setPositiveButton("OK", (d, w) -> d.dismiss())
                        .show();
            } catch (Throwable ignored) {}
            return;
        }

        new Thread(() -> {
            String inputPath = null;
            String outputPath = null;
            String resultMessage = null;
            boolean success = false;
            String sourceTitle = stripFileExtension(isoDisplayName);
            if (TextUtils.isEmpty(sourceTitle) && isoUri != null) {
                sourceTitle = stripFileExtension(isoUri.getLastPathSegment());
            }
            String sourceSerial = GameScanner.parseSerialFromString(sourceTitle);

            try {
                if (TextUtils.isEmpty(sourceSerial)) {
                    try {
                        sourceSerial = GameScanner.tryExtractIsoSerial(getContentResolver(), isoUri);
                    } catch (Throwable ignored) {}
                }

                // Get real file path from URI
                android.util.Log.i("ARMSX2_CHD", "Starting ISO to CHD conversion for: " + isoDisplayName);
                android.util.Log.i("ARMSX2_CHD", "Input URI: " + isoUri.toString());
                
                inputPath = getFilePathFromUri(isoUri);
                if (inputPath == null) {
                    resultMessage = "Could not access the selected ISO file. Please ensure the file is accessible.";
                    android.util.Log.e("ARMSX2_CHD", "Failed to get file path from URI: " + isoUri.toString());
                    return;
                }
                android.util.Log.i("ARMSX2_CHD", "Input path resolved to: " + inputPath);

                // Generate output CHD path to match what Rust will create 
                outputPath = inputPath.replaceAll("\\.iso$", ".chd");
                android.util.Log.i("ARMSX2_CHD", "Expected output path: " + outputPath);

                // Call native conversion 
                android.util.Log.i("ARMSX2_CHD", "Calling native conversion...");
                try {
                    android.util.Log.d("ARMSX2_CHD", "Input path bytes: " + java.util.Arrays.toString(inputPath.getBytes("UTF-8")));
                    android.util.Log.d("ARMSX2_CHD", "Input path length: " + inputPath.length());
                    android.util.Log.d("ARMSX2_CHD", "Input path string: '" + inputPath + "'");
                } catch (java.io.UnsupportedEncodingException e) {
                    android.util.Log.e("ARMSX2_CHD", "Failed to encode path as UTF-8: " + e.getMessage());
                }
                int result = NativeApp.convertIsoToChd(inputPath);
                android.util.Log.i("ARMSX2_CHD", "Native conversion returned code: " + result);
                
                success = handleConversionResult(result, inputPath, outputPath);
                
                if (success) {
                    final String chdCachePath = outputPath;
                    final String chdDisplayName = isoDisplayName;
                    final String finalSourceSerial = sourceSerial;
                    final String finalSourceTitle = sourceTitle;
                    android.util.Log.i("ARMSX2_CHD", "Conversion succeeded. Prompting user to choose CHD save location.");
                    runOnUiThread(() -> promptForChdSave(
                            chdCachePath, chdDisplayName, isoUri, finalSourceSerial, finalSourceTitle));
                    resultMessage = null;
                } else {
                    resultMessage = getErrorMessage(result) + "\n\nInput: " + inputPath + "\nOutput: " + outputPath;
                    android.util.Log.e("ARMSX2_CHD", "Conversion failed with code " + result + ": " + getErrorMessage(result));
                    android.util.Log.e("ARMSX2_CHD", "Input: " + inputPath);
                    android.util.Log.e("ARMSX2_CHD", "Output: " + outputPath);
                }

            } catch (Throwable e) {
                resultMessage = "Conversion failed with exception: " + e.getMessage() + 
                              "\n\nInput: " + inputPath + "\nOutput: " + outputPath;
                android.util.Log.e("ARMSX2_CHD", "Conversion exception: " + e.getClass().getSimpleName() + ": " + e.getMessage(), e);
                android.util.Log.e("ARMSX2_CHD", "Input: " + inputPath);
                android.util.Log.e("ARMSX2_CHD", "Output: " + outputPath);
            } finally {
                if (inputPath != null) {
                    File tempFile = new File(inputPath);
                    if (tempFile.exists() && tempFile.getParent().equals(getCacheDir().getAbsolutePath())) {
                        if (tempFile.delete()) {
                            android.util.Log.d("ARMSX2_CHD", "Cleaned up temporary file: " + inputPath);
                        } else {
                            android.util.Log.w("ARMSX2_CHD", "Failed to clean up temporary file: " + inputPath);
                        }
                    }
                }
                
                final String finalMessage = resultMessage;
                final boolean finalSuccess = success;
                runOnUiThread(() -> {
                    if (finalMessage != null) {
                        showConversionResult(finalSuccess, finalMessage);
                    }
                });
            }
        }, "IsoToChdConverter").start();
    }

    private String getFilePathFromUri(Uri uri) {
        android.util.Log.d("ARMSX2_CHD", "getFilePathFromUri called with: " + uri.toString());
        try {
            // Get display name from content resolver
            android.database.Cursor cursor = getContentResolver().query(uri, null, null, null, null);
            if (cursor != null) {
                try {
                    if (cursor.moveToFirst()) {
                        int displayNameIndex = cursor.getColumnIndex(android.provider.OpenableColumns.DISPLAY_NAME);
                        if (displayNameIndex >= 0) {
                            String displayName = cursor.getString(displayNameIndex);
                            
                            File cacheDir = getCacheDir();
                            File tempFile = new File(cacheDir, displayName);
                            android.util.Log.d("ARMSX2_CHD", "Creating temporary file: " + tempFile.getAbsolutePath());
                            
                            try (java.io.InputStream input = getContentResolver().openInputStream(uri);
                                 java.io.FileOutputStream output = new java.io.FileOutputStream(tempFile)) {
                                
                                if (input != null) {
                                    byte[] buffer = new byte[8192];
                                    int bytesRead;
                                    long totalBytes = 0;
                                    while ((bytesRead = input.read(buffer)) != -1) {
                                        output.write(buffer, 0, bytesRead);
                                        totalBytes += bytesRead;
                                    }
                                    android.util.Log.d("ARMSX2_CHD", "Copied " + totalBytes + " bytes to cache");
                                    return tempFile.getAbsolutePath();
                                }
                            }
                        }
                    }
                } finally {
                    cursor.close();
                }
            }
        } catch (Throwable e) {
            android.util.Log.e("ARMSX2_CHD", "Exception in getFilePathFromUri: " + e.getMessage(), e);
        }
        android.util.Log.w("ARMSX2_CHD", "getFilePathFromUri returning null - failed to resolve path");
        return null;
    }

    private void promptForChdSave(String chdCachePath,
                                  String displayName,
                                  @Nullable Uri sourceUri,
                                  @Nullable String sourceSerial,
                                  @Nullable String sourceTitle) {
        File chdFile = new File(chdCachePath);
        if (!chdFile.exists()) {
            android.util.Log.e("ARMSX2_CHD", "CHD file missing in cache, cannot prompt for save: " + chdCachePath);
            showConversionResult(false, "Converted file could not be found. Please try converting again.");
            return;
        }

        pendingChdCachePath = chdCachePath;
        pendingChdDisplayName = displayName;
        pendingChdSourceUri = sourceUri;
        pendingChdSourceSerial = sourceSerial;
        pendingChdSourceTitle = sourceTitle;

        String baseName = displayName;
        if (baseName == null || baseName.trim().isEmpty()) {
            baseName = chdFile.getName();
        }
        String lower = baseName.toLowerCase(Locale.US);
        if (lower.endsWith(".iso")) {
            baseName = baseName.substring(0, baseName.length() - 4);
            lower = baseName.toLowerCase(Locale.US);
        }
        if (!lower.endsWith(".chd")) {
            baseName = baseName + ".chd";
        }

        android.util.Log.d("ARMSX2_CHD", "Prompting user to save CHD as: " + baseName);

        Intent intent = new Intent(Intent.ACTION_CREATE_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("application/octet-stream");
        intent.putExtra(Intent.EXTRA_TITLE, baseName);

        startActivityResultSaveChd.launch(intent);
    }

    private boolean saveChdToUri(File chdFile, Uri destinationUri) {
        android.util.Log.d("ARMSX2_CHD", "Saving CHD from cache to destination: " + destinationUri);
        try (java.io.FileInputStream input = new java.io.FileInputStream(chdFile);
             java.io.OutputStream output = getContentResolver().openOutputStream(destinationUri, "w")) {

            if (output == null) {
                android.util.Log.e("ARMSX2_CHD", "Content resolver returned null output stream for destination");
                return false;
            }

            byte[] buffer = new byte[8192];
            int bytesRead;
            long totalBytes = 0;
            while ((bytesRead = input.read(buffer)) != -1) {
                output.write(buffer, 0, bytesRead);
                totalBytes += bytesRead;
            }
            output.flush();
            android.util.Log.d("ARMSX2_CHD", "Wrote " + totalBytes + " bytes to destination URI");
            return true;
        } catch (Throwable e) {
            android.util.Log.e("ARMSX2_CHD", "Failed to copy CHD to destination: " + e.getMessage(), e);
            return false;
        }
    }

    private boolean handleConversionResult(int result, String inputPath, String outputPath) {
        return result == 0; // All good!
    }

    private String getErrorMessage(int errorCode) {
        switch (errorCode) {
            case -1: return "Error: Null pointer provided to conversion function";
            case -2: return "Error: Invalid UTF-8 encoding in file paths";
            case -3: return "Error: Input ISO file not found";
            case -4: return "Error: Input path is not a regular file";
            case -5: return "Error: Failed to create output CHD file";
            case -6: return "Error: I/O error during conversion";
            case -7: return "Error: Too many hunks for CHD format";
            case -8: return "Error: Numeric overflow during conversion";
            case -9: return "Error: Unexpected end of ISO data";
            case -100: return "Error: Internal conversion error";
            default: return "Error: Unknown conversion error (code: " + errorCode + ")";
        }
    }

    private void showConversionResult(boolean success, String message) {
        try {
            new MaterialAlertDialogBuilder(this)
                    .setTitle(success ? "Conversion Successful" : "Conversion Failed")
                    .setMessage(message)
                    .setPositiveButton("OK", (d, w) -> d.dismiss())
                    .show();
        } catch (Throwable ignored) {}
    }

    private void loadHideTimeoutFromPrefs() {
        try {
            android.content.SharedPreferences sp = getSharedPreferences(PREFS, Context.MODE_PRIVATE);
            int sec = sp.getInt(PREF_HIDE_CONTROLS_SECONDS, 10); 
            if (sec < 0) sec = 0;
            if (sec > 60) sec = 60;
            hideDelayMs = (sec == 0) ? 0L : sec * 1000L;
        } catch (Throwable ignored) { hideDelayMs = 2500L; }
    }

    private void forwardKeyToPad(boolean down, int keycode) {
        int mapped = ControllerMappingManager.getPadCodeForKey(keycode);
        if (mapped == ControllerMappingManager.NO_MAPPING) {
            mapped = keycode;
        }
        NativeApp.setPadButton(mapped, 0, down);
    }

    private void handleGamepadMotion(MotionEvent e) {
        updateLastControllerDeviceId(e.getDeviceId());
        float lx = getCenteredAxis(e, MotionEvent.AXIS_X);
        float ly = getCenteredAxis(e, MotionEvent.AXIS_Y);
        sendAnalog(111, Math.max(0f, lx));
        sendAnalog(113, Math.max(0f, -lx));
        sendAnalog(112, Math.max(0f, ly));
        sendAnalog(110, Math.max(0f, -ly));

        float rx = getCenteredAxis(e, MotionEvent.AXIS_RX);
        float ry = getCenteredAxis(e, MotionEvent.AXIS_RY);
        if (rx == 0f && ry == 0f) {
            rx = getCenteredAxis(e, MotionEvent.AXIS_Z);
            ry = getCenteredAxis(e, MotionEvent.AXIS_RZ);
        }
        sendAnalog(121, Math.max(0f, rx));
        sendAnalog(123, Math.max(0f, -rx));
        sendAnalog(122, Math.max(0f, ry));
        sendAnalog(120, Math.max(0f, -ry));

        float ltrig = e.getAxisValue(MotionEvent.AXIS_LTRIGGER);
        float rtrig = e.getAxisValue(MotionEvent.AXIS_RTRIGGER);
        if (ltrig == 0f) ltrig = e.getAxisValue(MotionEvent.AXIS_BRAKE);
        if (rtrig == 0f) rtrig = e.getAxisValue(MotionEvent.AXIS_GAS);
        sendAnalog(KeyEvent.KEYCODE_BUTTON_L2, normalizeTrigger(ltrig), TRIGGER_DEADZONE);
        sendAnalog(KeyEvent.KEYCODE_BUTTON_R2, normalizeTrigger(rtrig), TRIGGER_DEADZONE);

        float hatX = e.getAxisValue(MotionEvent.AXIS_HAT_X);
        float hatY = e.getAxisValue(MotionEvent.AXIS_HAT_Y);
        final float hatThreshold = 0.4f;
        boolean nowLeft = hatX < -hatThreshold;
        boolean nowRight = hatX > hatThreshold;
        boolean nowUp = hatY < -hatThreshold;
        boolean nowDown = hatY > hatThreshold;
        setAxisState(hatLeft, nowLeft, KeyEvent.KEYCODE_DPAD_LEFT);  hatLeft = nowLeft;
        setAxisState(hatRight, nowRight, KeyEvent.KEYCODE_DPAD_RIGHT); hatRight = nowRight;
        setAxisState(hatUp, nowUp, KeyEvent.KEYCODE_DPAD_UP); hatUp = nowUp;
        setAxisState(hatDown, nowDown, KeyEvent.KEYCODE_DPAD_DOWN); hatDown = nowDown;
    }

    private void setAxisState(boolean prev, boolean now, int code) {
        if (prev == now) return;
        if (!ControllerMappingManager.isPadCodeBound(code)) {
            return;
        }
        NativeApp.setPadButton(code, 0, now);
    }

    private float getCenteredAxis(MotionEvent e, int axis) {
        final InputDevice device = e.getDevice();
        if (device != null) {
            final InputDevice.MotionRange range = device.getMotionRange(axis, e.getSource());
            if (range != null) {
                float value = e.getAxisValue(axis);
                float flat = range.getFlat();
                if (Math.abs(value) > flat) return value;
            }
        }
        return 0f;
    }

    private void sendAnalog(int keyCode, float normalized) {
        sendAnalog(keyCode, normalized, ANALOG_DEADZONE);
    }

    private void sendAnalog(int keyCode, float normalized, float deadzone) {
        if (Float.isNaN(normalized)) normalized = 0f;
        int padCode = ControllerMappingManager.getPadCodeForKey(keyCode);
        if (padCode == ControllerMappingManager.NO_MAPPING) {
            padCode = keyCode;
        }
        if (!ControllerMappingManager.isPadCodeBound(padCode)) {
            analogStates.put(padCode, 0);
            NativeApp.setPadButton(padCode, 0, false);
            return;
        }
        float value = Math.min(1f, Math.max(0f, normalized));
        if (value < deadzone) value = 0f;
        int scaled = Math.round(value * 255f);
        int prev = analogStates.get(padCode, -1);
        if (prev == scaled) return;
        analogStates.put(padCode, scaled);
        NativeApp.setPadButton(padCode, scaled, scaled > 0);
    }

    private float normalizeTrigger(float raw) {
        if (Float.isNaN(raw)) return 0f;
        if (raw < 0f) {
            return Math.min(1f, Math.max(0f, (raw + 1f) * 0.5f));
        }
        return Math.min(1f, raw);
    }

    private static void updateLastControllerDeviceId(int deviceId) {
        if (deviceId >= 0) {
            sLastControllerDeviceId = deviceId;
        }
    }

    public static void requestControllerRumble(float large, float small) {
        MainActivity activity = sInstanceRef != null ? sInstanceRef.get() : null;
        if (activity == null) {
            if (!sVibrationEnabled) stopControllerRumbleStatic();
            return;
        }
        activity.runOnUiThread(() -> activity.dispatchControllerRumble(large, small));
    }

    private void dispatchControllerRumble(float large, float small) {
        if (!sVibrationEnabled) {
            stopControllerRumble();
            return;
        }
        final float clampedLarge = clamp01(large);
        final float clampedSmall = clamp01(small);
        final float combined = Math.max(clampedLarge, clampedSmall);
        final int deviceId = sLastControllerDeviceId;

        if (combined <= 0f) {
            stopControllerRumble();
            return;
        }

        boolean usedController = false;
        if (deviceId >= 0 && SDLControllerManager.isDeviceSDLJoystick(deviceId)) {
            usedController = true;
            try {
                SDLControllerManager.hapticRumble(deviceId, clampedLarge, clampedSmall, RUMBLE_DURATION_MS);
            } catch (Throwable ignored) {}
        }

        final int vibratorServiceId = 999999;
        if (!usedController) {
            try {
                SDLControllerManager.hapticRun(vibratorServiceId, combined, RUMBLE_DURATION_MS);
            } catch (Throwable ignored) {}
        }
    }

    private void stopControllerRumble() {
        stopControllerRumbleStatic();
    }

    private static void stopControllerRumbleStatic() {
        final int deviceId = sLastControllerDeviceId;
        try {
            if (deviceId >= 0) SDLControllerManager.hapticStop(deviceId);
        } catch (Throwable ignored) {}
        try {
            SDLControllerManager.hapticStop(999999);
        } catch (Throwable ignored) {}
    }

    private static float clamp01(float value) {
        if (Float.isNaN(value)) return 0f;
        if (value <= 0f) return 0f;
        return Math.min(1f, value);
    }

    private void refreshVibrationPreference() {
        boolean enabled = true;
        try {
            String vibration = NativeApp.getSetting("Pad1", "Vibration", "bool");
            if (vibration != null && !vibration.isEmpty()) {
                enabled = !"false".equalsIgnoreCase(vibration);
            } else {
                NativeApp.setSetting("Pad1", "Vibration", "bool", "true");
                enabled = true;
            }
        } catch (Exception ignored) {}
        setVibrationPreference(enabled);
    }

    public static void setVibrationPreference(boolean enabled) {
        sVibrationEnabled = enabled;
        MainActivity activity = sInstanceRef != null ? sInstanceRef.get() : null;
        if (!enabled) {
            if (activity != null) {
                activity.runOnUiThread(activity::stopControllerRumble);
            } else {
                stopControllerRumbleStatic();
            }
        }
    }

    private final ActivityResultLauncher<Intent> startActivityResultPickDataDir = registerForActivityResult(
            new ActivityResultContracts.StartActivityForResult(), result -> {
                if (result.getResultCode() == Activity.RESULT_OK && result.getData() != null) {
                    Intent data = result.getData();
                    Uri tree = data.getData();
                    if (tree != null) {
                        final int takeFlags = data.getFlags() & (Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
                        try {
                            getContentResolver().takePersistableUriPermission(tree, takeFlags);
                        } catch (SecurityException ignored) {}
                        handleDataDirectorySelection(tree);
                        return;
                    }
                }
                storagePromptShown = false;
                maybeShowDataDirectoryPrompt();
            });

    //////////////////////////////////////////////////////////////////////////////////////////////

    // HOME FLOW
    private final ActivityResultLauncher<Intent> startActivityResultPickFolder = registerForActivityResult(
            new ActivityResultContracts.StartActivityForResult(), result -> {
                if (result.getResultCode() == Activity.RESULT_OK && result.getData() != null) {
                    Intent data = result.getData();
                    Uri tree = data.getData();
                    if (tree != null) {
                        try {
                            final int takeFlags = data.getFlags() & (Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
                            getContentResolver().takePersistableUriPermission(tree, takeFlags);
                        } catch (SecurityException ignored) {}
                        gamesFolderUri = tree;
                        try {
                            getSharedPreferences(PREFS, Context.MODE_PRIVATE)
                                .edit().putString(PREF_GAMES_URI, tree.toString()).apply();
                        } catch (Throwable ignored) {}
                        scanGamesFolder(tree);
                    }
                }
            });

    private final ActivityResultLauncher<Intent> startActivityResultImportCheats = registerForActivityResult(
            new ActivityResultContracts.StartActivityForResult(), result -> {
                if (result.getResultCode() == Activity.RESULT_OK && result.getData() != null) {
                    Uri uri = result.getData().getData();
                    if (uri != null) {
                        persistUriPermission(uri);
                        importCheatFile(uri);
                    }
                }
            });

    private final ActivityResultLauncher<Intent> startActivityResultImportTextures = registerForActivityResult(
            new ActivityResultContracts.StartActivityForResult(), result -> {
                if (result.getResultCode() == Activity.RESULT_OK && result.getData() != null) {
                    Uri uri = result.getData().getData();
                    if (uri != null) {
                        persistUriPermission(uri);
                        importTextureArchive(uri);
                    }
                }
            });

    private final ActivityResultLauncher<Intent> startActivityResultImportPnach = registerForActivityResult(
            new ActivityResultContracts.StartActivityForResult(), result -> {
                if (result.getResultCode() == Activity.RESULT_OK && result.getData() != null) {
                    Uri uri = result.getData().getData();
                    if (uri != null) {
                        processSelectedPnach(uri);
                    }
                }
            });

    private void pickGamesFolder() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
        startActivityResultPickFolder.launch(intent);
    }

    private void openPnachPicker() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("*/*");
        startActivityResultImportPnach.launch(intent);
    }

    private void processSelectedPnach(Uri uri) {
        try {
            StringBuilder sb = new StringBuilder();
            try (InputStream is = getContentResolver().openInputStream(uri);
                 BufferedReader reader = new BufferedReader(new InputStreamReader(is))) {
                String line;
                while ((line = reader.readLine()) != null) {
                    String trimmed = line.trim();
                    if (trimmed.startsWith("[") && trimmed.endsWith("]")) {
                        sb.append("// ").append(line).append("\n");
                    } else {
                        sb.append(line).append("\n");
                    }
                }
            }

            String serial = NativeApp.getGameSerial();
            int crcInt = NativeApp.getGameCRC();

            if (TextUtils.isEmpty(serial) || crcInt == 0) {
                Toast.makeText(this, "Erro: Start the Game First", Toast.LENGTH_SHORT).show();
                return;
            }

            String fileName = String.format("%s_%08X.pnach", serial, crcInt);
            File cheatsDir = new File(DataDirectoryManager.getDataRoot(this), "cheats");
            if (!cheatsDir.exists()) cheatsDir.mkdirs();

            File targetFile = new File(cheatsDir, fileName);
            try (FileOutputStream fos = new FileOutputStream(targetFile)) {
                fos.write(sb.toString().getBytes());
            }
            NativeApp.reloadCheats();
            NativeApp.setEnableCheats(false);
            NativeApp.setEnableCheats(true);

            Toast.makeText(this, "Cheat Imported: " + fileName, Toast.LENGTH_LONG).show();

        } catch (Exception e) {
            Toast.makeText(this, "Erro: " + e.getMessage(), Toast.LENGTH_SHORT).show();
        }
    }

    private void launchTextureImportPicker() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("application/zip");
        intent.putExtra(Intent.EXTRA_TITLE, getString(R.string.drawer_import_textures_picker_title));
        intent.putExtra(Intent.EXTRA_MIME_TYPES, new String[]{"application/zip", "application/x-zip-compressed"});
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
        startActivityResultImportTextures.launch(intent);
    }

    private void scanGamesFolder(Uri folder) {
    List<GameEntry> entries = GameScanner.scanFolder(this, folder);
        try {
            java.util.Collections.sort(entries, (a, b) -> {
                String ta = a != null ? (a.title != null ? a.title : "") : "";
                String tb = b != null ? (b.title != null ? b.title : "") : "";
                int ga = sortGroup(ta);
                int gb = sortGroup(tb);
                if (ga != gb) return Integer.compare(ga, gb);
                return ta.compareToIgnoreCase(tb);
            });
        } catch (Throwable ignored) {}
    gamesAdapter.update(entries);
        final List<GameEntry> toResolve = new ArrayList<>();
        for (GameEntry ge : entries) {
            try {
                if (ge != null && (ge.serial == null || ge.serial.isEmpty())) {
                    String name = ge.title != null ? ge.title.toLowerCase() : "";
                    if (name.endsWith(".iso") || name.endsWith(".img") || name.endsWith(".bin") ||
                            name.endsWith(".chd") || name.endsWith(".cso") || name.endsWith(".zso") || name.endsWith(".gz"))
                        toResolve.add(ge);
                }
            } catch (Throwable ignored) {}
        }
        if (!toResolve.isEmpty()) {
            new Thread(() -> {
                android.content.ContentResolver cr = getContentResolver();
                int n = 0;
                for (GameEntry ge : toResolve) {
                    try {
                        String info = NativeApp.getDiskInfo(ge.uri.toString());
                        if (!TextUtils.isEmpty(info)) {
                            String[] parts = info.split("\\|");
                            if (parts.length >= 3) {
                                ge.gameTitle = parts[0];
                                ge.serial = parts[1];
                                ge.region = parts[2];
                                if (isChdEntry(ge.uri, ge.title)) {
                                    persistChdMetadata(ge.uri, ge.serial, ge.gameTitle);
                                }
                                n++;
                            }
                            if (n > 0) runOnUiThread(() -> gamesAdapter.notifyDataSetChanged());
                        }
                    } catch (Throwable ignored) {
                    }
                }
            }, "RedumpResolve").start();
        }
        if (etSearch != null && etSearch.getText() != null && etSearch.length() > 0) {
            gamesAdapter.setFilter(etSearch.getText().toString());
        }
        if (rvGames != null && gamesAdapter.getItemCount() > 0) {
            rvGames.post(() -> {
                rvGames.requestFocus(); 
                rvGames.postDelayed(() -> {
                    RecyclerView.ViewHolder vh = rvGames.findViewHolderForAdapterPosition(0);
                    if (vh != null && vh.itemView != null) {
                        vh.itemView.requestFocus();
                    }
                }, 100); 
            });
        }
        boolean empty = entries.isEmpty();
    try { Toast.makeText(this, getString(R.string.home_games_found_count, entries.size()), Toast.LENGTH_SHORT).show(); } catch (Throwable ignored) {}
        if (tvEmpty != null) {
            tvEmpty.setText(empty ? getString(R.string.home_no_games_detected) : "");
            tvEmpty.setVisibility(empty ? View.VISIBLE : View.GONE);
        }
        if (emptyContainer != null) emptyContainer.setVisibility(empty ? View.VISIBLE : View.GONE);
        if (rvGames != null) rvGames.setVisibility(empty ? View.GONE : View.VISIBLE);
        if (!empty) showHome(true);

    }

    private static int sortGroup(String title) {
        if (title == null) return 2;
        String t = title.trim();
        if (t.isEmpty()) return 2;
        char c = t.charAt(0);
        if (c >= '0' && c <= '9') return 0; 
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) return 1; 
        return 2;
    }

    private void showHome(boolean show) {
        if (show) {
            restorePerGameOverrides();
        }

        if (homeContainer != null) {
            homeContainer.setVisibility(show ? View.VISIBLE : View.GONE);
            onBackPressCallback.setEnabled(!show);
        }
        if (drawerLayout != null) {
            drawerLayout.setVisibility(show ? View.VISIBLE : View.GONE);
            try {
                drawerLayout.setDrawerLockMode(show ? DrawerLayout.LOCK_MODE_UNLOCKED : DrawerLayout.LOCK_MODE_LOCKED_CLOSED);
                drawerLayout.setScrimColor(android.graphics.Color.TRANSPARENT);
            } catch (Throwable ignored) {}
        }
        if (inGameDrawer != null) {
            if (show) {
                try {
                    inGameDrawer.closeDrawer(GravityCompat.START);
                } catch (Throwable ignored) {}
                inGameDrawer.setDrawerLockMode(DrawerLayout.LOCK_MODE_LOCKED_CLOSED);
            } else {
                inGameDrawer.setDrawerLockMode(disableTouchControls ? DrawerLayout.LOCK_MODE_LOCKED_CLOSED : DrawerLayout.LOCK_MODE_UNLOCKED);
            }
        }
        if (show) {
            setFastForwardEnabled(false);
            if (rvGames != null && rvGames.getVisibility() == View.VISIBLE && gamesAdapter != null && gamesAdapter.getItemCount() > 0) {
                rvGames.post(() -> {
                    rvGames.requestFocus();
                    RecyclerView.ViewHolder vh = rvGames.findViewHolderForAdapterPosition(0);
                    if (vh != null && vh.itemView != null) {
                        vh.itemView.requestFocus();
                    }
                });
            }
        }
        if (show || disableTouchControls) {
            hideDrawerToggle();
        }
        int vis = show ? View.GONE : View.VISIBLE;
        setOnScreenControlsVisible(!show);
        if (llPadSelectStart != null) llPadSelectStart.setVisibility(vis);
        if (llPadRight != null) llPadRight.setVisibility(vis);
        View j = findViewById(R.id.joystick_left);
        if (j != null) j.setVisibility(vis);
        View jr = findViewById(R.id.joystick_right);
        if (jr != null) jr.setVisibility(vis);
        View d = findViewById(R.id.dpad_view);
        if (d != null) d.setVisibility(vis);
        try {
            if (show) {
                setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_FULL_SENSOR);
            } else {
                if (TextUtils.isEmpty(m_szGamefile)) {
                    setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_FULL_SENSOR);
                } else {
                    setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE);
                }
            }
        } catch (Throwable ignored) {}
        if (show && tvEmpty != null && gamesFolderUri == null) {
            tvEmpty.setText(R.string.home_nav_choose_games_folder);
            tvEmpty.setVisibility(View.VISIBLE);
            if (emptyContainer != null) emptyContainer.setVisibility(View.VISIBLE);
            if (rvGames != null) rvGames.setVisibility(View.GONE);
        }
        if (bgImage != null) {
            if (show) {
                android.graphics.drawable.Drawable ddraw = bgImage.getDrawable();
                bgImage.setVisibility(ddraw != null ? View.VISIBLE : View.GONE);
            } else {
                bgImage.setVisibility(View.GONE);
            }
        }
        applyFullscreen();
    }

    private boolean isHomeVisible() {
        return homeContainer == null || homeContainer.getVisibility() == View.VISIBLE;
    }

    private void shutdownVmToHome() {
        pendingGameUri = null;
        try {
            getWindow().getDecorView().removeCallbacks(pendingLaunchRunnable);
        } catch (Throwable ignored) {}
        stopEmuThread();
        m_szGamefile = "";
        showHome(true);
        lastInput = InputSource.TOUCH;
        lastTouchTimeMs = System.currentTimeMillis();
        setOnScreenControlsVisible(false);
        applyFullscreen();
        requestControllerRumble(0f, 0f);
        isVmPaused = false;
        updatePauseButtonIcon();
        setFastForwardEnabled(false);
    }

    private void enforceTouchControlsPolicy() {
        if (!disableTouchControls) {
            return;
        }
        setOnScreenControlsVisible(false);
        View joystick = findViewById(R.id.joystick_left);
        if (joystick != null) joystick.setVisibility(View.GONE);
        View joystickRight = findViewById(R.id.joystick_right);
        if (joystickRight != null) joystickRight.setVisibility(View.GONE);
        View dpad = findViewById(R.id.dpad_view);
        if (dpad != null) dpad.setVisibility(View.GONE);
        View padLeft = findViewById(R.id.ll_pad_select_start);
        if (padLeft != null) padLeft.setVisibility(View.GONE);
        View padRight = findViewById(R.id.ll_pad_right);
        if (padRight != null) padRight.setVisibility(View.GONE);
        View leftShoulders = findViewById(R.id.ll_pad_shoulders_left);
        if (leftShoulders != null) leftShoulders.setVisibility(View.GONE);
        View rightShoulders = findViewById(R.id.ll_pad_shoulders_right);
        if (rightShoulders != null) rightShoulders.setVisibility(View.GONE);
        hideDrawerToggle();
        setFastForwardEnabled(false);
        if (inGameDrawer != null) {
            try {
                inGameDrawer.closeDrawer(GravityCompat.START);
            } catch (Throwable ignored) {}
            inGameDrawer.setDrawerLockMode(DrawerLayout.LOCK_MODE_LOCKED_CLOSED);
        }
    }

    // region Background image picker
    private static final String PREF_BG_L = "bg_landscape";
    private static final String PREF_BG_P = "bg_portrait";
    private void pickBackgroundImage(boolean portrait) {
        Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        i.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
        i.setType("image/*");
        i.putExtra("PORTRAIT_BG", portrait);
        startActivityResultPickBg.launch(i);
    }
    private final ActivityResultLauncher<Intent> startActivityResultPickBg = registerForActivityResult(
            new ActivityResultContracts.StartActivityForResult(), result -> {
                if (result.getResultCode() == Activity.RESULT_OK && result.getData() != null) {
                    Intent data = result.getData();
                    Uri img = data.getData();
                    boolean portrait = data.getBooleanExtra("PORTRAIT_BG", false);
                    if (img != null) {
                        try {
                            final int takeFlags = data.getFlags() & (Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
                            getContentResolver().takePersistableUriPermission(img, takeFlags);
                        } catch (SecurityException ignored) {}
                        getSharedPreferences(PREFS, MODE_PRIVATE).edit()
                                .putString(portrait ? PREF_BG_P : PREF_BG_L, img.toString())
                                .apply();
                        applySavedBackground();
                    }
                }
            });

    private final ActivityResultLauncher<Intent> startActivityResultSwapDisc = registerForActivityResult(
            new ActivityResultContracts.StartActivityForResult(),
            result -> {
                if (result.getResultCode() == Activity.RESULT_OK && result.getData() != null) {
                    Uri uri = result.getData().getData();
                    if (uri != null) {
                        try {
                            getContentResolver().takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
                        } catch (Exception ignored) {}

                        String displayName = queryOpenableDisplayName(uri);
                        if (displayName == null) {
                            displayName = uri.getLastPathSegment();
                        }
                        if (NativeApp.changeDisc(uri.toString())) {
                            Toast.makeText(this, "Disc Changed: " + displayName, Toast.LENGTH_SHORT).show();
                        } else {
                            Toast.makeText(this, "Failure to Change Disk: " + displayName, Toast.LENGTH_LONG).show();
                        }
                    }
                }
            });

    private void SwapDisc() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("*/*");
        String[] mimeTypes = {"application/octet-stream", "application/x-iso9660-image", "application/x-cd-image"};
        intent.putExtra(Intent.EXTRA_MIME_TYPES, mimeTypes);
        startActivityResultSwapDisc.launch(intent);
    }

    private void applySavedBackground() {
        if (bgImage == null) return;
        android.content.SharedPreferences sp = getSharedPreferences(PREFS, MODE_PRIVATE);
        String l = sp.getString(PREF_BG_L, null);
        String p = sp.getString(PREF_BG_P, null);
        boolean isPortrait = getResources().getConfiguration().orientation == Configuration.ORIENTATION_PORTRAIT;
        String use = isPortrait ? (p != null ? p : l) : (l != null ? l : p);
        if (use == null || use.isEmpty()) { bgImage.setImageDrawable(null); bgImage.setVisibility(View.GONE); return; }
        try (InputStream is = getContentResolver().openInputStream(Uri.parse(use))) {
            if (is != null) {
                android.graphics.Bitmap bmp = android.graphics.BitmapFactory.decodeStream(is);
                if (bmp != null) {
                    bgImage.setImageBitmap(bmp);
                    bgImage.setVisibility(View.VISIBLE);
                    if (android.os.Build.VERSION.SDK_INT >= 31) {
                        try {
                            bgImage.setRenderEffect(android.graphics.RenderEffect.createBlurEffect(0f, 8f, android.graphics.Shader.TileMode.CLAMP));
                        } catch (Throwable ignored) {}
                    }
                }
            }
        } catch (Exception ignored) {}
    }
    private void clearBackgroundImages() {
        getSharedPreferences(PREFS, MODE_PRIVATE).edit().remove(PREF_BG_L).remove(PREF_BG_P).apply();
        if (bgImage != null) { bgImage.setImageDrawable(null); bgImage.setVisibility(View.GONE); }
        try { Toast.makeText(this, R.string.home_background_cleared, Toast.LENGTH_SHORT).show(); } catch (Throwable ignored) {}
    }
    // endregion Background image picker

    private void onGameSelected(GameEntry entry) {
        launchGameWithPreflight(entry.uri);
    }

    // Cheap but effective: if emulator isn't running yet, boot BIOS first, then load the game like the File button flow.
    private void launchGameWithPreflight(@NonNull Uri uri) {
        applyPerGameSettingsForUri(uri);
        if (isThread()) {
            handleSelectedGameUri(uri);
            return;
        }
        // Start BIOS first
        try { Toast.makeText(this, R.string.home_preflight_boot_bios, Toast.LENGTH_SHORT).show(); } catch (Throwable ignored) {}
        pendingGameUri = uri;
        pendingLaunchRetries = 0;
        bootBios();
        getWindow().getDecorView().postDelayed(pendingLaunchRunnable, 900);
        schedulePreflightFallback();
    }

    private final Runnable pendingLaunchRunnable = new Runnable() {
        @Override public void run() {
            if (pendingGameUri == null) return;
            try { Toast.makeText(MainActivity.this, R.string.home_preflight_launch_selected_game, Toast.LENGTH_SHORT).show(); } catch (Throwable ignored) {}
            Uri toLaunch = pendingGameUri;
            pendingGameUri = null;
            handleSelectedGameUri(toLaunch);
        }
    };

    private void schedulePreflightFallback() {
        try {
            getWindow().getDecorView().postDelayed(() -> {
                if (pendingGameUri != null) {
                    Uri toLaunch = pendingGameUri;
                    pendingGameUri = null;
                    handleSelectedGameUri(toLaunch);
                }
            }, 2000);
        } catch (Throwable ignored) {}
    }

    private void bootBios() {
        m_szGamefile = "";
        showHome(false);
        try { setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_FULL_SENSOR); } catch (Throwable ignored) {}
        try {
            lastInput = InputSource.TOUCH;
            lastTouchTimeMs = System.currentTimeMillis();
            setOnScreenControlsVisible(true);
        } catch (Throwable ignored) {}
        if (isThread()) {
            restartEmuThread();
        } else {
            startEmuThread();
        }
    }

    static class GameEntry {
        final String title;      
        final Uri uri;
        String serial;           
        String gameTitle;
        public String region;
        GameEntry(String t, Uri u) { title = t; uri = u; }
        String fileTitleNoExt() {
            int i = title.lastIndexOf('.');
            return (i > 0) ? title.substring(0, i) : title;
        }
    }

    static class GameScanner {
    static final String[] EXTS = new String[]{".iso", ".img", ".bin", ".cso", ".zso", ".chd", ".gz"};
    static List<GameEntry> scanFolder(Context ctx, Uri treeUri) {
            List<GameEntry> out = new ArrayList<>();
            android.content.ContentResolver cr = ctx.getContentResolver();
            try {
                String rootId = android.provider.DocumentsContract.getTreeDocumentId(treeUri);
                scanChildren(cr, treeUri, rootId, out, 0, 3);
            } catch (Exception ignored) {}
            for (GameEntry e : out) {
                if (e == null || !isChdEntry(e.uri, e.title)) {
                    continue;
                }
                Pair<String, String> metadata = getPersistedChdMetadata(ctx, e.uri);
                if (metadata == null) {
                    continue;
                }
                if (TextUtils.isEmpty(e.serial) && !TextUtils.isEmpty(metadata.first)) {
                    e.serial = metadata.first;
                }
                if (TextUtils.isEmpty(e.gameTitle) && !TextUtils.isEmpty(metadata.second)) {
                    e.gameTitle = metadata.second;
                }
            }
            return out;
        }

        static List<String> debugList(Context ctx, Uri treeUri) {
            List<String> out = new ArrayList<>();
            try {
                android.content.ContentResolver cr = ctx.getContentResolver();
                String rootId = android.provider.DocumentsContract.getTreeDocumentId(treeUri);
                debugChildren(cr, treeUri, rootId, out, 0, 3, "/");
            } catch (Exception e) { out.add("Error: " + e.getMessage()); }
            return out;
        }

        private static void scanChildren(android.content.ContentResolver cr, Uri treeUri, String parentDocId,
                                         List<GameEntry> out, int depth, int maxDepth) {
            if (depth > maxDepth) return;
            Uri children = android.provider.DocumentsContract.buildChildDocumentsUriUsingTree(treeUri, parentDocId);
            try (android.database.Cursor c = cr.query(children, new String[]{
                    android.provider.DocumentsContract.Document.COLUMN_DOCUMENT_ID,
                    android.provider.DocumentsContract.Document.COLUMN_DISPLAY_NAME,
                    android.provider.DocumentsContract.Document.COLUMN_MIME_TYPE
            }, null, null, null)) {
                if (c == null) return;
                while (c.moveToNext()) {
                    String docId = c.getString(0);
                    String name = c.getString(1);
                    String mime = c.getString(2);
                    if (mime != null && mime.equals(android.provider.DocumentsContract.Document.MIME_TYPE_DIR)) {
                        scanChildren(cr, treeUri, docId, out, depth + 1, maxDepth);
                        continue;
                    }
                    if (name == null) name = "Unknown";
                    String lower = name.toLowerCase();
                    boolean matchExt = false;
                    for (String ext : EXTS) { if (lower.endsWith(ext)) { matchExt = true; break; } }
                    boolean matchMime = false;
                    if (mime != null) {
                        String lm = mime.toLowerCase();
                        if (lm.contains("iso9660") || lm.equals("application/x-iso9660-image")) matchMime = true;
                    }
                    boolean match = matchExt || matchMime;
                    if (!match) continue;
                    Uri doc = android.provider.DocumentsContract.buildDocumentUriUsingTree(treeUri, docId);
                    GameEntry e = new GameEntry(name, doc);
                    String ft = e.fileTitleNoExt();
                    String s = parseSerialFromString(ft);
                    if (s != null) e.serial = s;
                    String lowerName = name != null ? name.toLowerCase() : "";
                    if (e.serial == null && (lowerName.endsWith(".iso") || lowerName.endsWith(".img") || lowerName.endsWith(".cso") || lowerName.endsWith(".zso"))) {
                        try {
                            String isoSerial = tryExtractIsoSerial(cr, doc);
                            if (isoSerial != null) e.serial = isoSerial;
                        } catch (Throwable t) {
                            try { DebugLog.d("ISO", "Serial parse failed: " + t.getMessage()); } catch (Throwable ignored) {}
                        }
                    }
                    if (e.serial == null && lowerName.endsWith(".bin")) {
                        try {
                            String quick = tryExtractBinSerialQuick(cr, doc);
                            if (quick != null) e.serial = quick;
                        } catch (Throwable t) {
                            try { DebugLog.d("BIN", "Quick serial scan failed: " + t.getMessage()); } catch (Throwable ignored) {}
                        }
                    }
                    out.add(e);
                }
            } catch (Exception ignored) {}
        }

        private static void debugChildren(android.content.ContentResolver cr, Uri treeUri, String parentDocId,
                                           List<String> out, int depth, int maxDepth, String pathPrefix) {
            if (depth > maxDepth) return;
            Uri children = android.provider.DocumentsContract.buildChildDocumentsUriUsingTree(treeUri, parentDocId);
            try (android.database.Cursor c = cr.query(children, new String[]{
                    android.provider.DocumentsContract.Document.COLUMN_DOCUMENT_ID,
                    android.provider.DocumentsContract.Document.COLUMN_DISPLAY_NAME,
                    android.provider.DocumentsContract.Document.COLUMN_MIME_TYPE
            }, null, null, null)) {
                if (c == null) return;
                while (c.moveToNext()) {
                    String docId = c.getString(0);
                    String name = c.getString(1);
                    String mime = c.getString(2);
                    String display = pathPrefix + (name != null ? name : "<null>") + (mime != null && mime.equals(android.provider.DocumentsContract.Document.MIME_TYPE_DIR) ? "/" : "");
                    boolean dir = mime != null && mime.equals(android.provider.DocumentsContract.Document.MIME_TYPE_DIR);
                    boolean accept = false;
                    if (!dir && name != null) {
                        String lower = name.toLowerCase();
                        boolean matchExt = false;
                        for (String ext : EXTS) { if (lower.endsWith(ext)) { matchExt = true; break; } }
                        boolean matchMime = false;
                        if (mime != null) {
                            String lm = mime.toLowerCase();
                            if (lm.contains("iso9660") || lm.equals("application/x-iso9660-image")) matchMime = true;
                        }
                        accept = matchExt || matchMime;
                    }
                    out.add("[" + (mime == null ? "null" : mime) + "] " + display + (dir ? "" : (accept ? "  -> accepted" : "  -> skipped")));
                    if (mime != null && mime.equals(android.provider.DocumentsContract.Document.MIME_TYPE_DIR)) {
                        debugChildren(cr, treeUri, docId, out, depth + 1, maxDepth, display);
                    }
                }
            } catch (Exception e) { out.add("Error listing: " + e.getMessage()); }
        }
        static String stripExt(String name) {
            int i = name.lastIndexOf('.');
            return (i > 0) ? name.substring(0, i) : name;
        }

    static String parseSerialFromString(String s) {
            if (s == null) return null;
        java.util.regex.Pattern p = java.util.regex.Pattern.compile(
            "(S[CL](?:ES|US|PS|CS)?[-_]?[0-9]{3,5}(?:\\.[0-9]{2})?)",
            java.util.regex.Pattern.CASE_INSENSITIVE);
            java.util.regex.Matcher m = p.matcher(s);
            if (m.find()) {
                String v = m.group(1).toUpperCase();
                v = v.replace('_','-');
                v = v.replace(".", "");
                v = v.replaceAll("^([A-Z]+)([0-9])", "$1-$2");
                return v;
            }
            return null;
        }

        static String tryExtractIsoSerial(android.content.ContentResolver cr, Uri uri) throws java.io.IOException {
            final int SECTOR = 2048;
            byte[] pvd = readRange(cr, uri, 16L * SECTOR, SECTOR);
            if (pvd == null || pvd.length < SECTOR) return null;
            if (pvd[0] != 0x01 || pvd[1] != 'C' || pvd[2] != 'D' || pvd[3] != '0' || pvd[4] != '0' || pvd[5] != '1')
                return null;
            int rootLBA = u32le(pvd, 156 + 2);
            int rootSize = u32le(pvd, 156 + 10);
            if (rootLBA <= 0 || rootSize <= 0 || rootSize > 512 * 1024) rootSize = 64 * 1024;
            byte[] dir = readRange(cr, uri, (long) rootLBA * SECTOR, rootSize);
            if (dir == null) return null;
            int off = 0;
            while (off < dir.length) {
                int len = u8(dir, off);
                if (len == 0) {
                    int next = ((off / SECTOR) + 1) * SECTOR;
                    if (next <= off) break;
                    off = next;
                    continue;
                }
                if (off + len > dir.length) break;
                int lba = u32le(dir, off + 2);
                int size = u32le(dir, off + 10);
                int nameLen = u8(dir, off + 32);
                int namePos = off + 33;
                if (namePos + nameLen <= dir.length && nameLen > 0) {
                    String name = new String(dir, namePos, nameLen, java.nio.charset.StandardCharsets.US_ASCII);
                    if (!(nameLen == 1 && (dir[namePos] == 0 || dir[namePos] == 1))) {
                        String norm = name;
                        int semi = norm.indexOf(';');
                        if (semi >= 0) norm = norm.substring(0, semi);
                        if ("SYSTEM.CNF".equalsIgnoreCase(norm)) {
                            int readSize = Math.min(size, 4096);
                            byte[] cnf = readRange(cr, uri, (long) lba * SECTOR, readSize);
                            if (cnf != null) {
                                String txt = new String(cnf, java.nio.charset.StandardCharsets.US_ASCII);
                java.util.regex.Matcher m = java.util.regex.Pattern.compile(
                    "BOOT\\d*\\s*=\\s*[^\\\\\\r\\n]*\\\\([A-Z0-9_\\.]+)",
                    java.util.regex.Pattern.CASE_INSENSITIVE).matcher(txt);
                                if (m.find()) {
                                    String bootElf = m.group(1);
                                    String serial = parseSerialFromString(bootElf);
                                    if (serial != null) return serial;
                                }
                            }
                            break;
                        }
                    }
                }
                off += len;
            }
            return null;
        }

        static String tryExtractBinSerialQuick(android.content.ContentResolver cr, Uri uri) throws java.io.IOException {
            final int MAX = 8 * 1024 * 1024; 
            byte[] buf;
            try (java.io.InputStream in = CsoUtils.openInputStream(cr, uri)) {
                if (in == null) return null;
                java.io.ByteArrayOutputStream bos = new java.io.ByteArrayOutputStream(Math.min(MAX, 1 << 20));
                byte[] tmp = new byte[64 * 1024];
                int total = 0;
                while (total < MAX) {
                    int want = Math.min(tmp.length, MAX - total);
                    int r = in.read(tmp, 0, want);
                    if (r <= 0) break;
                    bos.write(tmp, 0, r);
                    total += r;
                }
                buf = bos.toByteArray();
            }
            if (buf == null || buf.length == 0) return null;
            String txt = new String(buf, java.nio.charset.StandardCharsets.US_ASCII);
            java.util.regex.Matcher m = java.util.regex.Pattern.compile(
                "BOOT\\d*\\s*=\\s*[^\\\\\\r\\n]*\\\\([A-Z0-9_\\.]+)",
                java.util.regex.Pattern.CASE_INSENSITIVE).matcher(txt);
            if (m.find()) {
                String bootElf = m.group(1);
                String serial = parseSerialFromString(bootElf);
                if (serial != null) return serial;
            }
            String s2 = parseSerialFromString(txt);
            return s2;
        }

        private static int u8(byte[] a, int i) { return (i >= 0 && i < a.length) ? (a[i] & 0xFF) : 0; }
        private static int u32le(byte[] a, int i) {
            if (i + 3 >= a.length) return 0;
            return (a[i] & 0xFF) | ((a[i+1] & 0xFF) << 8) | ((a[i+2] & 0xFF) << 16) | ((a[i+3] & 0xFF) << 24);
        }
        private static byte[] readRange(android.content.ContentResolver cr, Uri uri, long offset, int size) throws java.io.IOException {
            if (size <= 0) return null;
            if (size > 2 * 1024 * 1024) size = 2 * 1024 * 1024;
            byte[] csoBytes = CsoUtils.readRange(cr, uri, offset, size);
            if (csoBytes != null) {
                return csoBytes;
            }
            try (java.io.InputStream in = cr.openInputStream(uri)) {
                if (in == null) return null;
                long toSkip = offset;
                byte[] skipBuf = new byte[8192];
                while (toSkip > 0) {
                    long skipped = in.skip(toSkip);
                    if (skipped <= 0) {
                        int r = in.read(skipBuf, 0, (int) Math.min(skipBuf.length, toSkip));
                        if (r <= 0) break;
                        toSkip -= r;
                    } else {
                        toSkip -= skipped;
                    }
                }
                byte[] buf = new byte[size];
                int off2 = 0;
                while (off2 < size) {
                    int r = in.read(buf, off2, size - off2);
                    if (r <= 0) break;
                    off2 += r;
                }
                if (off2 == 0) return null;
                if (off2 < size) return java.util.Arrays.copyOf(buf, off2);
                return buf;
            }
        }
    }

    private static final class CsoUtils {
        private static final int MAGIC_CISO = 0x4F534943;
        private static final int MAGIC_ZISO = 0x4F53495A;

        private CsoUtils() {}

        @Nullable
        static byte[] readRange(android.content.ContentResolver cr, Uri uri, long offset, int size) {
            CsoReader reader = null;
            try {
                reader = CsoReader.open(cr, uri);
                if (reader == null) {
                    return null;
                }
                return reader.readRange(offset, size);
            } catch (Exception ignored) {
                return null;
            } finally {
                closeQuietly(reader);
            }
        }

        @Nullable
        static java.io.InputStream openInputStream(android.content.ContentResolver cr, Uri uri) throws java.io.IOException {
            CsoReader reader = CsoReader.open(cr, uri);
            if (reader == null) {
                return cr.openInputStream(uri);
            }
            return new CsoInputStream(reader);
        }

        private static void closeQuietly(@Nullable Closeable closeable) {
            if (closeable == null) {
                return;
            }
            try {
                closeable.close();
            } catch (Exception ignored) {}
        }

        private static final class CsoReader implements Closeable {
            private final ParcelFileDescriptor descriptor;
            private final FileInputStream inputStream;
            private final FileChannel channel;
            private final long uncompressedSize;
            private final int blockSize;
            private final int alignShift;
            private final int[] indexTable;
            private final int blockCount;

            private CsoReader(ParcelFileDescriptor descriptor, FileInputStream inputStream, FileChannel channel,
                              long uncompressedSize, int blockSize, int alignShift, int[] indexTable) {
                this.descriptor = descriptor;
                this.inputStream = inputStream;
                this.channel = channel;
                this.uncompressedSize = uncompressedSize;
                this.blockSize = blockSize;
                this.alignShift = alignShift;
                this.indexTable = indexTable;
                this.blockCount = indexTable.length - 1;
            }

            static CsoReader open(android.content.ContentResolver cr, Uri uri) throws java.io.IOException {
                ParcelFileDescriptor pfd = cr.openFileDescriptor(uri, "r");
                if (pfd == null) {
                    return null;
                }
                FileInputStream fis = null;
                try {
                    fis = new FileInputStream(pfd.getFileDescriptor());
                    FileChannel channel = fis.getChannel();
                    ByteBuffer header = ByteBuffer.allocate(0x18).order(ByteOrder.LITTLE_ENDIAN);
                    if (channel.read(header) < 0x18) {
                        closeQuietly(fis);
                        closeQuietly(pfd);
                        return null;
                    }
                    header.flip();
                    int magic = header.getInt();
                    if (magic != MAGIC_CISO && magic != MAGIC_ZISO) {
                        closeQuietly(fis);
                        closeQuietly(pfd);
                        return null;
                    }
                    int headerSize = header.getInt();
                    long uncompressedSize = header.getLong();
                    int blockSize = header.getInt();
                    header.get();
                    int align = header.get() & 0xFF;
                    header.get();
                    header.get();
                    if (blockSize <= 0 || uncompressedSize <= 0 || headerSize < 0x18) {
                        closeQuietly(fis);
                        closeQuietly(pfd);
                        return null;
                    }
                    int entryCount = (headerSize - 0x18) / 4;
                    if (entryCount <= 1) {
                        closeQuietly(fis);
                        closeQuietly(pfd);
                        return null;
                    }
                    int[] table = new int[entryCount];
                    ByteBuffer indexBuffer = ByteBuffer.allocate(entryCount * 4).order(ByteOrder.LITTLE_ENDIAN);
                    if (channel.read(indexBuffer) < entryCount * 4) {
                        closeQuietly(fis);
                        closeQuietly(pfd);
                        return null;
                    }
                    indexBuffer.flip();
                    for (int i = 0; i < entryCount; i++) {
                        table[i] = indexBuffer.getInt();
                    }
                    return new CsoReader(pfd, fis, channel, uncompressedSize, blockSize, align, table);
                } catch (Exception e) {
                    closeQuietly(fis);
                    closeQuietly(pfd);
                    throw e;
                }
            }

            byte[] readRange(long offset, int size) throws java.io.IOException {
                if (size <= 0 || offset < 0 || offset >= uncompressedSize) {
                    return null;
                }
                int cappedSize = (int)Math.min(size, uncompressedSize - offset);
                byte[] output = new byte[cappedSize];
                byte[] blockBuffer = new byte[blockSize];
                int startBlock = (int)(offset / blockSize);
                int endBlock = Math.min(blockCount, (int)Math.ceil((offset + cappedSize) / (double)blockSize));
                int outOffset = 0;
                int offsetInBlock = (int)(offset % blockSize);
                long remaining = cappedSize;
                for (int block = startBlock; block < endBlock && remaining > 0; block++) {
                    int produced = readBlockInto(block, blockBuffer);
                    if (produced <= 0) {
                        break;
                    }
                    int start = (block == startBlock) ? offsetInBlock : 0;
                    if (start >= produced) {
                        continue;
                    }
                    int copyLength = (int)Math.min(produced - start, remaining);
                    System.arraycopy(blockBuffer, start, output, outOffset, copyLength);
                    outOffset += copyLength;
                    remaining -= copyLength;
                }
                if (outOffset == 0) {
                    return null;
                }
                if (outOffset < output.length) {
                    return Arrays.copyOf(output, outOffset);
                }
                return output;
            }

            int readBlockInto(int blockIndex, byte[] dest) throws java.io.IOException {
                if (blockIndex < 0 || blockIndex >= blockCount) {
                    return -1;
                }
                long startOffset = (long)(indexTable[blockIndex] & 0x7FFFFFFFL) << alignShift;
                long endOffset = (long)(indexTable[blockIndex + 1] & 0x7FFFFFFFL) << alignShift;
                boolean isPlain = (indexTable[blockIndex] & 0x80000000) != 0;
                int compressedSize = (int)Math.max(0, endOffset - startOffset);
                int expectedSize = (int)Math.min(blockSize, uncompressedSize - ((long)blockIndex * blockSize));
                if (expectedSize <= 0) {
                    return 0;
                }
                if (compressedSize == 0) {
                    Arrays.fill(dest, 0, expectedSize, (byte)0);
                    return expectedSize;
                }
                byte[] compressed = new byte[compressedSize];
                ByteBuffer buffer = ByteBuffer.wrap(compressed);
                channel.position(startOffset);
                int readTotal = 0;
                while (buffer.hasRemaining()) {
                    int r = channel.read(buffer);
                    if (r <= 0) {
                        break;
                    }
                    readTotal += r;
                }
                if (readTotal != compressedSize) {
                    return -1;
                }
                if (isPlain) {
                    int toCopy = Math.min(expectedSize, compressedSize);
                    System.arraycopy(compressed, 0, dest, 0, toCopy);
                    if (toCopy < expectedSize) {
                        Arrays.fill(dest, toCopy, expectedSize, (byte)0);
                    }
                    return expectedSize;
                }
                Inflater inflater = new Inflater(true);
                try {
                    inflater.setInput(compressed);
                    int total = 0;
                    while (!inflater.finished() && total < expectedSize) {
                        int r = inflater.inflate(dest, total, expectedSize - total);
                        if (r <= 0) {
                            if (inflater.needsInput() || inflater.finished()) {
                                break;
                            }
                        } else {
                            total += r;
                        }
                    }
                    if (total <= 0) {
                        Arrays.fill(dest, 0, expectedSize, (byte)0);
                        return expectedSize;
                    }
                    return total;
                } catch (Exception ignored) {
                    return -1;
                } finally {
                    inflater.end();
                }
            }

            int getBlockCount() {
                return blockCount;
            }

            int getBlockSize() {
                return blockSize;
            }

            long getUncompressedSize() {
                return uncompressedSize;
            }

            @Override
            public void close() throws java.io.IOException {
                try {
                    channel.close();
                } finally {
                    try {
                        inputStream.close();
                    } finally {
                        descriptor.close();
                    }
                }
            }
        }

        private static final class CsoInputStream extends java.io.InputStream {
            private final CsoReader reader;
            private final byte[] blockBuffer;
            private int currentBlock = 0;
            private int blockPos = 0;
            private int blockLimit = 0;
            private long bytesRemaining;

            CsoInputStream(CsoReader reader) {
                this.reader = reader;
                this.blockBuffer = new byte[reader.getBlockSize()];
                this.bytesRemaining = reader.getUncompressedSize();
            }

            @Override
            public int read() throws java.io.IOException {
                byte[] single = new byte[1];
                int r = read(single, 0, 1);
                if (r <= 0) {
                    return -1;
                }
                return single[0] & 0xFF;
            }

            @Override
            public int read(@NonNull byte[] b, int off, int len) throws java.io.IOException {
                if (b == null) {
                    throw new NullPointerException();
                }
                if (off < 0 || len < 0 || len > b.length - off) {
                    throw new IndexOutOfBoundsException();
                }
                if (len == 0) {
                    return 0;
                }
                if (bytesRemaining <= 0) {
                    return -1;
                }
                int total = 0;
                while (len > 0 && bytesRemaining > 0) {
                    if (blockPos >= blockLimit) {
                        if (currentBlock >= reader.getBlockCount()) {
                            break;
                        }
                        blockLimit = reader.readBlockInto(currentBlock, blockBuffer);
                        currentBlock++;
                        blockPos = 0;
                        if (blockLimit <= 0) {
                            break;
                        }
                    }
                    int available = blockLimit - blockPos;
                    int copy = Math.min(len, available);
                    copy = (int)Math.min(copy, bytesRemaining);
                    if (copy <= 0) {
                        break;
                    }
                    System.arraycopy(blockBuffer, blockPos, b, off, copy);
                    off += copy;
                    len -= copy;
                    total += copy;
                    blockPos += copy;
                    bytesRemaining -= copy;
                }
                return total > 0 ? total : -1;
            }

            @Override
            public void close() throws java.io.IOException {
                reader.close();
            }
        }
    }

    static class RedumpDB {
        static class Result { String serial; String name; }
        private static final Object LOCK = new Object();
        private static java.util.Map<String, Result> sMd5SizeToResult = null; 

        private static String md5ToLower(String s) { return s != null ? s.trim().toLowerCase() : null; }

        private static String externalResourcesPath(Context ctx) {
            File base = DataDirectoryManager.getDataRoot(ctx);
            return new File(base, "resources").getAbsolutePath();
        }

        private static void ensureLoaded(Context ctx) {
            if (sMd5SizeToResult != null) return;
            synchronized (LOCK) {
                if (sMd5SizeToResult != null) return;
                sMd5SizeToResult = new java.util.HashMap<>(8192);
                File f = new File(externalResourcesPath(ctx), "RedumpDatabase.yaml");
                java.io.BufferedReader br = null;
                try {
                    java.io.InputStream in;
                    if (f.exists()) {
                        in = new java.io.FileInputStream(f);
                    } else {
                        try { in = ctx.getAssets().open("resources/RedumpDatabase.yaml"); }
                        catch (Exception e) {
                            try { DebugLog.w("Redump", "Database not found (assets and external)"); } catch (Throwable ignored) {}
                            return;
                        }
                    }
                    br = new java.io.BufferedReader(new java.io.InputStreamReader(in, java.nio.charset.StandardCharsets.UTF_8));
                    String line;
                    java.util.List<String[]> pendingHashes = new java.util.ArrayList<>(); // each [md5,size]
                    String curSerial = null;
                    String curName = null;
                    String pendingMd5 = null;
                    String pendingSize = null;
                    while ((line = br.readLine()) != null) {
                        String t = line.trim();
                        if (t.isEmpty() || t.startsWith("#")) continue;
                        if (t.startsWith("- hashes:")) {
                            if (curSerial != null && !pendingHashes.isEmpty()) {
                                for (String[] hs : pendingHashes) {
                                    String md5 = md5ToLower(hs[0]);
                                    String size = hs[1] != null ? hs[1].trim() : null;
                                    if (md5 != null && size != null) {
                                        Result r = new Result();
                                        r.serial = curSerial;
                                        r.name = (curName != null ? curName : curSerial);
                                        sMd5SizeToResult.put(md5 + "|" + size, r);
                                    }
                                }
                            }
                            pendingHashes.clear();
                            curSerial = null;
                            curName = null;
                            pendingMd5 = null; pendingSize = null;
                            continue;
                        }
                        if (t.startsWith("- md5:")) {
                            int idx = t.indexOf(':');
                            if (idx >= 0) { pendingMd5 = t.substring(idx + 1).trim(); }
                            continue;
                        }
                        if (t.startsWith("md5:")) {
                            int idx = t.indexOf(':');
                            if (idx >= 0) { pendingMd5 = t.substring(idx + 1).trim(); }
                            continue;
                        }
                        if (t.startsWith("size:")) {
                            int idx = t.indexOf(':');
                            if (idx >= 0) { pendingSize = t.substring(idx + 1).trim(); }
                            if (pendingMd5 != null && pendingSize != null) {
                                pendingHashes.add(new String[]{pendingMd5, pendingSize});
                                pendingMd5 = null; pendingSize = null;
                            }
                            continue;
                        }
                        if (t.startsWith("serial:")) {
                            int idx = t.indexOf(':');
                            curSerial = (idx >= 0 ? t.substring(idx + 1).trim() : null);
                            continue;
                        }
                        if (t.startsWith("name:")) {
                            int idx = t.indexOf(':');
                            curName = (idx >= 0 ? t.substring(idx + 1).trim() : null);
                            continue;
                        }
                    }
                    if (curSerial != null && !pendingHashes.isEmpty()) {
                        for (String[] hs : pendingHashes) {
                            String md5 = md5ToLower(hs[0]);
                            String size = hs[1] != null ? hs[1].trim() : null;
                            if (md5 != null && size != null) {
                                Result r = new Result();
                                r.serial = curSerial;
                                r.name = (curName != null ? curName : curSerial);
                                sMd5SizeToResult.put(md5 + "|" + size, r);
                            }
                        }
                    }
                    pendingHashes.clear();
                    try { DebugLog.i("Redump", "Loaded hash map entries: " + sMd5SizeToResult.size()); } catch (Throwable ignored) {}
                } catch (Exception ex) {
                    try { DebugLog.e("Redump", "Failed to load DB: " + ex.getMessage()); } catch (Throwable ignored) {}
                } finally {
                    if (br != null) try { br.close(); } catch (Exception ignored) {}
                }
            }
        }

        static Result lookupByFile(android.content.ContentResolver cr, Uri file) {
            Context ctx = NativeApp.getContext();
            if (ctx == null) return null;
            ensureLoaded(ctx);
            if (sMd5SizeToResult == null || sMd5SizeToResult.isEmpty()) return null;
            try {
                MessageDigest md = MessageDigest.getInstance("MD5");
                long total = 0;
                final int BUF = 1024 * 1024;
                byte[] buf = new byte[BUF];
                boolean isChd = file.toString().toLowerCase().endsWith(".chd");
                long maxRead = isChd ? 16L * 1024 * 1024 : -1;
                try (java.io.InputStream in = CsoUtils.openInputStream(cr, file)) {
                    while (true) {
                        if (maxRead > 0 && total >= maxRead) break;
                        int r = in.read(buf);
                        if (r <= 0) break;
                        md.update(buf, 0, r);
                        total += r;
                    }
                    if (isChd) {
                        try (android.content.res.AssetFileDescriptor afd = cr.openAssetFileDescriptor(file, "r")) {
                            if (afd != null) total = afd.getLength();
                        }
                    }
                }
                String md5 = new java.math.BigInteger(1, md.digest()).toString(16);
                while (md5.length() < 32) md5 = "0" + md5;
                String key = md5ToLower(md5) + "|" + Long.toString(total);
                return sMd5SizeToResult.get(key);
            } catch (Exception ignored) {
                return null;
            }
        }
    }

    private android.graphics.Bitmap loadHeaderBitmapFromAssets() {
        try (java.io.InputStream is = getAssets().open("icon.png")) {
            return android.graphics.BitmapFactory.decodeStream(is);
        } catch (Exception ignored) {
            try (java.io.InputStream is2 = getAssets().open("app_icons/icon.png")) {
                return android.graphics.BitmapFactory.decodeStream(is2);
            } catch (Exception ignored2) { return null; }
        }
    }


    private android.graphics.Bitmap loadHeaderBlurBitmapFromAssets() {
        try (java.io.InputStream is = getAssets().open("app_icons/icon-old.png")) {
            return android.graphics.BitmapFactory.decodeStream(is);
        } catch (Exception ignored) {
            try (java.io.InputStream is2 = getAssets().open("app_icons/icon.png")) {
                return android.graphics.BitmapFactory.decodeStream(is2);
            } catch (Exception ignored2) {
                try (java.io.InputStream is3 = getAssets().open("icon.png")) {
                    return android.graphics.BitmapFactory.decodeStream(is3);
                } catch (Exception ignored3) { return null; }
            }
        }
    }

    // Recycler adapter
    static class GamesAdapter extends RecyclerView.Adapter<GamesAdapter.VH> {
        interface OnClick { void onClick(GameEntry e); }
        static class VH extends RecyclerView.ViewHolder {
            final TextView tv;
            final TextView tvRegion;
            final android.widget.ImageView img;
            final TextView tvOverlay;
            VH(View v) {
                super(v);
                this.tv = v.findViewById(R.id.tv_title);
                this.tvRegion = v.findViewById(R.id.tv_region);
                this.img = v.findViewById(R.id.img_cover);
                this.tvOverlay = v.findViewById(R.id.tv_cover_fallback);
            }
        }
        private final List<GameEntry> data;
    private final List<GameEntry> filtered = new ArrayList<>();
        private final OnClick onClick;
        private boolean listMode = false;
        // Lightweight in-memory cache for cover bitmaps
        private static final android.util.LruCache<String, android.graphics.Bitmap> sCoverCache;
        private static final java.util.Set<String> sNegativeCache = java.util.Collections.synchronizedSet(new java.util.HashSet<>());
        private static final java.util.concurrent.ExecutorService sExec = java.util.concurrent.Executors.newFixedThreadPool(3);
        private static final java.util.Map<String, File> sLocalCoverFiles = java.util.Collections.synchronizedMap(new java.util.HashMap<>());
        private static final java.util.Set<String> sLocalCoverMissing = java.util.Collections.synchronizedSet(new java.util.HashSet<>());
        static {
            int maxMem = (int) (Runtime.getRuntime().maxMemory() / 1024);
            int cacheSize = Math.max(1024 * 8, Math.min(1024 * 64, maxMem / 16)); 
            sCoverCache = new android.util.LruCache<String, android.graphics.Bitmap>(cacheSize) {
                @Override protected int sizeOf(String key, android.graphics.Bitmap value) {
                    return value.getByteCount() / 1024;
                }
            };
        }
        static void clearLocalCoverCache() {
            sLocalCoverFiles.clear();
            sLocalCoverMissing.clear();
        }

        static void registerCachedCover(GameEntry entry, File file) {
            if (entry == null || file == null || !file.exists()) {
                return;
            }
            String key = coverKey(entry);
            if (TextUtils.isEmpty(key)) {
                return;
            }
            sLocalCoverFiles.put(key, file);
            sLocalCoverMissing.remove(key);
        }
    GamesAdapter(List<GameEntry> d, OnClick oc) { data = d; filtered.addAll(d); onClick = oc; setHasStableIds(true); }
        void update(List<GameEntry> d) { clearLocalCoverCache(); data.clear(); data.addAll(d); applyFilter(currentFilter); }
        int getItemCountTotal() { return data.size(); }
        private String currentFilter = "";
        void setFilter(String q) { currentFilter = q == null ? "" : q.trim(); applyFilter(currentFilter); }
        private void applyFilter(String q) {
            filtered.clear();
            if (TextUtils.isEmpty(q)) {
                filtered.addAll(data);
            } else {
                String needle = q.toLowerCase();
                for (GameEntry e : data) {
                    String t = e != null && e.title != null ? e.title.toLowerCase() : "";
                    String s = e != null && e.serial != null ? e.serial.toLowerCase() : "";
                    if (t.contains(needle) || s.contains(needle)) filtered.add(e);
                }
            }
            notifyDataSetChanged();
        }
        void setListMode(boolean list) { this.listMode = list; notifyDataSetChanged(); }
        @Override public int getItemViewType(int position) { return listMode ? 1 : 0; }
        @NonNull @Override public VH onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
            int layout = (viewType == 1) ? R.layout.item_game_list : R.layout.item_game;
            View v = getLayoutInflater(parent).inflate(layout, parent, false);
            return new VH(v);
        }
        @Override public long getItemId(int position) {
            try {
                GameEntry e = data.get(position);
                String key = (e.uri != null ? e.uri.toString() : e.title) + "|" + (e.title != null ? e.title : "");
                return (long) key.hashCode();
            } catch (Throwable ignored) { return position; }
        }
        @Override public void onViewRecycled(@NonNull VH holder) {
            super.onViewRecycled(holder);
            try {
                holder.img.setTag(R.id.tag_request_key, null);
                holder.img.setImageDrawable(null);
            } catch (Throwable ignored) {}
        }
        @Override public void onBindViewHolder(@NonNull VH holder, int position) {
            GameEntry e = filtered.get(position);
            String tpl = ((MainActivity)holder.itemView.getContext()).getCoversUrlTemplate();
            String requestKey = (e.uri != null ? e.uri.toString() : e.title) + "|" +
                    (e.serial != null ? e.serial : "") + "|" +
                    (e.gameTitle != null ? e.gameTitle : "");
            if (requestKey.equals(holder.img.getTag(R.id.tag_request_key))) {
                return;
            }
            holder.img.setTag(R.id.tag_request_key, requestKey);
            boolean loaded = false;
            android.graphics.Bitmap fastCache = sCoverCache.get(requestKey);
            if (fastCache != null) {
                holder.img.setImageBitmap(fastCache);
                loaded = true;
            } else {
                try { holder.img.setImageDrawable(null); } catch (Throwable ignored) {}
            }
            if (holder.tvOverlay != null) holder.tvOverlay.setVisibility(View.GONE);

            if (!loaded) {
                try {
                    String gameKey = gameKeyFromEntry(e);
                    String manual = ((MainActivity)holder.itemView.getContext()).getManualCoverUri(gameKey);
                    if (manual != null && !manual.isEmpty()) {
                        android.net.Uri mu = android.net.Uri.parse(manual);
                        try (java.io.InputStream is = holder.itemView.getContext().getContentResolver().openInputStream(mu)) {
                            android.graphics.Bitmap bmp = android.graphics.BitmapFactory.decodeStream(is);
                            if (bmp != null) {
                                holder.img.setImageBitmap(bmp);
                                sCoverCache.put(requestKey, bmp);
                                loaded = true;
                            }
                        } catch (Throwable ignored) {}
                    }
                } catch (Throwable ignored) {}
            }

            if (!loaded) {
                File cachedLocal = findCachedCoverFile(holder.itemView.getContext(), e);
                if (cachedLocal != null && cachedLocal.exists()) {
                    String localKey = cachedLocal.getAbsolutePath();
                    android.graphics.Bitmap cachedBmp = sCoverCache.get(localKey);
                    if (cachedBmp != null) {
                        holder.img.setImageBitmap(cachedBmp);
                        sCoverCache.put(requestKey, cachedBmp);
                        loaded = true;
                    } else {
                        android.graphics.Bitmap bmp = android.graphics.BitmapFactory.decodeFile(localKey);
                        if (bmp != null) {
                            holder.img.setImageBitmap(bmp);
                            sCoverCache.put(localKey, bmp);
                            sCoverCache.put(requestKey, bmp);
                            loaded = true;
                        }
                    }
                }
            }
            boolean online = MainActivity.hasInternetConnection(holder.itemView.getContext());
            if (!loaded && online && tpl != null && !tpl.isEmpty()) {
                java.util.List<String> urls = MainActivity.buildCoverCandidateUrls(e, tpl);
                for (String u : urls) {
                    if (u == null || u.isEmpty() || u.contains("${")) continue;
                    android.graphics.Bitmap cached = sCoverCache.get(u);
                    if (cached != null) {
                        holder.img.setImageBitmap(cached);
                        sCoverCache.put(requestKey, cached);
                        loaded = true;
                        break;
                    }
                }
                if (!loaded && !urls.isEmpty()) {
                    loadImageWithFallback(holder.img, holder.tvOverlay, holder.itemView.getContext(), e, urls, requestKey);
                }
            }
            holder.img.setVisibility(View.VISIBLE);
            if (listMode) {
                holder.tv.setVisibility(View.VISIBLE);
                holder.tv.setText(e.gameTitle != null ? e.gameTitle : e.title);
                if (holder.tvOverlay != null) holder.tvOverlay.setVisibility(View.GONE);
            } else {
                if (loaded) {
                    if (holder.tvOverlay != null) holder.tvOverlay.setVisibility(View.GONE);
                    holder.tv.setVisibility(View.GONE);
                } else {
                    holder.tv.setVisibility(View.GONE);
                    if (holder.tvOverlay != null) {
                        holder.tvOverlay.setText(e.gameTitle != null ? e.gameTitle : e.title);
                        holder.tvOverlay.setVisibility(View.VISIBLE);
                        holder.tvOverlay.bringToFront();
                    }
                }
            }
            if (holder.tvRegion != null) {
                holder.tvRegion.setText(e.region != null ? e.region : "");
                holder.tvRegion.setVisibility(TextUtils.isEmpty(e.region) ? View.GONE : View.VISIBLE);
            }
            holder.itemView.setOnClickListener(v -> onClick.onClick(e));
            holder.itemView.setOnKeyListener((v, keyCode, event) -> {
                if (event.getAction() != KeyEvent.ACTION_DOWN) return false;
                RecyclerView rv = (RecyclerView) holder.itemView.getParent();
                RecyclerView.LayoutManager lm = rv.getLayoutManager();
                if (!(lm instanceof GridLayoutManager)) return false;
                int span = ((GridLayoutManager) lm).getSpanCount();
                int pos = holder.getAdapterPosition();
                if (pos == RecyclerView.NO_POSITION) return false;
                switch (keyCode) {
                    case KeyEvent.KEYCODE_DPAD_LEFT:
                        if (pos % span > 0) {
                            int target = pos - 1;
                            rv.smoothScrollToPosition(target);
                            rv.post(() -> {
                                RecyclerView.ViewHolder tvh = rv.findViewHolderForAdapterPosition(target);
                                if (tvh != null) tvh.itemView.requestFocus();
                            });
                        }
                        return true;
                    case KeyEvent.KEYCODE_DPAD_RIGHT:
                        if (pos + 1 < getItemCount()) {
                            int target = pos + 1;
                            rv.smoothScrollToPosition(target);
                            rv.post(() -> {
                                RecyclerView.ViewHolder tvh = rv.findViewHolderForAdapterPosition(target);
                                if (tvh != null) tvh.itemView.requestFocus();
                            });
                        }
                        return true;
                    case KeyEvent.KEYCODE_DPAD_UP:
                        if (pos - span >= 0) {
                            int target = pos - span;
                            rv.smoothScrollToPosition(target);
                            rv.post(() -> {
                                RecyclerView.ViewHolder tvh = rv.findViewHolderForAdapterPosition(target);
                                if (tvh != null) tvh.itemView.requestFocus();
                            });
                        }
                        return true;
                    case KeyEvent.KEYCODE_DPAD_DOWN:
                        if (pos + span < getItemCount()) {
                            int target = pos + span;
                            rv.smoothScrollToPosition(target);
                            rv.post(() -> {
                                RecyclerView.ViewHolder tvh = rv.findViewHolderForAdapterPosition(target);
                                if (tvh != null) tvh.itemView.requestFocus();
                            });
                        }
                        return true;
                    case KeyEvent.KEYCODE_BUTTON_A:
                    case KeyEvent.KEYCODE_BUTTON_START:
                    case KeyEvent.KEYCODE_ENTER:
                        v.performClick();
                        return true;
                }
                return false;
            });
            holder.itemView.setOnLongClickListener(v -> {
                try { ((MainActivity)holder.itemView.getContext()).showGameOptionsPopup(v, e); } catch (Throwable ignored) {}
                return true;
            });
        }
    @Override public int getItemCount() { return filtered.size(); }
        private static android.view.LayoutInflater getLayoutInflater(ViewGroup parent) {
            return android.view.LayoutInflater.from(parent.getContext());
        }

        private void loadImageWithFallback(android.widget.ImageView iv, TextView overlayView, Context ctx, GameEntry entry, java.util.List<String> urls, String requestKey) {
            try {
                sExec.execute(() -> {
                    try {
                        android.graphics.Bitmap bmp = null;
                        String hitUrl = null;
                        byte[] downloadedBytes = null;
                        String downloadExtension = null;
                        for (String ustr : urls) {
                            if (ustr == null || ustr.isEmpty() || ustr.contains("${")) continue;
                            Object tag = iv.getTag(R.id.tag_request_key);
                            if (!(requestKey.equals(tag))) { break; }
                            if (sNegativeCache.contains(ustr)) continue;
                            android.graphics.Bitmap cached = sCoverCache.get(ustr);
                            if (cached != null) { bmp = cached; hitUrl = ustr; break; }
                            try {
                                java.net.HttpURLConnection c = (java.net.HttpURLConnection) new java.net.URL(ustr).openConnection();
                                c.setConnectTimeout(4000); c.setReadTimeout(6000);
                                c.setInstanceFollowRedirects(true);
                                c.setRequestMethod("GET");
                                int code = c.getResponseCode();
                                if (code == 200) {
                                    try (java.io.InputStream is = c.getInputStream();
                                         java.io.ByteArrayOutputStream baos = new java.io.ByteArrayOutputStream()) {
                                        byte[] buffer = new byte[8192];
                                        int read;
                                        while ((read = is.read(buffer)) != -1) {
                                            baos.write(buffer, 0, read);
                                        }
                                        byte[] data = baos.toByteArray();
                                        if (data.length > 0) {
                                            android.graphics.Bitmap candidate = android.graphics.BitmapFactory.decodeByteArray(data, 0, data.length);
                                            if (candidate != null) {
                                                bmp = candidate;
                                                downloadedBytes = data;
                                                downloadExtension = guessImageExtension(ustr, c.getContentType());
                                                hitUrl = ustr;
                                                break;
                                            }
                                        }
                                    }
                                } else if (code == 404) {
                                    sNegativeCache.add(ustr);
                                    continue; 
                                } else {
                                    try { DebugLog.d("Covers", "HTTP " + code + " for " + ustr); } catch (Throwable ignored) {}
                                }
                            } catch (Exception ex) {
                                try { DebugLog.d("Covers", "Error loading cover: " + ex.getMessage()); } catch (Throwable ignored) {}
                            }
                        }
                        if (downloadedBytes != null && downloadedBytes.length > 0 && entry != null && ctx != null) {
                            try { storeCoverBytes(ctx, entry, downloadedBytes, downloadExtension); } catch (Throwable ignored) {}
                        }
                        final android.graphics.Bitmap fb = bmp;
                        final String fUrl = hitUrl;
                        iv.post(() -> {
                            Object tagNow = iv.getTag(R.id.tag_request_key);
                            if (requestKey.equals(tagNow) && fb != null) {
                                iv.setImageBitmap(fb);
                                if (fUrl != null) sCoverCache.put(fUrl, fb);
                                if (overlayView != null) overlayView.setVisibility(View.GONE);
                            }
                        });
                    } catch (Throwable ignored) {}
                });
            } catch (Throwable ignored) {}
        }

        private File findCachedCoverFile(Context ctx, GameEntry entry) {
            if (ctx == null || entry == null || entry.uri == null) {
                return null;
            }
            String key = coverKey(entry);
            if (TextUtils.isEmpty(key)) {
                return null;
            }
            File cached = sLocalCoverFiles.get(key);
            if (cached != null && cached.exists()) {
                return cached;
            }
            if (sLocalCoverMissing.contains(key)) {
                return null;
            }
            File cacheDir = MainActivity.getCoversCacheDir(ctx);
            if (cacheDir == null) {
                sLocalCoverMissing.add(key);
                return null;
            }
            String baseName = computeCoverBaseName(entry);
            File coverFile = MainActivity.findExistingCoverFile(cacheDir, baseName);
            if (coverFile != null && coverFile.isFile() && coverFile.length() > 0) {
                sLocalCoverFiles.put(key, coverFile);
                sLocalCoverMissing.remove(key);
                return coverFile;
            }
            sLocalCoverMissing.add(key);
            return null;
        }

        private static void storeCoverBytes(Context ctx, GameEntry entry, byte[] data, String extension) {
            if (ctx == null || entry == null || data == null || data.length == 0) {
                return;
            }
            File cacheDir = MainActivity.getCoversCacheDir(ctx);
            if (cacheDir == null) {
                return;
            }
            String baseName = computeCoverBaseName(entry);
            if (TextUtils.isEmpty(baseName)) {
                return;
            }
            String ext = extension;
            if (TextUtils.isEmpty(ext)) {
                ext = ".jpg";
            }
            if (!ext.startsWith(".")) {
                ext = "." + ext;
            }
            File target = new File(cacheDir, baseName + ext);
            File parent = target.getParentFile();
            if (parent != null && !parent.exists() && !parent.mkdirs()) {
                return;
            }
            File temp = new File(cacheDir, baseName + "_tmp" + ext);
            try (FileOutputStream fos = new FileOutputStream(temp)) {
                fos.write(data);
                fos.flush();
            } catch (IOException ignored) {
                temp.delete();
                return;
            }
            if (!temp.renameTo(target)) {
                temp.delete();
                return;
            }
            GamesAdapter.registerCachedCover(entry, target);
            try { DebugLog.d("Covers", "Stored cover cache file: " + target.getAbsolutePath()); } catch (Throwable ignored) {}
        }

        private static String coverKey(GameEntry entry) {
            if (entry == null) {
                return null;
            }
            if (entry.uri != null) {
                return entry.uri.toString();
            }
            String fallback = entry.title;
            if (TextUtils.isEmpty(fallback)) {
                fallback = entry.fileTitleNoExt();
            }
            return fallback;
        }

    }

}
