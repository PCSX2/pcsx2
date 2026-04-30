/*

may the android, pcsx2 and the java gods bless me with material you support for this, amen.

*/

package kr.co.iefriends.pcsx2.activities;

import android.app.Activity;
import android.content.Context;
import android.graphics.Bitmap;
import android.content.DialogInterface;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.provider.DocumentsContract;
import android.provider.Settings;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.BaseAdapter;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.ListView;
import android.widget.ProgressBar;
import android.widget.SeekBar;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;
import android.widget.ViewFlipper;
import android.view.animation.AnimationUtils;

import com.google.android.material.appbar.MaterialToolbar;
import com.google.android.material.button.MaterialButton;
import com.google.android.material.button.MaterialButtonToggleGroup;
import com.google.android.material.imageview.ShapeableImageView;
import com.google.android.material.materialswitch.MaterialSwitch;
import com.google.android.material.slider.Slider;
import com.google.android.material.tabs.TabLayout;
import com.google.android.material.dialog.MaterialAlertDialogBuilder;
import com.google.android.material.textfield.TextInputEditText;

import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Enumeration;

import kr.co.iefriends.pcsx2.input.ControllerMappingDialog;
import kr.co.iefriends.pcsx2.BuildConfig;
import kr.co.iefriends.pcsx2.provider.Armsx2DocumentsProvider;
import kr.co.iefriends.pcsx2.utils.AppIconManager;
import kr.co.iefriends.pcsx2.utils.DataDirectoryManager;
import kr.co.iefriends.pcsx2.utils.DiscordBridge;
import kr.co.iefriends.pcsx2.utils.LogcatRecorder;
import kr.co.iefriends.pcsx2.NativeApp;
import kr.co.iefriends.pcsx2.R;
import kr.co.iefriends.pcsx2.utils.RetroAchievementsBridge;
import kr.co.iefriends.pcsx2.input.ControllerMappingManager;
import kr.co.iefriends.pcsx2.utils.DeviceProfiles;
import kr.co.iefriends.pcsx2.utils.AvatarLoader;

import java.net.NetworkInterface;

public class SettingsActivity extends AppCompatActivity {

    private boolean mIgnoreRendererInit = true;
    private static final int REQ_IMPORT_MEMCARD = 9911;
    private static final int SECTION_GENERAL = 0;
	private static final int SECTION_GRAPHICS = 1;
	private static final int SECTION_PERFORMANCE = 2;
	private static final int SECTION_STATS = 3;
	private static final int SECTION_CONTROLLER = 4;
	private static final int SECTION_CUSTOMIZATION = 5;
	private static final int SECTION_STORAGE = 6;
    private static final int SECTION_MEMORY = 7;
	private static final int SECTION_ACHIEVEMENTS = 8;
	private static final String PREF_GPU_PROFILE_OVERRIDE_FALLBACK = "gpu_profile_override_fallback";
	private static final String STATE_SELECTED_SECTION = "settings_selected_section";
	private static final String EXTRA_SHOW_ADVANCED = "android.provider.extra.SHOW_ADVANCED";
	private static final String ACTION_BROWSE_DOCUMENT_ROOT = "android.provider.action.BROWSE_DOCUMENT_ROOT";
	private TextView tvDataDirPath;
	private AlertDialog dataDirProgressDialog;
    private boolean disableTouchControls;
    private MaterialToolbar toolbar;
    private ViewFlipper sectionFlipper;
    private MaterialButtonToggleGroup sectionToggleGroup;
    private TabLayout sectionTabs;
    private int currentSection = SECTION_GENERAL;
    private boolean suppressNavigationCallbacks;
	private final Intent pendingSettingsResult = new Intent();
    private TextView tvDiscordStatus;
	private MaterialButton btnDiscordConnect;
	private View groupDiscordIdentity;
	private ShapeableImageView imgDiscordAvatar;
	private TextView tvDiscordLoggedInAs;
	private TextView tvOnScreenUiStyleValue;
	private TextView tvAppIconValue;
	private MaterialButton btnDiscordLogout;
    private MaterialSwitch switchRaEnabled;
    private MaterialSwitch switchRaHardcore;
    private MaterialButton btnRaLogin;
    private MaterialButton btnRaLogout;
    private TextView tvRaStatus;
    private TextView tvRaProfile;
    private TextView tvRaGame;
	private View groupRaIdentity;
	private ShapeableImageView imgRaAvatar;
	private TextView tvRaLoggedInAs;
    private boolean updatingRaUi;

	private final ActivityResultLauncher<Intent> startActivityResultPickDataDir =
		registerForActivityResult(new ActivityResultContracts.StartActivityForResult(), result -> {
			if (result.getResultCode() == Activity.RESULT_OK && result.getData() != null) {
				Intent data = result.getData();
				Uri tree = data.getData();
				if (tree != null) {
					final int takeFlags = data.getFlags() & (Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
					try {
						getContentResolver().takePersistableUriPermission(tree, takeFlags);
					} catch (SecurityException ignored) {}
					handleDataDirectorySelection(tree);
				}
			}
		});

	@Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_settings_new);
        LogcatRecorder.initialize(getApplicationContext());
        disableTouchControls = DeviceProfiles.isTvOrDesktop(this);
	DiscordBridge.updateEngineActivity(this);

        String displayName = DeviceProfiles.getProductDisplayName(this, getString(R.string.app_name));
        toolbar = findViewById(R.id.settings_toolbar);
        if (toolbar != null) {
            toolbar.setTitle(getString(R.string.settings_toolbar_title, displayName));
            toolbar.setSubtitle(getString(R.string.settings_section_general));
            setSupportActionBar(toolbar);
            if (getSupportActionBar() != null) {
                getSupportActionBar().setDisplayHomeAsUpEnabled(true);
            }
            toolbar.setNavigationOnClickListener(v -> finish());
        } else {
            setTitle(getString(R.string.settings_toolbar_title, displayName));
        }

		initializeGeneralSettings();
		initializeGraphicsSettings();
		initializeControllerSettings();
		initializePerformanceSettings();
		initializeStatsSettings();
		initializeCustomizationSettings();
		initializeMemoryCardSettings();
		initializeStorageSettings();
		initializeActionButtons();
		initializeAchievementsSettings();

        int initialSection = savedInstanceState != null
                ? savedInstanceState.getInt(STATE_SELECTED_SECTION, SECTION_GENERAL)
                : SECTION_GENERAL;
        setupSectionNavigation(initialSection);
    }

    @Override
    protected void onDestroy() {
        DiscordBridge.setListener(null);
        RetroAchievementsBridge.setListener(null);
        super.onDestroy();
    }

	@Override
	protected void onResume() {
		super.onResume();
		DiscordBridge.updateEngineActivity(this);
        updateDataDirSummary();
        updateOnScreenUiStyleSummary();
		updateAppIconSummary();
		AppIconManager.applyTaskDescription(this);
        updateDiscordUi(DiscordBridge.isLoggedIn());
        RetroAchievementsBridge.refreshState();
    }

	private void markSettingsResultChanged() {
		setResult(RESULT_OK, pendingSettingsResult);
	}

	private void markLayoutNeedsRefresh() {
		pendingSettingsResult.putExtra(MainActivity.EXTRA_SETTINGS_LAYOUT_CHANGED, true);
		markSettingsResultChanged();
	}

	private static String normalizeGpuProfileOverride(@Nullable String value) {
		if ("mali".equalsIgnoreCase(value)) {
			return "mali";
		}
		if ("adreno".equalsIgnoreCase(value)) {
			return "adreno";
		}
		return "auto";
	}

	private static int gpuProfileSelectionForValue(@Nullable String profile) {
		String normalized = normalizeGpuProfileOverride(profile);
		if ("mali".equals(normalized)) {
			return 1;
		}
		if ("adreno".equals(normalized)) {
			return 2;
		}
		return 0;
	}

	private static String gpuProfileValueForSelection(int position) {
		switch (position) {
			case 1:
				return "mali";
			case 2:
				return "adreno";
			default:
				return "auto";
		}
	}

    private final RetroAchievementsBridge.Listener retroAchievementsListener = new RetroAchievementsBridge.Listener() {
        @Override
        public void onStateUpdated(RetroAchievementsBridge.State state) {
            updateRetroAchievementsUi(state);
        }

        @Override
        public void onLoginRequested(int reason) {
            runOnUiThread(() -> handleRetroAchievementsLoginRequest(reason));
        }

        @Override
        public void onLoginSuccess(String username, int points, int softPoints, int unreadMessages) {
			runOnUiThread(() -> showRetroAchievementsLoginToast(username));
        }

        @Override
        public void onHardcoreModeChanged(boolean enabled) {
            runOnUiThread(() -> {
                if (switchRaHardcore != null && !updatingRaUi) {
                    switchRaHardcore.setChecked(enabled);
                }
            });
        }
    };

    private void initializeAchievementsSettings() {
        switchRaEnabled = findViewById(R.id.switch_retroachievements_enabled);
        switchRaHardcore = findViewById(R.id.switch_retroachievements_hardcore);
        btnRaLogin = findViewById(R.id.btn_connect_retroachievements);
        btnRaLogout = findViewById(R.id.btn_logout_retroachievements);
        tvRaStatus = findViewById(R.id.tv_ra_status);
        tvRaProfile = findViewById(R.id.tv_ra_profile);
        tvRaGame = findViewById(R.id.tv_ra_game);
	    groupRaIdentity = findViewById(R.id.group_ra_identity);
	    imgRaAvatar = findViewById(R.id.img_ra_avatar);
	    tvRaLoggedInAs = findViewById(R.id.tv_ra_logged_in_as);

        if (switchRaEnabled != null) {
            switchRaEnabled.setOnCheckedChangeListener((buttonView, isChecked) -> {
                if (updatingRaUi) {
                    return;
                }
                RetroAchievementsBridge.setEnabled(isChecked);
            });
        }

        if (switchRaHardcore != null) {
            switchRaHardcore.setOnCheckedChangeListener((buttonView, isChecked) -> {
                if (updatingRaUi) {
                    return;
                }
                RetroAchievementsBridge.setHardcore(isChecked);
            });
        }

        if (btnRaLogin != null) {
            btnRaLogin.setOnClickListener(v -> {
                if (switchRaEnabled != null && !switchRaEnabled.isChecked()) {
                    try {
                        Toast.makeText(this, R.string.settings_achievements_login_required, Toast.LENGTH_SHORT).show();
                    } catch (Throwable ignored) {}
                    return;
                }
                showRetroAchievementsLoginDialog();
            });
        }

        if (btnRaLogout != null) {
            btnRaLogout.setOnClickListener(v -> showRetroAchievementsLogoutDialog());
        }

        RetroAchievementsBridge.setListener(retroAchievementsListener);
        RetroAchievementsBridge.refreshState();

	    tvDiscordStatus = findViewById(R.id.tv_discord_status);
	    groupDiscordIdentity = findViewById(R.id.group_discord_identity);
	    imgDiscordAvatar = findViewById(R.id.img_discord_avatar);
	    tvDiscordLoggedInAs = findViewById(R.id.tv_discord_logged_in_as);
	    btnDiscordConnect = findViewById(R.id.btn_discord_connect);
	    btnDiscordLogout = findViewById(R.id.btn_discord_logout);

	    boolean discordAvailable = DiscordBridge.isAvailable();
	    if (!discordAvailable) {
	    	if (tvDiscordStatus != null) {
		    	tvDiscordStatus.setText(R.string.settings_discord_unavailable_status);
		    }
		    if (btnDiscordConnect != null) {
			    btnDiscordConnect.setEnabled(false);
			    btnDiscordConnect.setText(R.string.settings_discord_unavailable_button);
		    }
            if (btnDiscordLogout != null) {
                btnDiscordLogout.setVisibility(View.GONE);
		    }
		    if (groupDiscordIdentity != null) {
                groupDiscordIdentity.setVisibility(View.GONE);
		    }
		    DiscordBridge.setListener(null);
		    return;
	    }
        if (btnDiscordConnect != null) {
            updateDiscordUi(DiscordBridge.isLoggedIn());
            btnDiscordConnect.setOnClickListener(v -> {
                if (!DiscordBridge.isLoggedIn()) {
                    DiscordBridge.beginAuthorize(this);
                }
            });
        }
        if (btnDiscordLogout != null) {
            btnDiscordLogout.setOnClickListener(v -> {
                DiscordBridge.clearTokens();
                updateDiscordUi(false);
                try {
                    Toast.makeText(this, R.string.settings_discord_disconnected, Toast.LENGTH_SHORT).show();
                } catch (Throwable ignored) {}
            });
        }

        DiscordBridge.setListener(new DiscordBridge.DiscordStateListener() {
            @Override
            public void onLoginStateChanged(boolean loggedIn) {
                runOnUiThread(() -> updateDiscordUi(loggedIn));
            }

            @Override
            public void onError(String message) {
                runOnUiThread(() -> {
                    if (!TextUtils.isEmpty(message)) {
                        try {
                            Toast.makeText(SettingsActivity.this, message, Toast.LENGTH_LONG).show();
                        } catch (Throwable ignored) {}
                    }
                });
            }

            @Override
            public void onUserInfoUpdated(String username) {
                runOnUiThread(() -> updateDiscordUi(DiscordBridge.isLoggedIn()));
            }
        });
    }

	private void updateDiscordUi(boolean loggedIn) {
        if (tvDiscordStatus == null || btnDiscordConnect == null) {
            return;
        }
		if (!DiscordBridge.isAvailable()) {
			tvDiscordStatus.setText(R.string.settings_discord_unavailable_status);
			btnDiscordConnect.setEnabled(false);
			btnDiscordConnect.setText(R.string.settings_discord_unavailable_button);
			if (tvDiscordLoggedInAs != null) {
				tvDiscordLoggedInAs.setVisibility(View.GONE);
			}
			if (groupDiscordIdentity != null) {
				groupDiscordIdentity.setVisibility(View.GONE);
			}
			if (imgDiscordAvatar != null) {
				AvatarLoader.clear(imgDiscordAvatar);
			}
			if (btnDiscordLogout != null) {
				btnDiscordLogout.setVisibility(View.GONE);
			}
			return;
		}
        if (loggedIn) {
            tvDiscordStatus.setText(R.string.settings_discord_status_active);
            btnDiscordConnect.setEnabled(false);
            btnDiscordConnect.setText(R.string.settings_discord_connect);
            if (tvDiscordLoggedInAs != null) {
                String username = DiscordBridge.getLoggedInUsername();
                if (TextUtils.isEmpty(username)) {
                    username = getString(R.string.settings_discord_unknown_user);
                }
				tvDiscordLoggedInAs.setText(getString(R.string.settings_identity_logged_in_as, username));
				tvDiscordLoggedInAs.setVisibility(View.VISIBLE);
            }
			if (groupDiscordIdentity != null) {
				groupDiscordIdentity.setVisibility(View.VISIBLE);
			}
			if (imgDiscordAvatar != null) {
				String avatarUrl = DiscordBridge.getLoggedInAvatarUrl();
				if (!TextUtils.isEmpty(avatarUrl)) {
					AvatarLoader.loadRemote(imgDiscordAvatar, avatarUrl);
				} else {
					AvatarLoader.clear(imgDiscordAvatar);
				}
			}
            if (btnDiscordLogout != null) {
                btnDiscordLogout.setVisibility(View.VISIBLE);
                btnDiscordLogout.setEnabled(true);
            }
        } else {
            tvDiscordStatus.setText(R.string.settings_discord_status_connect_activity);
            btnDiscordConnect.setEnabled(true);
            btnDiscordConnect.setText(R.string.settings_discord_connect);
            if (tvDiscordLoggedInAs != null) {
                tvDiscordLoggedInAs.setVisibility(View.GONE);
            }
			if (groupDiscordIdentity != null) {
				groupDiscordIdentity.setVisibility(View.GONE);
			}
			if (imgDiscordAvatar != null) {
				AvatarLoader.clear(imgDiscordAvatar);
			}
            if (btnDiscordLogout != null) {
                btnDiscordLogout.setVisibility(View.GONE);
            }
        }

        String pendingError = DiscordBridge.consumeLastError();
        if (!TextUtils.isEmpty(pendingError)) {
            try {
                Toast.makeText(this, pendingError, Toast.LENGTH_LONG).show();
            } catch (Throwable ignored) {}
        }
    }

    private void updateRetroAchievementsUi(RetroAchievementsBridge.State state) {
        if (state == null) {
            return;
        }

        updatingRaUi = true;

        if (switchRaEnabled != null) {
            switchRaEnabled.setEnabled(true);
            switchRaEnabled.setChecked(state.achievementsEnabled);
        }

        if (btnRaLogin != null) {
            btnRaLogin.setEnabled(state.achievementsEnabled);
            btnRaLogin.setText(state.loggedIn
                    ? R.string.settings_achievements_button_change
                    : R.string.settings_achievements_button);
        }

        if (btnRaLogout != null) {
            btnRaLogout.setVisibility(state.loggedIn ? View.VISIBLE : View.GONE);
            btnRaLogout.setEnabled(state.loggedIn);
        }

        if (switchRaHardcore != null) {
            switchRaHardcore.setEnabled(state.achievementsEnabled && state.loggedIn);
            switchRaHardcore.setChecked(state.hardcorePreference);
        }

		if (tvRaStatus != null) {
			String status;
			if (!state.achievementsEnabled) {
				status = getString(R.string.settings_achievements_status_signed_out);
			} else if (state.loggedIn) {
				status = getString(R.string.settings_achievements_status_connected, state.displayName);
				if (state.hardcorePreference && !state.hardcoreActive) {
					status = status + "\n" + getString(R.string.settings_achievements_hardcore_inactive);
				}
			} else {
				status = getString(R.string.settings_achievements_status_signed_out);
			}
			tvRaStatus.setText(status);
		}

		if (groupRaIdentity != null) {
			boolean showIdentity = state.achievementsEnabled && state.loggedIn;
			groupRaIdentity.setVisibility(showIdentity ? View.VISIBLE : View.GONE);
			if (showIdentity) {
				if (tvRaLoggedInAs != null) {
					String name = !TextUtils.isEmpty(state.displayName) ? state.displayName : state.username;
					if (TextUtils.isEmpty(name)) {
						name = getString(R.string.settings_achievements_title);
					}
					tvRaLoggedInAs.setText(getString(R.string.settings_identity_logged_in_as, name));
				}
				if (imgRaAvatar != null) {
					AvatarLoader.loadLocal(imgRaAvatar, state.avatarPath);
				}
			} else {
				if (tvRaLoggedInAs != null) {
					tvRaLoggedInAs.setText(R.string.settings_achievements_status_signed_out);
				}
				if (imgRaAvatar != null) {
					AvatarLoader.clear(imgRaAvatar);
				}
			}
		}

        if (tvRaProfile != null) {
            tvRaProfile.setText(state.loggedIn
                    ? getString(R.string.settings_achievements_profile_fmt, state.points, state.softcorePoints, state.unreadMessages)
                    : getString(R.string.settings_achievements_profile_signed_out));
        }

        if (tvRaGame != null) {
            if (state.hasActiveGame && !TextUtils.isEmpty(state.gameTitle)) {
                tvRaGame.setText(getString(
                        R.string.settings_achievements_game_fmt,
                        state.gameTitle,
                        state.unlockedAchievements,
                        state.totalAchievements,
                        state.unlockedPoints,
                        state.totalPoints));
            } else {
                tvRaGame.setText(R.string.settings_achievements_no_game);
            }
        }

        updatingRaUi = false;
    }

	private void showRetroAchievementsLoginToast(String fallbackUsername) {
		try {
			RetroAchievementsBridge.State state = RetroAchievementsBridge.getLastState();
			String displayName = fallbackUsername;
			String avatarPath = null;
			if (state != null) {
				if (!TextUtils.isEmpty(state.displayName)) {
					displayName = state.displayName;
				} else if (!TextUtils.isEmpty(state.username)) {
					displayName = state.username;
				}
				if (!TextUtils.isEmpty(state.avatarPath)) {
					avatarPath = state.avatarPath;
				}
			}

			if (TextUtils.isEmpty(displayName)) {
				displayName = getString(R.string.settings_achievements_title);
			}

			View toastView = LayoutInflater.from(this).inflate(R.layout.toast_ra_login, null);
			ShapeableImageView avatarView = toastView.findViewById(R.id.img_toast_ra_avatar);
			TextView messageView = toastView.findViewById(R.id.tv_toast_ra_message);
			messageView.setText(getString(R.string.settings_achievements_login_success_fmt, displayName));
			AvatarLoader.loadLocal(avatarView, avatarPath);

			Toast toast = new Toast(getApplicationContext());
			toast.setDuration(Toast.LENGTH_SHORT);
			toast.setView(toastView);
			toast.show();
		} catch (Throwable ignored) {
			try {
				Toast.makeText(this, getString(R.string.settings_achievements_login_success), Toast.LENGTH_SHORT).show();
			} catch (Throwable ignored2) {}
		}
	}

    private void handleRetroAchievementsLoginRequest(int reason) {
        if (isFinishing()) {
            return;
        }

        showRetroAchievementsLoginDialog();
        if (reason == RetroAchievementsBridge.LOGIN_REASON_TOKEN_INVALID) {
            try {
                Toast.makeText(this, R.string.settings_achievements_token_invalid, Toast.LENGTH_LONG).show();
            } catch (Throwable ignored) {}
        }
    }

    private void showRetroAchievementsLoginDialog() {
        View dialogView = LayoutInflater.from(this).inflate(R.layout.dialog_retroachievements_login, null);
        TextInputEditText etUsername = dialogView.findViewById(R.id.et_ra_username);
        TextInputEditText etPassword = dialogView.findViewById(R.id.et_ra_password);

        RetroAchievementsBridge.State state = RetroAchievementsBridge.getLastState();
        if (state != null && !TextUtils.isEmpty(state.username) && etUsername != null) {
            etUsername.setText(state.username);
        }

        AlertDialog dialog = new MaterialAlertDialogBuilder(this)
                .setTitle(R.string.settings_achievements_login_title)
                .setView(dialogView)
                .setPositiveButton(R.string.settings_achievements_button, null)
                .setNegativeButton(android.R.string.cancel, (d, which) -> d.dismiss())
                .create();

        dialog.setOnShowListener(dlg -> {
            MaterialButton positive = dialog.getButton(DialogInterface.BUTTON_POSITIVE) instanceof MaterialButton
                    ? (MaterialButton) dialog.getButton(DialogInterface.BUTTON_POSITIVE)
                    : null;
            View.OnClickListener loginClickListener = v -> {
                String user = etUsername != null && etUsername.getText() != null
                        ? etUsername.getText().toString().trim()
                        : "";
                String pass = etPassword != null && etPassword.getText() != null
                        ? etPassword.getText().toString()
                        : "";
                if (TextUtils.isEmpty(user) || TextUtils.isEmpty(pass)) {
                    try {
                        Toast.makeText(this, R.string.settings_achievements_error_credentials, Toast.LENGTH_SHORT).show();
                    } catch (Throwable ignored) {}
                    return;
                }
                dialog.dismiss();
                beginRetroAchievementsLogin(user, pass);
            };

            if (positive != null) {
                positive.setOnClickListener(loginClickListener);
            } else {
                dialog.getButton(DialogInterface.BUTTON_POSITIVE).setOnClickListener(loginClickListener);
            }
        });

        dialog.show();
    }

    private void beginRetroAchievementsLogin(String username, String password) {
        AlertDialog progressDialog = new MaterialAlertDialogBuilder(this)
                .setTitle(R.string.settings_achievements_logging_in)
                .setMessage(R.string.settings_achievements_logging_in_message)
                .setCancelable(false)
                .create();
        progressDialog.show();

        RetroAchievementsBridge.login(username, password, (success, message) -> {
            progressDialog.dismiss();
            if (!success && !TextUtils.isEmpty(message)) {
                try {
                    new MaterialAlertDialogBuilder(this)
                            .setTitle(R.string.settings_achievements_login_title)
                            .setMessage(message)
                            .setPositiveButton(android.R.string.ok, null)
                            .show();
                } catch (Throwable ignored) {}
            }
        });
    }

    private void showRetroAchievementsLogoutDialog() {
        new MaterialAlertDialogBuilder(this)
                .setTitle(R.string.settings_achievements_logout_title)
                .setMessage(R.string.settings_achievements_logout_message)
                .setPositiveButton(R.string.settings_achievements_logout, (dialog, which) -> {
                    RetroAchievementsBridge.logout();
                    try {
                        Toast.makeText(this, R.string.settings_achievements_logout_success, Toast.LENGTH_SHORT).show();
                    } catch (Throwable ignored) {}
                })
                .setNegativeButton(android.R.string.cancel, null)
                .show();
    }

	private void initializeGeneralSettings() {
		MaterialSwitch swFsui = findViewById(R.id.sw_fsui);
		if (swFsui != null) {
			try {
				String fsui = NativeApp.getSetting("UI", "EnableFullscreenUI", "bool");
				swFsui.setChecked("true".equalsIgnoreCase(fsui));
			} catch (Exception ignored) {}
			swFsui.setOnCheckedChangeListener((buttonView, isChecked) -> {
				NativeApp.setSetting("UI", "EnableFullscreenUI", "bool", isChecked ? "true" : "false");
				markLayoutNeedsRefresh();
			});
		}

		MaterialSwitch swExpandCutout = findViewById(R.id.sw_expand_cutout);
		if (swExpandCutout != null) {
			try {
				String expand = NativeApp.getSetting("UI", "ExpandIntoDisplayCutout", "bool");
				swExpandCutout.setChecked("true".equalsIgnoreCase(expand));
			} catch (Exception ignored) {
				swExpandCutout.setChecked(true);
			}
			swExpandCutout.setOnCheckedChangeListener((buttonView, isChecked) -> {
				NativeApp.setSetting("UI", "ExpandIntoDisplayCutout", "bool", isChecked ? "true" : "false");
				markLayoutNeedsRefresh();
			});
		}

		Spinner spAspectRatio = findViewById(R.id.sp_aspect_ratio);
		ArrayAdapter<CharSequence> aspectAdapter = ArrayAdapter.createFromResource(this, R.array.aspect_ratios, android.R.layout.simple_spinner_item);
		aspectAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
		spAspectRatio.setAdapter(aspectAdapter);
		final String[] aspectChoices = getResources().getStringArray(R.array.aspect_ratios);
		try {
			String aspect = NativeApp.getSetting("EmuCore/GS", "AspectRatio", "string");
			int pos = 0;
			if (aspect != null && !aspect.isEmpty()) {
				for (int i = 0; i < aspectChoices.length; i++) {
					if (aspect.equalsIgnoreCase(aspectChoices[i])) {
						pos = i;
						break;
					}
				}
			}
			spAspectRatio.setSelection(Math.max(0, Math.min(aspectAdapter.getCount() - 1, pos)), false);
		} catch (Exception ignored) {}
		spAspectRatio.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
			@Override public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
				if (position < 0 || position >= aspectChoices.length)
					return;
				String value = aspectChoices[position];
				NativeApp.setSetting("EmuCore/GS", "AspectRatio", "string", value);
				NativeApp.setAspectRatio(position);
			}
			@Override public void onNothingSelected(AdapterView<?> parent) {}
		});

		MaterialSwitch swFastBoot = findViewById(R.id.sw_fast_boot);
		if (swFastBoot != null) {
			try {
				String fast = NativeApp.getSetting("EmuCore", "EnableFastBoot", "bool");
				swFastBoot.setChecked("true".equalsIgnoreCase(fast) || fast == null || fast.isEmpty());
			} catch (Exception ignored) {}
			swFastBoot.setOnCheckedChangeListener((b, isChecked) ->
					NativeApp.setSetting("EmuCore", "EnableFastBoot", "bool", isChecked ? "true" : "false"));
		}

        MaterialSwitch swWarnUnsafe = findViewById(R.id.sw_warn_unsafe_settings);
        if (swWarnUnsafe != null) {
            try {
                String warn = NativeApp.getSetting("EmuCore", "WarnAboutUnsafeSettings", "bool");
                swWarnUnsafe.setChecked(!"false".equalsIgnoreCase(warn));
            } catch (Exception ignored) {}
            swWarnUnsafe.setOnCheckedChangeListener((buttonView, isChecked) -> {
                NativeApp.setSetting("EmuCore", "WarnAboutUnsafeSettings", "bool", isChecked ? "true" : "false");
            });
        }

		MaterialSwitch swRecordLogs = findViewById(R.id.sw_record_logs);
		if (swRecordLogs != null) {
			boolean recordLogs = false;
			try {
				String current = NativeApp.getSetting("Logging", "RecordAndroidLog", "bool");
				recordLogs = "true".equalsIgnoreCase(current);
			} catch (Exception ignored) {}
			swRecordLogs.setChecked(recordLogs);
			LogcatRecorder.setEnabled(recordLogs);
			swRecordLogs.setOnCheckedChangeListener((buttonView, isChecked) -> {
				NativeApp.setSetting("Logging", "RecordAndroidLog", "bool", isChecked ? "true" : "false");
				LogcatRecorder.setEnabled(isChecked);
			});
		}

		Slider sbBrightness = findViewById(R.id.sb_brightness);
		TextView tvBrightness = findViewById(R.id.tv_brightness_value);
		if (sbBrightness != null && tvBrightness != null) {
			try {
				String br = NativeApp.getSetting("EmuCore/GS", "BrightnessScale", "float");
				float val = (br == null || br.isEmpty()) ? 1.0f : Float.parseFloat(br);
				int prog = Math.round(val * 100f);
				prog = Math.max(0, Math.min(200, prog));
				sbBrightness.setValue(prog);
				tvBrightness.setText(getString(R.string.settings_brightness_value, val));
			} catch (Exception ignored) {
				sbBrightness.setValue(100f);
				tvBrightness.setText(R.string.settings_brightness_default);
			}
			sbBrightness.addOnChangeListener((slider, value, fromUser) -> {
				int clamped = Math.max(0, Math.min(200, Math.round(value)));
				if (clamped != Math.round(value)) slider.setValue(clamped);
				float scale = clamped / 100f;
				tvBrightness.setText(getString(R.string.settings_brightness_value, scale));
				NativeApp.setSetting("EmuCore/GS", "BrightnessScale", "float", Float.toString(scale));
			});
		}

		Slider sbOsc = findViewById(R.id.sb_osc_timeout);
		TextView tvOsc = findViewById(R.id.tv_osc_timeout_value);
		MaterialSwitch swOscNever = findViewById(R.id.sw_osc_never);
		View oscGroup = findViewById(R.id.group_osc_settings);
		if (disableTouchControls) {
			if (oscGroup != null) oscGroup.setVisibility(View.GONE);
		} else if (sbOsc != null && tvOsc != null && swOscNever != null) {
			final String prefsName = "armsx2";
			final String prefKey = "onscreen_timeout_seconds";
			android.content.SharedPreferences sp = getSharedPreferences(prefsName, MODE_PRIVATE);
			int cur = 3;
			try { cur = sp.getInt(prefKey, 3); } catch (Throwable ignored) {}
			if (cur < 0) cur = 0;
			if (cur > 60) cur = 60;
			tvOsc.setText(cur == 0
				? getString(R.string.settings_osc_timeout_never)
				: getString(R.string.settings_osc_timeout_seconds, cur));
			sbOsc.setValue(cur == 0 ? 3f : cur);
			swOscNever.setChecked(cur == 0);
			sbOsc.setEnabled(cur != 0);
			swOscNever.setOnCheckedChangeListener((b, checked) -> {
				if (checked) {
					sp.edit().putInt(prefKey, 0).apply();
					tvOsc.setText(R.string.settings_osc_timeout_never);
					sbOsc.setEnabled(false);
				} else {
					int val = Math.max(1, Math.min(60, Math.round(sbOsc.getValue())));
					sp.edit().putInt(prefKey, val).apply();
					tvOsc.setText(getString(R.string.settings_osc_timeout_seconds, val));
					sbOsc.setEnabled(true);
				}
			});
			sbOsc.addOnChangeListener((slider, value, fromUser) -> {
				if (swOscNever.isChecked()) return;
				int val = Math.max(1, Math.min(60, Math.round(value)));
				if (val != Math.round(value)) slider.setValue(val);
				sp.edit().putInt(prefKey, val).apply();
				tvOsc.setText(getString(R.string.settings_osc_timeout_seconds, val));
			});
		} else if (oscGroup != null) {
			oscGroup.setVisibility(View.GONE);
		}
	}

    private void initializeGraphicsSettings() {
        final boolean[] ignoreInit = new boolean[]{true};

		com.google.android.material.button.MaterialButton btnGpuDriverManager = findViewById(R.id.btn_gpu_driver_manager);
		if (btnGpuDriverManager != null) {
			btnGpuDriverManager.setOnClickListener(v -> {
				Intent intent = new Intent(this, GpuDriverManagerActivity.class);
				startActivity(intent);
			});
		}

		// Renderer
		Spinner spRenderer = findViewById(R.id.sp_renderer);
		ArrayAdapter<CharSequence> rendererAdapter = ArrayAdapter.createFromResource(this, R.array.renderers, android.R.layout.simple_spinner_item);
		rendererAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
		spRenderer.setAdapter(rendererAdapter);
		spRenderer.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
			@Override public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
				int value;
				switch (position) {
					case 1: value = 12; break; // OpenGL
					case 2: value = 13; break; // Software
					case 3: value = 14; break; // Vulkan
					default: value = -1; break; // Auto
				}
				if (mIgnoreRendererInit || ignoreInit[0]) { mIgnoreRendererInit = false; ignoreInit[0] = false; return; }
				pendingSettingsResult.putExtra("SET_RENDERER", value);
				markSettingsResultChanged();
			}
			@Override public void onNothingSelected(AdapterView<?> parent) {}
		});

		Spinner spGpuProfileOverride = findViewById(R.id.sp_gpu_profile_override);
		if (spGpuProfileOverride != null) {
			final String[] currentProfile = new String[]{"auto"};
			ArrayAdapter<CharSequence> gpuProfileAdapter = ArrayAdapter.createFromResource(
					this, R.array.gpu_profile_override, android.R.layout.simple_spinner_item);
			gpuProfileAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
			spGpuProfileOverride.setAdapter(gpuProfileAdapter);
			android.content.SharedPreferences prefs = getSharedPreferences("armsx2", MODE_PRIVATE);
			String fallbackProfile = normalizeGpuProfileOverride(
					prefs.getString(PREF_GPU_PROFILE_OVERRIDE_FALLBACK, "auto"));
			String activeProfile = fallbackProfile;
			try {
				String nativeProfile = NativeApp.getSetting("EmuCore/GS", "AndroidGpuProfileOverride", "string");
				if (!TextUtils.isEmpty(nativeProfile)) {
					activeProfile = normalizeGpuProfileOverride(nativeProfile);
				}
			} catch (Exception ignored) {}
			currentProfile[0] = activeProfile;
			spGpuProfileOverride.setSelection(gpuProfileSelectionForValue(activeProfile), false);
			spGpuProfileOverride.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
				@Override public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
					final String value = gpuProfileValueForSelection(position);
					if (value.equalsIgnoreCase(currentProfile[0])) {
						return;
					}
					currentProfile[0] = value;
					boolean persisted = false;
					for (int attempt = 0; attempt < 2 && !persisted; attempt++) {
						try {
							NativeApp.setSetting("EmuCore/GS", "AndroidGpuProfileOverride", "string", value);
							String verify = normalizeGpuProfileOverride(
									NativeApp.getSetting("EmuCore/GS", "AndroidGpuProfileOverride", "string"));
							persisted = value.equalsIgnoreCase(verify);
						} catch (Exception ignored) {}
					}
					prefs.edit().putString(PREF_GPU_PROFILE_OVERRIDE_FALLBACK, value).apply();
					pendingSettingsResult.putExtra(MainActivity.EXTRA_SETTINGS_GPU_PROFILE_OVERRIDE, value);
					pendingSettingsResult.putExtra(MainActivity.EXTRA_SETTINGS_GPU_PROFILE_PERSISTED, persisted);
					markSettingsResultChanged();
					if (!persisted) {
						try {
							Toast.makeText(SettingsActivity.this, R.string.settings_gpu_profile_persist_retrying, Toast.LENGTH_LONG).show();
						} catch (Throwable ignored) {}
					}
				}
				@Override public void onNothingSelected(AdapterView<?> parent) {}
			});
		}

        Slider sbUpscale = findViewById(R.id.sb_upscale);
        TextView tvUpscale = findViewById(R.id.tv_upscale_value);
        if (sbUpscale != null && tvUpscale != null) {
            try {
                String up = NativeApp.getSetting("EmuCore/GS", "upscale_multiplier", "float");
                float f = (up == null || up.isEmpty()) ? 1.0f : Float.parseFloat(up);
                float clamped = Math.max(0.25f, Math.min(8.0f, f));
                sbUpscale.setValue(clamped);
                String formattedInitial = (clamped == (int) clamped)
                        ? String.valueOf((int) clamped) : String.valueOf(clamped);
                tvUpscale.setText(getString(R.string.settings_upscale_value, formattedInitial));
            } catch (Exception ignored) {
                sbUpscale.setValue(1.0f);
                tvUpscale.setText(R.string.settings_upscale_default);
            }
            sbUpscale.addOnChangeListener((slider, value, fromUser) -> {
                String valStr = (value == (int) value) ? String.valueOf((int) value) : String.valueOf(value);
                tvUpscale.setText(getString(R.string.settings_upscale_value, valStr));

                NativeApp.setSetting("EmuCore/GS", "upscale_multiplier", "float", String.valueOf(value));
                NativeApp.renderUpscalemultiplier(value);
                markSettingsResultChanged();
            });
        }

		// Texture Filtering
		Spinner spFiltering = findViewById(R.id.sp_filtering);
		ArrayAdapter<CharSequence> filtAdapter = ArrayAdapter.createFromResource(this, R.array.texture_filtering, android.R.layout.simple_spinner_item);
		filtAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
		spFiltering.setAdapter(filtAdapter);
		try {
			String filt = NativeApp.getSetting("EmuCore/GS", "filter", "int");
			int v = (filt == null || filt.isEmpty()) ? 2 : Integer.parseInt(filt);
			int pos = (v==2)?0: (v==1?1:2);
			spFiltering.setSelection(pos,false);
		} catch (Exception ignored) {}
		spFiltering.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
			@Override public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
				int value;
				switch (position) {
					case 0: value = 2; break; // PS2
					case 1: value = 1; break; // Forced
					case 2: value = 0; break; // Nearest
					default: value = 2; break;
				}
				NativeApp.setSetting("EmuCore/GS", "filter", "int", Integer.toString(value));
			}
			@Override public void onNothingSelected(AdapterView<?> parent) {}
		});

        // Interlace Mode
        Spinner spInterlace = findViewById(R.id.sp_interlace);
		ArrayAdapter<CharSequence> interAdapter = ArrayAdapter.createFromResource(this, R.array.interlace_modes, android.R.layout.simple_spinner_item);
		interAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
		spInterlace.setAdapter(interAdapter);
		try {
			String inter = NativeApp.getSetting("EmuCore/GS", "deinterlace_mode", "int");
			int pos = (inter==null||inter.isEmpty())?0:Integer.parseInt(inter);
			spInterlace.setSelection(Math.max(0, Math.min(interAdapter.getCount()-1, pos)), false);
		} catch (Exception ignored) {}
        spInterlace.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                NativeApp.setSetting("EmuCore/GS", "deinterlace_mode", "int", Integer.toString(position));
            }
            @Override public void onNothingSelected(AdapterView<?> parent) {}
        });

        // FXAA
        MaterialSwitch swFxaa = findViewById(R.id.sw_fxaa);
        if (swFxaa != null) {
            try {
                String fxaa = NativeApp.getSetting("EmuCore/GS", "fxaa", "bool");
                swFxaa.setChecked("true".equalsIgnoreCase(fxaa));
            } catch (Exception ignored) {}
            swFxaa.setOnCheckedChangeListener((buttonView, isChecked) ->
                    NativeApp.setSetting("EmuCore/GS", "fxaa", "bool", isChecked ? "true" : "false"));
        }

        // CAS Mode
        Spinner spCasMode = findViewById(R.id.sp_cas_mode);
        ArrayAdapter<CharSequence> casAdapter = ArrayAdapter.createFromResource(this, R.array.cas_modes, android.R.layout.simple_spinner_item);
        casAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        spCasMode.setAdapter(casAdapter);
        try {
            String cas = NativeApp.getSetting("EmuCore/GS", "CASMode", "int");
            int pos = (cas==null||cas.isEmpty())? 0 : Integer.parseInt(cas);
            spCasMode.setSelection(Math.max(0, Math.min(casAdapter.getCount()-1, pos)), false);
        } catch (Exception ignored) {}
        spCasMode.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                NativeApp.setSetting("EmuCore/GS", "CASMode", "int", Integer.toString(position));
            }
            @Override public void onNothingSelected(AdapterView<?> parent) {}
        });

        Spinner spTexturePreload = findViewById(R.id.sp_texture_preloading);
        if (spTexturePreload != null) {
            ArrayAdapter<CharSequence> preloadAdapter = ArrayAdapter.createFromResource(this, R.array.texture_preloading, android.R.layout.simple_spinner_item);
            preloadAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
            spTexturePreload.setAdapter(preloadAdapter);
            try {
                String preload = NativeApp.getSetting("EmuCore/GS", "texture_preloading", "int");
                int val = (preload == null || preload.isEmpty()) ? 0 : Integer.parseInt(preload);
                if (val < 0 || val >= preloadAdapter.getCount()) val = 0;
                spTexturePreload.setSelection(val, false);
            } catch (Exception ignored) {}
            spTexturePreload.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
                @Override public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                    NativeApp.setSetting("EmuCore/GS", "texture_preloading", "int", Integer.toString(position));
                }
                @Override public void onNothingSelected(AdapterView<?> parent) {}
            });
        }

        Spinner spAccBlending = findViewById(R.id.sp_acc_blending);
        if (spAccBlending != null) {
            ArrayAdapter<CharSequence> blendAdapter = ArrayAdapter.createFromResource(this, R.array.acc_blending, android.R.layout.simple_spinner_item);
            blendAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
            spAccBlending.setAdapter(blendAdapter);
            try {
                String blend = NativeApp.getSetting("EmuCore/GS", "accurate_blending_unit", "int");
                int val = (blend == null || blend.isEmpty()) ? 1 : Integer.parseInt(blend);
                if (val < 0 || val >= blendAdapter.getCount()) val = 1;
                spAccBlending.setSelection(val, false);
            } catch (Exception ignored) {}
            spAccBlending.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
                @Override public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                    NativeApp.setSetting("EmuCore/GS", "accurate_blending_unit", "int", Integer.toString(position));
                }
                @Override public void onNothingSelected(AdapterView<?> parent) {}
            });
        }

        Spinner spAnisotropic = findViewById(R.id.sp_anisotropic);
        if (spAnisotropic != null) {
            ArrayAdapter<CharSequence> anisoAdapter = ArrayAdapter.createFromResource(this, R.array.anisotropic, android.R.layout.simple_spinner_item);
            anisoAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
            spAnisotropic.setAdapter(anisoAdapter);
            final int[] anisoValues = {0, 2, 4, 8, 16};
            try {
                String aniso = NativeApp.getSetting("EmuCore/GS", "MaxAnisotropy", "int");
                int val = (aniso == null || aniso.isEmpty()) ? 0 : Integer.parseInt(aniso);
                int idx = 0;
                for (int i = 0; i < anisoValues.length; i++) {
                    if (anisoValues[i] == val) {
                        idx = i;
                        break;
                    }
                }
                spAnisotropic.setSelection(idx, false);
            } catch (Exception ignored) {}
            spAnisotropic.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
                @Override public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                    int value = position >= 0 && position < anisoValues.length ? anisoValues[position] : 0;
                    NativeApp.setSetting("EmuCore/GS", "MaxAnisotropy", "int", Integer.toString(value));
                }
                @Override public void onNothingSelected(AdapterView<?> parent) {}
            });
        }

        Spinner spTrilinear = findViewById(R.id.sp_trilinear_filter);
        if (spTrilinear != null) {
            ArrayAdapter<CharSequence> triAdapter = ArrayAdapter.createFromResource(this, R.array.trilinear_filtering, android.R.layout.simple_spinner_item);
            triAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
            spTrilinear.setAdapter(triAdapter);
            try {
                String tri = NativeApp.getSetting("EmuCore/GS", "TriFilter", "int");
                int val = (tri == null || tri.isEmpty()) ? 0 : Integer.parseInt(tri);
                if (val < 0 || val >= triAdapter.getCount()) val = 0;
                spTrilinear.setSelection(val, false);
            } catch (Exception ignored) {}
            spTrilinear.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
                @Override public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                    NativeApp.setSetting("EmuCore/GS", "TriFilter", "int", Integer.toString(position));
                }
                @Override public void onNothingSelected(AdapterView<?> parent) {}
            });
        }

        Spinner spDithering = findViewById(R.id.sp_dithering);
        if (spDithering != null) {
            ArrayAdapter<CharSequence> ditheringAdapter = ArrayAdapter.createFromResource(this, R.array.dithering_modes, android.R.layout.simple_spinner_item);
            ditheringAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
            spDithering.setAdapter(ditheringAdapter);
            try {
                String dither = NativeApp.getSetting("EmuCore/GS", "dithering_ps2", "int");
                int val = (dither == null || dither.isEmpty()) ? 2 : Integer.parseInt(dither);
                if (val < 0 || val >= ditheringAdapter.getCount()) val = 2;
                spDithering.setSelection(val, false);
            } catch (Exception ignored) {}
            spDithering.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
                @Override public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                    NativeApp.setSetting("EmuCore/GS", "dithering_ps2", "int", Integer.toString(position));
                }
                @Override public void onNothingSelected(AdapterView<?> parent) {}
            });
        }

        Spinner spBilinearPresent = findViewById(R.id.sp_bilinear_upscale);
        if (spBilinearPresent != null) {
            ArrayAdapter<CharSequence> bilinearAdapter = ArrayAdapter.createFromResource(this, R.array.bilinear_present, android.R.layout.simple_spinner_item);
            bilinearAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
            spBilinearPresent.setAdapter(bilinearAdapter);
            try {
                String linear = NativeApp.getSetting("EmuCore/GS", "linear_present_mode", "int");
                int val = (linear == null || linear.isEmpty()) ? 2 : Integer.parseInt(linear);
                if (val < 0 || val >= bilinearAdapter.getCount()) val = 2;
                spBilinearPresent.setSelection(val, false);
            } catch (Exception ignored) {}
            spBilinearPresent.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
                @Override public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                    NativeApp.setSetting("EmuCore/GS", "linear_present_mode", "int", Integer.toString(position));
                }
                @Override public void onNothingSelected(AdapterView<?> parent) {}
            });
        }

        Slider sbCas = findViewById(R.id.sb_cas_sharpness);
        TextView tvCas = findViewById(R.id.tv_cas_sharpness_value);
        if (sbCas != null && tvCas != null) {
            try {
                String sharp = NativeApp.getSetting("EmuCore/GS", "CASSharpness", "int");
                int v = (sharp == null || sharp.isEmpty()) ? 50 : Integer.parseInt(sharp);
                if (v < 0) v = 0;
                if (v > 100) v = 100;
                sbCas.setValue(v);
                tvCas.setText(getString(R.string.settings_cas_sharpness_value, v));
            } catch (Exception ignored) {
                sbCas.setValue(50f);
                tvCas.setText(R.string.settings_cas_sharpness_default);
            }
            sbCas.addOnChangeListener((slider, value, fromUser) -> {
                int v = Math.max(0, Math.min(100, Math.round(value)));
                if (v != Math.round(value)) slider.setValue(v);
                tvCas.setText(getString(R.string.settings_cas_sharpness_value, v));
                NativeApp.setSetting("EmuCore/GS", "CASSharpness", "int", Integer.toString(v));
            });
        }

		// Hardware Mipmapping
		MaterialSwitch swHWMip = findViewById(R.id.sw_hw_mipmap);
		if (swHWMip != null) {
			try {
				String hw = NativeApp.getSetting("EmuCore/GS", "hw_mipmap", "bool");
				swHWMip.setChecked("true".equalsIgnoreCase(hw));
			} catch (Exception ignored) {}
			swHWMip.setOnCheckedChangeListener((buttonView, isChecked) ->
					NativeApp.setSetting("EmuCore/GS", "hw_mipmap", "bool", isChecked ? "true" : "false"));
		}

        // VSync
        MaterialSwitch swVsync = findViewById(R.id.sw_vsync);
        if (swVsync != null) {
            try {
                String vs = NativeApp.getSetting("EmuCore/GS", "VsyncEnable", "bool");
                swVsync.setChecked("true".equalsIgnoreCase(vs));
            } catch (Exception ignored) {}
            swVsync.setOnCheckedChangeListener((buttonView, isChecked) ->
                    NativeApp.setSetting("EmuCore/GS", "VsyncEnable", "bool", isChecked ? "true" : "false"));
        }

        // Auto Flush (SW)
        MaterialSwitch swAutoFlushSW = findViewById(R.id.sw_autoflush_sw);
        if (swAutoFlushSW != null) {
            try {
                String af = NativeApp.getSetting("EmuCore/GS", "autoflush_sw", "bool");
                swAutoFlushSW.setChecked("true".equalsIgnoreCase(af));
            } catch (Exception ignored) {}
            swAutoFlushSW.setOnCheckedChangeListener((b, isChecked) ->
                    NativeApp.setSetting("EmuCore/GS", "autoflush_sw", "bool", isChecked ? "true" : "false"));
        }

        // Auto Flush (HW)
        Spinner spAutoFlushHW = findViewById(R.id.sp_autoflush_hw);
        if (spAutoFlushHW != null) {
            ArrayAdapter<CharSequence> afAdapter = ArrayAdapter.createFromResource(this, R.array.auto_flush_hw, android.R.layout.simple_spinner_item);
            afAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
            spAutoFlushHW.setAdapter(afAdapter);
            try {
                String lvl = NativeApp.getSetting("EmuCore/GS", "UserHacks_AutoFlushLevel", "int");
                int pos = (lvl==null||lvl.isEmpty()) ? 0 : Integer.parseInt(lvl);
                if (pos < 0 || pos > 2) pos = 0;
                spAutoFlushHW.setSelection(pos, false);
            } catch (Exception ignored) {}
            spAutoFlushHW.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
                @Override public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                    NativeApp.setSetting("EmuCore/GS", "UserHacks_AutoFlushLevel", "int", Integer.toString(position));
                }
                @Override public void onNothingSelected(AdapterView<?> parent) {}
            });
        }

        MaterialSwitch swIntegerScaling = findViewById(R.id.sw_integer_scaling);
        if (swIntegerScaling != null) {
            try {
                String integer = NativeApp.getSetting("EmuCore/GS", "IntegerScaling", "bool");
                swIntegerScaling.setChecked("true".equalsIgnoreCase(integer));
            } catch (Exception ignored) {}
            swIntegerScaling.setOnCheckedChangeListener((buttonView, isChecked) ->
                    NativeApp.setSetting("EmuCore/GS", "IntegerScaling", "bool", isChecked ? "true" : "false"));
        }

        MaterialSwitch swScreenOffsets = findViewById(R.id.sw_screen_offsets);
        if (swScreenOffsets != null) {
            try {
                String offsets = NativeApp.getSetting("EmuCore/GS", "pcrtc_offsets", "bool");
                swScreenOffsets.setChecked("true".equalsIgnoreCase(offsets));
            } catch (Exception ignored) {}
            swScreenOffsets.setOnCheckedChangeListener((buttonView, isChecked) ->
                    NativeApp.setSetting("EmuCore/GS", "pcrtc_offsets", "bool", isChecked ? "true" : "false"));
        }

        MaterialSwitch swShowOverscan = findViewById(R.id.sw_show_overscan);
        if (swShowOverscan != null) {
            try {
                String overscan = NativeApp.getSetting("EmuCore/GS", "pcrtc_overscan", "bool");
                swShowOverscan.setChecked("true".equalsIgnoreCase(overscan));
            } catch (Exception ignored) {}
            swShowOverscan.setOnCheckedChangeListener((buttonView, isChecked) ->
                    NativeApp.setSetting("EmuCore/GS", "pcrtc_overscan", "bool", isChecked ? "true" : "false"));
        }

        MaterialSwitch swAntiblur = findViewById(R.id.sw_antiblur);
        if (swAntiblur != null) {
            try {
                String antiblur = NativeApp.getSetting("EmuCore/GS", "pcrtc_antiblur", "bool");
                if (antiblur == null || antiblur.isEmpty()) {
                    swAntiblur.setChecked(true);
                } else {
                    swAntiblur.setChecked("true".equalsIgnoreCase(antiblur));
                }
            } catch (Exception ignored) {}
            swAntiblur.setOnCheckedChangeListener((buttonView, isChecked) ->
                    NativeApp.setSetting("EmuCore/GS", "pcrtc_antiblur", "bool", isChecked ? "true" : "false"));
        }

        // Set initial renderer value
        try {
            String r = NativeApp.getSetting("EmuCore/GS", "Renderer", "int");
            int v = (r==null||r.isEmpty())? -1 : Integer.parseInt(r);
            int pos; 
			switch (v) { 
				case 12: pos=1; break; 
				case 13: pos=2; break; 
				case 14: pos=3; break; 
				default: pos=0; 
			}
			spRenderer.setSelection(pos, false);
		} catch (Exception ignored) {}
	}

	private void initializeControllerSettings() {
		Button btnCalibrateController = findViewById(R.id.btn_calibrate_controller);
		if (btnCalibrateController != null) {
			btnCalibrateController.setOnClickListener(v -> showControllerCalibrationDialog());
		}

		MaterialButton btnEditMapping = findViewById(R.id.btn_edit_controller_mapping);
		if (btnEditMapping != null) {
			ControllerMappingManager.init(this);
			btnEditMapping.setOnClickListener(v -> new ControllerMappingDialog().show(getSupportFragmentManager(), "controller_mapping"));
		}

		// Vibration Toggle
		MaterialSwitch swVibration = findViewById(R.id.sw_vibration);
		boolean vibrationEnabled = true;
		try {
			String vibration = NativeApp.getSetting("Pad1", "Vibration", "bool");
			if (vibration != null && !vibration.isEmpty()) {
				vibrationEnabled = !"false".equalsIgnoreCase(vibration);
			} else {
				NativeApp.setSetting("Pad1", "Vibration", "bool", "true");
				vibrationEnabled = true;
			}
		} catch (Exception ignored) {}
		swVibration.setChecked(vibrationEnabled);
		MainActivity.setVibrationPreference(vibrationEnabled);
		swVibration.setOnCheckedChangeListener((buttonView, isChecked) -> {
			NativeApp.setSetting("Pad1", "Vibration", "bool", isChecked ? "true" : "false");
			MainActivity.setVibrationPreference(isChecked);
		});
	}

    private void initializePerformanceSettings() {
		// CPU Core (Translator / Interpreter)
		try {
			Spinner spCpu = findViewById(R.id.sp_cpu_core);
			if (spCpu != null) {
				ArrayAdapter<CharSequence> cpuAdapter = ArrayAdapter.createFromResource(
						this,
						R.array.cpu_cores_basic,
						android.R.layout.simple_spinner_item);
				cpuAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
				spCpu.setAdapter(cpuAdapter);

				int pos = 0;
				try {
					String coreTypeStr = NativeApp.getSetting("EmuCore/CPU", "CoreType", "int");
					if (coreTypeStr != null && !coreTypeStr.isEmpty()) {
						int ct = Integer.parseInt(coreTypeStr);
						if (ct < 0 || ct >= cpuAdapter.getCount()) {
							ct = 0;
						}
						pos = ct;
					}
				} catch (Exception ignored) {}
				spCpu.setSelection(pos, false);
				spCpu.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
					@Override public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
						NativeApp.setSetting("EmuCore/CPU", "CoreType", "int", Integer.toString(position));
					}
					@Override public void onNothingSelected(AdapterView<?> parent) {}
				});
			}
		} catch (Throwable ignored) {}

        // Hardware Readbacks
        MaterialSwitch swHwRead = findViewById(R.id.sw_hw_readbacks);
        if (swHwRead != null) {
            try {
                String hr = NativeApp.getSetting("EmuCore/GS", "HardwareReadbacks", "bool");
                swHwRead.setChecked("true".equalsIgnoreCase(hr));
            } catch (Exception ignored) {}
            swHwRead.setOnCheckedChangeListener((buttonView, isChecked) ->
                    NativeApp.setSetting("EmuCore/GS", "HardwareReadbacks", "bool", isChecked ? "true" : "false"));
        }

        // Hardware Download Mode
        Slider sbHwDownloadMode = findViewById(R.id.sb_hw_download_mode);
        TextView tvHwDownloadMode = findViewById(R.id.tv_hw_download_mode);
        if (sbHwDownloadMode != null && tvHwDownloadMode != null) {
            try {
                String mode = NativeApp.getSetting("EmuCore/GS", "HWDownloadMode", "int");
                int v = (mode == null || mode.isEmpty()) ? 0 : Integer.parseInt(mode);
                if (v < 0) v = 0;
                if (v > 3) v = 3;
                sbHwDownloadMode.setValue(v);
                tvHwDownloadMode.setText(getString(R.string.settings_hw_download_mode_value, v));
            } catch (Exception ignored) {
                sbHwDownloadMode.setValue(0f);
                tvHwDownloadMode.setText(R.string.settings_hw_download_mode_default);
            }
            sbHwDownloadMode.addOnChangeListener((slider, value, fromUser) -> {
                int v = Math.max(-3, Math.min(3, Math.round(value)));
                if (v != Math.round(value)) slider.setValue(v);
                tvHwDownloadMode.setText(getString(R.string.settings_hw_download_mode_value, v));
                NativeApp.setSetting("EmuCore/GS", "HWDownloadMode", "int", Integer.toString(v));
            });
        }

        // Skip Duplicate Frames
        MaterialSwitch skipDuplicateFrames = findViewById(R.id.skip_duplicate_frames);
        if (skipDuplicateFrames != null) {
            try {
                String sdf = NativeApp.getSetting("EmuCore/GS", "SkipDuplicateFrames", "bool");
                skipDuplicateFrames.setChecked("true".equalsIgnoreCase(sdf));
            } catch (Exception ignored) {}
            skipDuplicateFrames.setOnCheckedChangeListener((buttonView, isChecked) ->
                    NativeApp.setSetting("EmuCore/GS", "SkipDuplicateFrames", "bool", isChecked ? "true" : "false"));
        }

        Slider sbEeRate = findViewById(R.id.sb_ee_cycle_rate);
        TextView tvEeRate = findViewById(R.id.tv_ee_cycle_rate);
        if (sbEeRate != null && tvEeRate != null) {
            try {
                String rate = NativeApp.getSetting("EmuCore/Speedhacks", "EECycleRate", "int");
                int v = (rate == null || rate.isEmpty()) ? 0 : Integer.parseInt(rate);
                if (v < -3) v = -3;
                if (v > 3) v = 3;
                sbEeRate.setValue(v);
                tvEeRate.setText(getString(R.string.settings_ee_cycle_rate_value, v));
            } catch (Exception ignored) {
                sbEeRate.setValue(0f);
                tvEeRate.setText(R.string.settings_ee_cycle_rate_default);
            }
            sbEeRate.addOnChangeListener((slider, value, fromUser) -> {
                int v = Math.max(-3, Math.min(3, Math.round(value)));
                if (v != Math.round(value)) slider.setValue(v);
                tvEeRate.setText(getString(R.string.settings_ee_cycle_rate_value, v));
                NativeApp.setSetting("EmuCore/Speedhacks", "EECycleRate", "int", Integer.toString(v));
            });
        }

        Slider sbEeSkip = findViewById(R.id.sb_ee_cycle_skip);
        TextView tvEeSkip = findViewById(R.id.tv_ee_cycle_skip);
        if (sbEeSkip != null && tvEeSkip != null) {
            try {
                String skip = NativeApp.getSetting("EmuCore/Speedhacks", "EECycleSkip", "int");
                int v = (skip == null || skip.isEmpty()) ? 0 : Integer.parseInt(skip);
                if (v < 0) v = 0;
                if (v > 3) v = 3;
                sbEeSkip.setValue(v);
                tvEeSkip.setText(getString(R.string.settings_ee_cycle_skip_value, v));
            } catch (Exception ignored) {
                sbEeSkip.setValue(0f);
                tvEeSkip.setText(R.string.settings_ee_cycle_skip_default);
            }
            sbEeSkip.addOnChangeListener((slider, value, fromUser) -> {
                int v = Math.max(0, Math.min(3, Math.round(value)));
                if (v != Math.round(value)) slider.setValue(v);
                tvEeSkip.setText(getString(R.string.settings_ee_cycle_skip_value, v));
                NativeApp.setSetting("EmuCore/Speedhacks", "EECycleSkip", "int", Integer.toString(v));
            });
        }

		MaterialSwitch swWaitLoop = findViewById(R.id.sw_wait_loop);
		if (swWaitLoop != null) {
			boolean enabled = true;
			try {
				String waitLoop = NativeApp.getSetting("EmuCore/Speedhacks", "WaitLoop", "bool");
				if (waitLoop != null && !waitLoop.isEmpty()) {
					enabled = !"false".equalsIgnoreCase(waitLoop);
				}
			} catch (Exception ignored) {}
			swWaitLoop.setChecked(enabled);
			swWaitLoop.setOnCheckedChangeListener((buttonView, isChecked) ->
					NativeApp.setSetting("EmuCore/Speedhacks", "WaitLoop", "bool", isChecked ? "true" : "false"));
		}

		MaterialSwitch swIntc = findViewById(R.id.sw_intc_spin);
		if (swIntc != null) {
			boolean enabled = true;
			try {
				String intc = NativeApp.getSetting("EmuCore/Speedhacks", "IntcStat", "bool");
				if (intc != null && !intc.isEmpty()) {
					enabled = !"false".equalsIgnoreCase(intc);
				}
			} catch (Exception ignored) {}
			swIntc.setChecked(enabled);
			swIntc.setOnCheckedChangeListener((buttonView, isChecked) ->
					NativeApp.setSetting("EmuCore/Speedhacks", "IntcStat", "bool", isChecked ? "true" : "false"));
		}

		MaterialSwitch swMvuFlag = findViewById(R.id.sw_mvu_flag);
		if (swMvuFlag != null) {
			boolean enabled = true;
			try {
				String flag = NativeApp.getSetting("EmuCore/Speedhacks", "vuFlagHack", "bool");
				if (flag != null && !flag.isEmpty()) {
					enabled = !"false".equalsIgnoreCase(flag);
				}
			} catch (Exception ignored) {}
			swMvuFlag.setChecked(enabled);
			swMvuFlag.setOnCheckedChangeListener((buttonView, isChecked) ->
					NativeApp.setSetting("EmuCore/Speedhacks", "vuFlagHack", "bool", isChecked ? "true" : "false"));
		}

		MaterialSwitch swInstantVu1 = findViewById(R.id.sw_instant_vu1);
		if (swInstantVu1 != null) {
			boolean enabled = true;
			try {
				String instant = NativeApp.getSetting("EmuCore/Speedhacks", "vu1Instant", "bool");
				if (instant != null && !instant.isEmpty()) {
					enabled = "true".equalsIgnoreCase(instant);
				}
			} catch (Exception ignored) {}
			swInstantVu1.setChecked(enabled);
			swInstantVu1.setOnCheckedChangeListener((buttonView, isChecked) ->
					NativeApp.setSetting("EmuCore/Speedhacks", "vu1Instant", "bool", isChecked ? "true" : "false"));
		}

		// VU Thread
		MaterialSwitch swVu = findViewById(R.id.sw_vu_thread);
		if (swVu != null) {
			try {
				String vu = NativeApp.getSetting("EmuCore/Speedhacks", "vuThread", "bool");
				swVu.setChecked("true".equalsIgnoreCase(vu));
			} catch (Exception ignored) {}
			if (swInstantVu1 != null) {
				swInstantVu1.setEnabled(!swVu.isChecked());
			}
			swVu.setOnCheckedChangeListener((b, isChecked) -> {
				NativeApp.setSetting("EmuCore/Speedhacks", "vuThread", "bool", isChecked ? "true" : "false");
				if (swInstantVu1 != null) {
					if (isChecked && swInstantVu1.isChecked()) {
						swInstantVu1.setChecked(true);
					}
					swInstantVu1.setEnabled(!isChecked);
				}
			});
		}

        // Fast CDVD
        MaterialSwitch swFastCdvd = findViewById(R.id.sw_fast_cdvd);
        if (swFastCdvd != null) {
            try {
                String fast = NativeApp.getSetting("EmuCore/Speedhacks", "fastCDVD", "bool");
                swFastCdvd.setChecked("true".equalsIgnoreCase(fast));
            } catch (Exception ignored) {}
            swFastCdvd.setOnCheckedChangeListener((b, isChecked) ->
                    NativeApp.setSetting("EmuCore/Speedhacks", "fastCDVD", "bool", isChecked ? "true" : "false"));
        }

        // Frame Limiter
        final Slider sbFpsLimit = findViewById(R.id.sb_fps_limit);
        final TextView tvFpsLimit = findViewById(R.id.tv_fps_limit_value);

        MaterialSwitch swFrameLimiter = findViewById(R.id.sw_frame_limiter);
        if (swFrameLimiter != null) {
            try {
                String ns = NativeApp.getSetting("Framerate", "NominalScalar", "float");
                float scalar = (ns == null || ns.isEmpty()) ? 1.0f : Float.parseFloat(ns);
                swFrameLimiter.setChecked(scalar < 5.0f);
            } catch (Exception ignored) {}
            swFrameLimiter.setOnCheckedChangeListener((buttonView, isChecked) -> {
                if (!isChecked) {
                    NativeApp.setSetting("Framerate", "NominalScalar", "float", Float.toString(10.0f));
                } else {
                    float baseFps = 59.94f;
                    try {
                        String ntsc = NativeApp.getSetting("EmuCore/GS", "FramerateNTSC", "float");
                        if (ntsc != null && !ntsc.isEmpty()) {
                            baseFps = Float.parseFloat(ntsc);
                        }
                    } catch (Exception ignored2) {}
                    int fps = 60;
                    if (sbFpsLimit != null) {
                        fps = Math.max(30, Math.min(180, Math.round(sbFpsLimit.getValue())));
                    }
                    float scalar = fps / baseFps;
                    NativeApp.setSetting("Framerate", "NominalScalar", "float", Float.toString(scalar));
                }
            });
        }

        if (sbFpsLimit != null && tvFpsLimit != null) {
            try {
                float baseFps = 59.94f;
                try {
                    String ntsc = NativeApp.getSetting("EmuCore/GS", "FramerateNTSC", "float");
                    if (ntsc != null && !ntsc.isEmpty()) baseFps = Float.parseFloat(ntsc);
                } catch (Exception ignored2) {}

                String ns = NativeApp.getSetting("Framerate", "NominalScalar", "float");
                float scalar = (ns == null || ns.isEmpty()) ? 1.0f : Float.parseFloat(ns);
                int fpsValue = Math.round(scalar * baseFps);
                if (fpsValue < 30) fpsValue = 30;
                if (fpsValue > 180) fpsValue = 180;
                sbFpsLimit.setValue(fpsValue);
                tvFpsLimit.setText(getString(R.string.settings_custom_fps_limit_value, fpsValue));
            } catch (Exception ignored) {
                sbFpsLimit.setValue(60f);
                tvFpsLimit.setText(R.string.settings_custom_fps_limit_default);
            }
            sbFpsLimit.addOnChangeListener((slider, value, fromUser) -> {
                int fps = Math.max(30, Math.min(180, Math.round(value)));
                if (fps != Math.round(value)) slider.setValue(fps);
                tvFpsLimit.setText(getString(R.string.settings_custom_fps_limit_value, fps));
                float baseFps = 59.94f;
                try {
                    String ntsc = NativeApp.getSetting("EmuCore/GS", "FramerateNTSC", "float");
                    if (ntsc != null && !ntsc.isEmpty()) baseFps = Float.parseFloat(ntsc);
                } catch (Exception ignored2) {}
                float scalar = fps / baseFps;
                NativeApp.setSetting("Framerate", "NominalScalar", "float", Float.toString(scalar));
            });
        }

        // Framerate Control
        //NTSC
        final Slider sbNtscRate = findViewById(R.id.sb_ntsc_framerate);
        final TextView tvNtscRate = findViewById(R.id.tv_ntsc_framerate);if (sbNtscRate != null && tvNtscRate != null) {
            try {
                String rate = NativeApp.getSetting("EmuCore/GS", "FramerateNTSC", "float");
                float v = (rate == null || rate.isEmpty()) ? 59.94f : Float.parseFloat(rate);
                sbNtscRate.setValue(v);
                tvNtscRate.setText(getString(R.string.settings_ntsc_framerate_value, v));
            } catch (Exception ignored) {
                sbNtscRate.setValue(59.94f);
            }
            sbNtscRate.addOnChangeListener((slider, value, fromUser) -> {
                tvNtscRate.setText(getString(R.string.settings_ntsc_framerate_value, value));
                NativeApp.setSetting("EmuCore/GS", "FramerateNTSC", "float", String.valueOf(value));
            });
        }
        //PAL
        final Slider sbPalRate = findViewById(R.id.sb_pal_framerate);
        final TextView tvPalRate = findViewById(R.id.tv_pal_framerate);
        if (sbPalRate != null && tvPalRate != null) {
            try {
                String rate = NativeApp.getSetting("EmuCore/GS", "FrameratePAL", "float");
                float v = (rate == null || rate.isEmpty()) ? 50.00f : Float.parseFloat(rate);
                sbPalRate.setValue(v);
                tvPalRate.setText(getString(R.string.settings_pal_framerate_value, v));
            } catch (Exception ignored) {
                sbPalRate.setValue(50.00f);
            }
            sbPalRate.addOnChangeListener((slider, value, fromUser) -> {
                tvPalRate.setText(getString(R.string.settings_pal_framerate_value, value));
                NativeApp.setSetting("EmuCore/GS", "FrameratePAL", "float", String.valueOf(value));
            });
        }

        // Reset Framrates
        MaterialButton btnResetFramerates = findViewById(R.id.btn_reset_framerates);

        if (btnResetFramerates != null) {
            btnResetFramerates.setOnClickListener(v -> {
                float defaultNtsc = 59.94f;
                float defaultPal = 50.00f;

                if (sbNtscRate != null) sbNtscRate.setValue(defaultNtsc);
                if (sbPalRate != null) sbPalRate.setValue(defaultPal);

                if (tvNtscRate != null) tvNtscRate.setText(getString(R.string.settings_ntsc_framerate_value, defaultNtsc));
                if (tvPalRate != null) tvPalRate.setText(getString(R.string.settings_pal_framerate_value, defaultPal));

                NativeApp.setSetting("EmuCore/GS", "FramerateNTSC", "float", String.valueOf(defaultNtsc));
                NativeApp.setSetting("EmuCore/GS", "FrameratePAL", "float", String.valueOf(defaultPal));

                if (sbFpsLimit != null) sbFpsLimit.setValue(60f);
                if (tvFpsLimit != null) tvFpsLimit.setText(R.string.settings_custom_fps_limit_default);
                if (swFrameLimiter != null) swFrameLimiter.setChecked(true);

                NativeApp.setSetting("Framerate", "NominalScalar", "float", "1.0");

                Toast.makeText(this, "Default FrameRates", Toast.LENGTH_SHORT).show();
            });
        }
    }

    private void initializeStatsSettings() {
        // Performance Overlay
        MaterialSwitch swPerfOverlay = findViewById(R.id.sw_perf_overlay);
        if (swPerfOverlay != null) {
            try {
                String pos = NativeApp.getSetting("EmuCore/GS", "OsdPerformancePos", "int");
                int v = (pos == null || pos.isEmpty()) ? 0 : Integer.parseInt(pos);
                if (v < 0 || v > 2) v = 0;
                swPerfOverlay.setChecked(v != 0);
            } catch (Exception ignored) {}
            swPerfOverlay.setOnCheckedChangeListener((buttonView, isChecked) -> {
                int value = isChecked ? 2 : 0;
                NativeApp.setSetting("EmuCore/GS", "OsdPerformancePos", "int", Integer.toString(value));
            });
        }

        Slider sbOsdScale = findViewById(R.id.sb_osd_scale);
        TextView tvOsdScale = findViewById(R.id.tv_osd_scale);
        if (sbOsdScale != null && tvOsdScale != null) {
            try {
                String scale = NativeApp.getSetting("EmuCore/GS", "OsdScale", "int");
                int v = (scale == null || scale.isEmpty()) ? 50 : Integer.parseInt(scale);
                if (v < 50) v = 50;
                if (v > 100) v = 100;
                sbOsdScale.setValue(v);
                tvOsdScale.setText(getString(R.string.settings_osd_scale_value, v));
            } catch (Exception ignored) {
                sbOsdScale.setValue(100f);
                tvOsdScale.setText(R.string.settings_osd_scale_default);
            }
            sbOsdScale.addOnChangeListener((slider, value, fromUser) -> {
                int v = Math.max(50, Math.min(100, Math.round(value)));
                if (v != Math.round(value)) slider.setValue(v);
                tvOsdScale.setText(getString(R.string.settings_osd_scale_value, v));
                NativeApp.setSetting("EmuCore/GS", "OsdScale", "int", Integer.toString(v));
            });
        }

        // OSD FPS
		MaterialSwitch swOsdFps = findViewById(R.id.sw_osd_fps);
		if (swOsdFps != null) {
			try {
				String fps = NativeApp.getSetting("EmuCore/GS", "OsdShowFPS", "bool");
				swOsdFps.setChecked("true".equalsIgnoreCase(fps));
			} catch (Exception ignored) {}
			swOsdFps.setOnCheckedChangeListener((buttonView, isChecked) ->
					NativeApp.setSetting("EmuCore/GS", "OsdShowFPS", "bool", isChecked ? "true" : "false"));
		}

        // OSD VPS
		MaterialSwitch swOsdVps = findViewById(R.id.sw_osd_vps);
		if (swOsdVps != null) {
			try {
				String vps = NativeApp.getSetting("EmuCore/GS", "OsdShowVPS", "bool");
                swOsdVps.setChecked("true".equalsIgnoreCase(vps));
			} catch (Exception ignored) {}
            swOsdVps.setOnCheckedChangeListener((buttonView, isChecked) ->
					NativeApp.setSetting("EmuCore/GS", "OsdShowVPS", "bool", isChecked ? "true" : "false"));
		}

        // OSD Speed
		MaterialSwitch swOsdSpeed = findViewById(R.id.sw_osd_speed);
		if (swOsdSpeed != null) {
			try {
				String speed = NativeApp.getSetting("EmuCore/GS", "OsdShowSpeed", "bool");
                swOsdSpeed.setChecked("true".equalsIgnoreCase(speed));
			} catch (Exception ignored) {}
            swOsdSpeed.setOnCheckedChangeListener((buttonView, isChecked) ->
					NativeApp.setSetting("EmuCore/GS", "OsdShowSpeed", "bool", isChecked ? "true" : "false"));
		}

        // OSD CPU
		MaterialSwitch swOsdCpu = findViewById(R.id.sw_osd_cpu);
		if (swOsdCpu != null) {
			try {
				String cpu = NativeApp.getSetting("EmuCore/GS", "OsdShowCPU", "bool");
                swOsdCpu.setChecked("true".equalsIgnoreCase(cpu));
			} catch (Exception ignored) {}
            swOsdCpu.setOnCheckedChangeListener((buttonView, isChecked) ->
					NativeApp.setSetting("EmuCore/GS", "OsdShowCPU", "bool", isChecked ? "true" : "false"));
		}

        // OSD GPU
		MaterialSwitch swOsdGpu = findViewById(R.id.sw_osd_gpu);
		if (swOsdGpu != null) {
			try {
				String gpu = NativeApp.getSetting("EmuCore/GS", "OsdShowGPU", "bool");
                swOsdGpu.setChecked("true".equalsIgnoreCase(gpu));
			} catch (Exception ignored) {}
            swOsdGpu.setOnCheckedChangeListener((buttonView, isChecked) ->
					NativeApp.setSetting("EmuCore/GS", "OsdShowGPU", "bool", isChecked ? "true" : "false"));
		}

        // OSD Resolution
		MaterialSwitch swOsdRes = findViewById(R.id.sw_osd_res);
		if (swOsdRes != null) {
			try {
				String res = NativeApp.getSetting("EmuCore/GS", "OsdShowResolution", "bool");
                swOsdRes.setChecked("false".equalsIgnoreCase(res));
			} catch (Exception ignored) {}
            swOsdRes.setOnCheckedChangeListener((buttonView, isChecked) ->
					NativeApp.setSetting("EmuCore/GS", "OsdShowResolution", "bool", isChecked ? "true" : "false"));
		}

        // OSD GS Stats
		MaterialSwitch swOsdGs = findViewById(R.id.sw_osd_gs);
		if (swOsdGs != null) {
			try {
				String gs = NativeApp.getSetting("EmuCore/GS", "OsdShowGSStats", "bool");
                swOsdGs.setChecked("false".equalsIgnoreCase(gs));
			} catch (Exception ignored) {}
            swOsdGs.setOnCheckedChangeListener((buttonView, isChecked) ->
					NativeApp.setSetting("EmuCore/GS", "OsdShowGSStats", "bool", isChecked ? "true" : "false"));
		}

        // OSD Indicators
		MaterialSwitch swOsdIndicators = findViewById(R.id.sw_osd_indicators);
		if (swOsdIndicators != null) {
			try {
				String indicators = NativeApp.getSetting("EmuCore/GS", "OsdShowIndicators", "bool");
                swOsdIndicators.setChecked("false".equalsIgnoreCase(indicators));
			} catch (Exception ignored) {}
            swOsdIndicators.setOnCheckedChangeListener((buttonView, isChecked) ->
					NativeApp.setSetting("EmuCore/GS", "OsdShowIndicators", "bool", isChecked ? "true" : "false"));
		}

        // OSD Settings
		MaterialSwitch swOsdSettings = findViewById(R.id.sw_osd_settings);
		if (swOsdSettings != null) {
			try {
				String settings = NativeApp.getSetting("EmuCore/GS", "OsdShowSettings", "bool");
                swOsdSettings.setChecked("false".equalsIgnoreCase(settings));
			} catch (Exception ignored) {}
            swOsdSettings.setOnCheckedChangeListener((buttonView, isChecked) ->
					NativeApp.setSetting("EmuCore/GS", "OsdShowSettings", "bool", isChecked ? "true" : "false"));
		}

        // OSD Inputs
		MaterialSwitch swOsdInputs = findViewById(R.id.sw_osd_inputs);
		if (swOsdInputs != null) {
			try {
				String inputs = NativeApp.getSetting("EmuCore/GS", "OsdShowInputs", "bool");
                swOsdInputs.setChecked("false".equalsIgnoreCase(inputs));
			} catch (Exception ignored) {}
            swOsdInputs.setOnCheckedChangeListener((buttonView, isChecked) ->
					NativeApp.setSetting("EmuCore/GS", "OsdShowInputs", "bool", isChecked ? "true" : "false"));
		}

        // OSD Frame Times
		MaterialSwitch swOsdFrameTimes = findViewById(R.id.sw_osd_frame_times);
		if (swOsdFrameTimes != null) {
			try {
				String ft = NativeApp.getSetting("EmuCore/GS", "OsdShowFrameTimes", "bool");
                swOsdFrameTimes.setChecked("true".equalsIgnoreCase(ft));
			} catch (Exception ignored) {}
            swOsdFrameTimes.setOnCheckedChangeListener((buttonView, isChecked) ->
					NativeApp.setSetting("EmuCore/GS", "OsdShowFrameTimes", "bool", isChecked ? "true" : "false"));
		}

        // OSD Version
		MaterialSwitch swOsdVersion = findViewById(R.id.sw_osd_version);
		if (swOsdVersion != null) {
			try {
				String ver = NativeApp.getSetting("EmuCore/GS", "OsdShowVersion", "bool");
                swOsdVersion.setChecked("true".equalsIgnoreCase(ver));
			} catch (Exception ignored) {}
            swOsdVersion.setOnCheckedChangeListener((buttonView, isChecked) ->
					NativeApp.setSetting("EmuCore/GS", "OsdShowVersion", "bool", isChecked ? "true" : "false"));
		}

        // OSD HW Info
		MaterialSwitch swOsdHwInfo = findViewById(R.id.sw_osd_hw_info);
		if (swOsdHwInfo != null) {
			try {
				String hw = NativeApp.getSetting("EmuCore/GS", "OsdShowHardwareInfo", "bool");
                swOsdHwInfo.setChecked("true".equalsIgnoreCase(hw));
			} catch (Exception ignored) {}
            swOsdHwInfo.setOnCheckedChangeListener((buttonView, isChecked) ->
					NativeApp.setSetting("EmuCore/GS", "OsdShowHardwareInfo", "bool", isChecked ? "true" : "false"));
		}

        // OSD Video Capture
		MaterialSwitch swOsdVideoCapture = findViewById(R.id.sw_osd_video_capture);
		if (swOsdVideoCapture != null) {
			try {
				String video = NativeApp.getSetting("EmuCore/GS", "OsdShowVideoCapture", "bool");
                swOsdVideoCapture.setChecked("true".equalsIgnoreCase(video));
			} catch (Exception ignored) {}
            swOsdVideoCapture.setOnCheckedChangeListener((buttonView, isChecked) ->
					NativeApp.setSetting("EmuCore/GS", "OsdShowVideoCapture", "bool", isChecked ? "true" : "false"));
		}

        // OSD Video Capture
		MaterialSwitch swOsdInputRec = findViewById(R.id.sw_osd_input_rec);
		if (swOsdInputRec != null) {
			try {
				String rec = NativeApp.getSetting("EmuCore/GS", "OsdShowInputRec", "bool");
                swOsdInputRec.setChecked("true".equalsIgnoreCase(rec));
			} catch (Exception ignored) {}
            swOsdInputRec.setOnCheckedChangeListener((buttonView, isChecked) ->
					NativeApp.setSetting("EmuCore/GS", "OsdShowInputRec", "bool", isChecked ? "true" : "false"));
		}
    }

	private void initializeCustomizationSettings() {
		MaterialButton btnUiStyle = findViewById(R.id.btn_on_screen_ui_style);
		tvOnScreenUiStyleValue = findViewById(R.id.tv_on_screen_ui_style_value);
		updateOnScreenUiStyleSummary();
		if (btnUiStyle != null) {
			btnUiStyle.setOnClickListener(v -> showOnScreenUiStyleDialog());
		}

		MaterialButton btnChangeAppIcon = findViewById(R.id.btn_change_app_icon);
		tvAppIconValue = findViewById(R.id.tv_app_icon_value);
		updateAppIconSummary();
		if (btnChangeAppIcon != null) {
			btnChangeAppIcon.setOnClickListener(v -> showAppIconPickerDialog());
		}
	}

	private void updateOnScreenUiStyleSummary() {
		if (tvOnScreenUiStyleValue == null) {
			return;
		}
		String current = getSharedPreferences("armsx2", MODE_PRIVATE)
				.getString("on_screen_ui_style", "default");
		int labelRes = "nether".equalsIgnoreCase(current)
				? R.string.on_screen_ui_style_nether
				: R.string.on_screen_ui_style_default;
		tvOnScreenUiStyleValue.setText(labelRes);
	}

	private void updateAppIconSummary() {
		if (tvAppIconValue == null) {
			return;
		}
		AppIconManager.AppIconOption option = AppIconManager.getCurrentSelection(this);
		String label = option != null ? option.displayName : getString(R.string.settings_app_icon_default);
		tvAppIconValue.setText(getString(R.string.settings_app_icon_summary, label));
	}

	private void showOnScreenUiStyleDialog() {
		final String prefName = "armsx2";
		final String prefKey = "on_screen_ui_style";
		String current = getSharedPreferences(prefName, MODE_PRIVATE)
				.getString(prefKey, "default");
		int checked = "nether".equalsIgnoreCase(current) ? 1 : 0;
		CharSequence[] options = new CharSequence[]{
				getString(R.string.on_screen_ui_style_default),
				getString(R.string.on_screen_ui_style_nether)
		};
		new MaterialAlertDialogBuilder(this)
				.setTitle(R.string.dialog_on_screen_ui_style_title)
				.setSingleChoiceItems(options, checked, (dialog, which) -> {
					String selected = which == 1 ? "nether" : "default";
					if (!selected.equalsIgnoreCase(current)) {
						getSharedPreferences(prefName, MODE_PRIVATE)
								.edit()
								.putString(prefKey, selected)
								.apply();
						updateOnScreenUiStyleSummary();
					}
					dialog.dismiss();
				})
				.setNegativeButton(android.R.string.cancel, null)
				.show();
	}

	private void showAppIconPickerDialog() {
		if (isFinishing() || isDestroyed()) {
			return;
		}
		List<AppIconManager.AppIconOption> options = AppIconManager.getAvailableIcons(this);
		if (options.size() <= 1) {
			try {
				Toast.makeText(this, R.string.settings_app_icon_picker_empty, Toast.LENGTH_SHORT).show();
			} catch (Throwable ignored) {}
			return;
		}
		AppIconManager.AppIconOption current = AppIconManager.getCurrentSelection(this);
		AppIconOptionAdapter adapter = new AppIconOptionAdapter(this, options);
		AlertDialog dialog = new MaterialAlertDialogBuilder(this)
				.setTitle(R.string.settings_app_icon_picker_title)
				.setNegativeButton(android.R.string.cancel, null)
				.setAdapter(adapter, (d, which) -> {
					AppIconManager.AppIconOption option = adapter.getItem(which);
					if (option != null) {
						handleAppIconSelection(option);
					}
				})
				.create();
		dialog.setOnShowListener(d -> {
			ListView listView = dialog.getListView();
			if (listView == null) {
				return;
			}
			listView.setChoiceMode(ListView.CHOICE_MODE_SINGLE);
			int index = adapter.indexOf(current.id);
			if (index >= 0) {
				listView.setItemChecked(index, true);
				listView.setSelection(index);
			}
			listView.setOnItemClickListener((parent, view, position, id) -> {
				AppIconManager.AppIconOption option = adapter.getItem(position);
				if (option != null) {
					dialog.dismiss();
					handleAppIconSelection(option);
				}
			});
		});
		dialog.show();
	}

	private void handleAppIconSelection(@NonNull AppIconManager.AppIconOption option) {
		AppIconManager.saveSelection(this, option);
		updateAppIconSummary();
		AppIconManager.applyTaskDescription(this);
		boolean showApplyNotice = true;
		if (option.assetPath != null) {
			boolean pinned = AppIconManager.requestPinnedShortcut(this, option);
			if (pinned) {
				showApplyNotice = false;
				try {
					Toast.makeText(this, getString(R.string.settings_app_icon_pin_success, option.displayName), Toast.LENGTH_SHORT).show();
				} catch (Throwable ignored) {}
			} else {
				try {
					Toast.makeText(this, R.string.settings_app_icon_pin_not_supported, Toast.LENGTH_SHORT).show();
				} catch (Throwable ignored) {}
			}
		}
		if (showApplyNotice) {
			try {
				Toast.makeText(this, R.string.settings_app_icon_apply_notice, Toast.LENGTH_SHORT).show();
			} catch (Throwable ignored) {}
		}
	}

    private void initializeMemoryCardSettings() {
        View btnCreateCard = findViewById(R.id.btn_create_card);
        if (btnCreateCard != null) {
            btnCreateCard.setOnClickListener(v -> showCreateCardDialog());
        }
        View btnImportMc = findViewById(R.id.btn_import_memcard);
        if (btnImportMc != null) {
            btnImportMc.setOnClickListener(v -> {
                Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
                i.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
                i.setType("*/*");
                startActivityForResult(Intent.createChooser(i, "Select memory card"), REQ_IMPORT_MEMCARD);
            });
        }

        View btnSwapCards = findViewById(R.id.btn_swap_cards);
        if (btnSwapCards != null) {
            btnSwapCards.setOnClickListener(v -> {
                String slot1 = NativeApp.getSetting("MemoryCards", "Slot1_Filename", "string");
                String slot2 = NativeApp.getSetting("MemoryCards", "Slot2_Filename", "string");
                NativeApp.setSetting("MemoryCards", "Slot1_Filename", "string", slot2);
                NativeApp.setSetting("MemoryCards", "Slot2_Filename", "string", slot1);
                updateMemoryCardUi();
                Toast.makeText(this, R.string.settings_swap_cards, Toast.LENGTH_SHORT).show();
            });
        }
        MaterialSwitch swFilter = findViewById(R.id.sw_memcard_filter);
        if (swFilter != null) {
            try {
                String val = NativeApp.getSetting("EmuCore", "McdFolderAutoManage", "bool");
                swFilter.setChecked("true".equalsIgnoreCase(val));
            } catch (Exception ignored) {}
            swFilter.setOnCheckedChangeListener((b, isChecked) ->
                    NativeApp.setSetting("EmuCore", "McdFolderAutoManage", "bool", isChecked ? "true" : "false"));
        }
        setupCardSlotControls(1, R.id.sw_card_1_enabled, R.id.btn_card_1_name, R.id.tv_card_1_name_value);
        setupCardSlotControls(2, R.id.sw_card_2_enabled, R.id.btn_card_2_name, R.id.tv_card_2_name_value);
    }

    private void setupCardSlotControls(int slot, int swId, int btnId, int tvId) {
        MaterialSwitch sw = findViewById(swId);
        View btn = findViewById(btnId);
        TextView tv = findViewById(tvId);
        String section = "MemoryCards";
        String keyEnable = "Slot" + slot + "_Enable";
        String keyFile = "Slot" + slot + "_Filename";

        if (sw != null) {
            try {
                String enabled = NativeApp.getSetting(section, keyEnable, "bool");
                sw.setChecked("true".equalsIgnoreCase(enabled));
            } catch (Exception ignored) {}
            sw.setOnCheckedChangeListener((b, isChecked) ->
                    NativeApp.setSetting(section, keyEnable, "bool", isChecked ? "true" : "false"));
        }
        if (tv != null) {
            try {
                String filename = NativeApp.getSetting(section, keyFile, "string");
                tv.setText(TextUtils.isEmpty(filename) ? "None" : filename);
            } catch (Exception ignored) {}
        }
        if (btn != null) {
            btn.setOnClickListener(v -> showMemoryCardSelector(slot));
        }
    }

    private void showCreateCardDialog() {
        final TextInputEditText input = new TextInputEditText(this);
        input.setHint("Memory card name (e.g. MySave)");

        final String[] options = {
                "8 MB (Standard High compatibility)",
                "16 MB (May cause issues)",
                "32 MB (May cause issues)",
                "64 MB (Too Large - May cause issues)",
                "Folder (Recommended - Unlimited size)"
        };

        final long[] sizes = {
                8650752,       // 8MB
                17301504,      // 16MB
                34603008,      // 32MB
                69206016,      // 64MB
                -1             // Folder
        };

        final int[] selectedOption = {0};

        new MaterialAlertDialogBuilder(this)
                .setTitle("Create New Memory Card")
                .setView(input)
                .setSingleChoiceItems(options, 0, (dialog, which) -> {
                    selectedOption[0] = which;
                })
                .setPositiveButton("Create", (dialog, which) -> {
                    String name = input.getText().toString().trim();
                    if (TextUtils.isEmpty(name)) return;

                    if (!name.toLowerCase().endsWith(".ps2")) {
                        name += ".ps2";
                    }

                    File memcardsDir = new File(DataDirectoryManager.getDataRoot(this), "memcards");
                    if (!memcardsDir.exists() && !memcardsDir.mkdirs()) return;

                    long size = sizes[selectedOption[0]];

                    if (size == -1) {
                        File folderCard = new File(memcardsDir, name);
                        if (folderCard.exists()) {
                            Toast.makeText(this, "Folder or File already exists", Toast.LENGTH_SHORT).show();
                            return;
                        }
                        if (folderCard.mkdirs()) {
                            Toast.makeText(this, "Folder memory card created!", Toast.LENGTH_SHORT).show();
                            updateMemoryCardUi();
                        }
                    } else {
                        File fileCard = new File(memcardsDir, name);
                        if (fileCard.exists()) {
                            Toast.makeText(this, "File already exists", Toast.LENGTH_SHORT).show();
                            return;
                        }
                        try {
                            if (fileCard.createNewFile()) {
                                java.io.RandomAccessFile raf = new java.io.RandomAccessFile(fileCard, "rw");
                                byte[] buffer = new byte[1024 * 1024];
                                java.util.Arrays.fill(buffer, (byte) 0xFF);

                                long bytesWritten = 0;
                                while (bytesWritten < size) {
                                    long toWrite = Math.min(buffer.length, size - bytesWritten);
                                    raf.write(buffer, 0, (int) toWrite);
                                    bytesWritten += toWrite;
                                }
                                raf.close();
                                Toast.makeText(this, "File card created: " + name, Toast.LENGTH_SHORT).show();
                                updateMemoryCardUi();
                            }
                        } catch (Exception e) {
                            Toast.makeText(this, "Error: " + e.getMessage(), Toast.LENGTH_LONG).show();
                        }
                    }
                })
                .setNegativeButton(android.R.string.cancel, null)
                .show();
    }

    private void showMemoryCardSelector(int slot) {
        File memcardsDir = new File(DataDirectoryManager.getDataRoot(this), "memcards");
        if (!memcardsDir.exists()) memcardsDir.mkdirs();

        File[] files = memcardsDir.listFiles((dir, name) -> name.toLowerCase().endsWith(".ps2"));
        if (files == null || files.length == 0) {
            Toast.makeText(this, "No memory cards found in /memcards", Toast.LENGTH_SHORT).show();
            return;
        }

        String[] displayNames = new String[files.length];
        String[] realNames = new String[files.length];

        for (int i = 0; i < files.length; i++) {
            realNames[i] = files[i].getName();
            if (files[i].isDirectory()) {
                displayNames[i] = files[i].getName() + " (Folder)";
            } else {
                displayNames[i] = files[i].getName();
            }
        }

        new MaterialAlertDialogBuilder(this)
                .setItems(displayNames, (dialog, which) -> {
                    String selected = realNames[which];
                    int otherSlot = (slot == 1) ? 2 : 1;
                    String otherCard = NativeApp.getSetting("MemoryCards", "Slot" + otherSlot + "_Filename", "string");
                    if (selected.equals(otherCard)) {
                        Toast.makeText(this, getString(R.string.settings_memory_card_already_in_use, otherSlot), Toast.LENGTH_LONG).show();
                        return;
                    }
                    NativeApp.setSetting("MemoryCards", "Slot" + slot + "_Filename", "string", selected);
                    updateMemoryCardUi();
                    Toast.makeText(this, "Slot " + slot + " set to: " + selected, Toast.LENGTH_SHORT).show();
                })
                .setNegativeButton(android.R.string.cancel, null)
                .show();
    }

    private void updateMemoryCardUi() {
        TextView tvSlot1 = findViewById(R.id.tv_card_1_name_value);
        TextView tvSlot2 = findViewById(R.id.tv_card_2_name_value);

        String name1 = NativeApp.getSetting("MemoryCards", "Slot1_Filename", "string");
        String name2 = NativeApp.getSetting("MemoryCards", "Slot2_Filename", "string");

        File memcardsDir = new File(DataDirectoryManager.getDataRoot(this), "memcards");

        if (!TextUtils.isEmpty(name1)) {
            File file1 = new File(memcardsDir, name1);
            if (!file1.exists()) {
                NativeApp.setSetting("MemoryCards", "Slot1_Filename", "string", "");
                name1 = "";
            }
        }

        if (!TextUtils.isEmpty(name2)) {
            File file2 = new File(memcardsDir, name2);
            if (!file2.exists()) {
                NativeApp.setSetting("MemoryCards", "Slot2_Filename", "string", "");
                name2 = "";
            }
        }

        if (tvSlot1 != null) {
            if (TextUtils.isEmpty(name1)) {
                tvSlot1.setText("None");
            } else {
                File f1 = new File(memcardsDir, name1);
                String suffix = f1.isDirectory() ? " (Folder)" : "";
                tvSlot1.setText(name1 + suffix);
            }
        }

        if (tvSlot2 != null) {
            if (TextUtils.isEmpty(name2)) {
                tvSlot2.setText("None");
            } else {
                File f2 = new File(memcardsDir, name2);
                String suffix = f2.isDirectory() ? " (Folder)" : "";
                tvSlot2.setText(name2 + suffix);
            }
        }
    }

	private void initializeStorageSettings() {
		tvDataDirPath = findViewById(R.id.tv_data_dir_path);
		MaterialButton btnChange = findViewById(R.id.btn_change_data_dir);
		updateDataDirSummary();
		if (btnChange != null) {
			if (BuildConfig.IS_GOOGLE_PLAY_BUILD) {
				btnChange.setEnabled(false);
				btnChange.setAlpha(0.6f);
				btnChange.setText(R.string.settings_storage_google_play_disabled);
				btnChange.setOnClickListener(null);
			} else {
				btnChange.setEnabled(true);
				btnChange.setAlpha(1f);
				btnChange.setOnClickListener(v -> launchDataDirectoryPicker());
			}
		}

		MaterialButton btnOpenDataDir = findViewById(R.id.btn_open_data_dir);
		if (btnOpenDataDir != null) {
			boolean providerEnabled = getResources().getBoolean(R.bool.armsx2_documents_provider_enabled);
			if (providerEnabled) {
				btnOpenDataDir.setVisibility(View.VISIBLE);
				btnOpenDataDir.setOnClickListener(v -> openDataDirectoryInFilesApp());
			} else {
				btnOpenDataDir.setVisibility(View.GONE);
			}
		}

		MaterialButton btnAddSecondaryGameDir = findViewById(R.id.btn_add_secondary_game_dir);
		if (btnAddSecondaryGameDir != null) {
			btnAddSecondaryGameDir.setEnabled(true);
			btnAddSecondaryGameDir.setAlpha(1f);
			btnAddSecondaryGameDir.setOnClickListener(v -> {
				Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
				intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
				startActivityForResult(intent, 9912);
			});
		}

		MaterialSwitch swDev9Hdd = findViewById(R.id.sw_dev9_hdd_enable);
		TextView tvDev9HddPath = findViewById(R.id.tv_dev9_hdd_path);
		MaterialButton btnDev9Reset = findViewById(R.id.btn_dev9_reset_hdd);
		boolean hddEnabled = false;
		if (swDev9Hdd != null) {
			try {
				String value = NativeApp.getSetting("DEV9/Hdd", "HddEnable", "bool");
				hddEnabled = "true".equalsIgnoreCase(value);
			} catch (Exception ignored) {}
			swDev9Hdd.setChecked(hddEnabled);
		}
		updateDev9HddPathSummary(tvDev9HddPath, hddEnabled);
		if (swDev9Hdd != null) {
			final TextView finalTvDev9HddPath = tvDev9HddPath;
			swDev9Hdd.setOnCheckedChangeListener((buttonView, isChecked) -> {
				NativeApp.setSetting("DEV9/Hdd", "HddEnable", "bool", isChecked ? "true" : "false");
				updateDev9HddPathSummary(finalTvDev9HddPath, isChecked);
			});
		}
		if (btnDev9Reset != null) {
			btnDev9Reset.setOnClickListener(v -> {
				NativeApp.setSetting("DEV9/Hdd", "HddFile", "string", "DEV9hdd.raw");
				updateDev9HddPathSummary(tvDev9HddPath, swDev9Hdd != null && swDev9Hdd.isChecked());
				try {
					Toast.makeText(this, R.string.settings_dev9_hdd_reset_toast, Toast.LENGTH_SHORT).show();
				} catch (Throwable ignored) {}
			});
		}

		MaterialSwitch swDev9Network = findViewById(R.id.sw_dev9_network_enable);
		if (swDev9Network != null) {
			boolean networkEnabled = false;
			try {
				String value = NativeApp.getSetting("DEV9/Eth", "EthEnable", "bool");
				networkEnabled = "true".equalsIgnoreCase(value);
			} catch (Exception ignored) {}
			swDev9Network.setChecked(networkEnabled);
			swDev9Network.setOnCheckedChangeListener((buttonView, isChecked) ->
					NativeApp.setSetting("DEV9/Eth", "EthEnable", "bool", isChecked ? "true" : "false"));
		}
		
		Spinner spDev9networkEthApi = findViewById(R.id.sp_dev9_network_ethapi);

		if (spDev9networkEthApi != null) {
			ArrayAdapter<CharSequence> adapter = ArrayAdapter.createFromResource(
					this,
					R.array.network_ethapi,
					android.R.layout.simple_spinner_item
			);
			adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
			spDev9networkEthApi.setAdapter(adapter);

			// Load saved value from native settings
			try {
				String savedValue = NativeApp.getSetting("DEV9/Eth", "EthApi", "string");
				if (savedValue != null) {
					int pos = adapter.getPosition(savedValue);
					if (pos >= 0) {
						spDev9networkEthApi.setSelection(pos);
					}
				}
			} catch (Exception ignored) {}

			// Save when user selects
			spDev9networkEthApi.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
				@Override
				public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
					String selected = parent.getItemAtPosition(position).toString();
					NativeApp.setSetting("DEV9/Eth", "EthApi", "string", selected);
				}

				@Override
				public void onNothingSelected(AdapterView<?> parent) { }
			});
		}

		Spinner spDev9networkEthDevice = findViewById(R.id.sp_dev9_network_ethdevice);

		if (spDev9networkEthApi != null) {
			List<String> adapterNames = new ArrayList<>();

			try {
				Enumeration<NetworkInterface> interfaces = NetworkInterface.getNetworkInterfaces();

				if (interfaces != null) {
					for (NetworkInterface ni : Collections.list(interfaces)) {
						if(ni.isUp())
							adapterNames.add(ni.getName());  // ONLY getName()
					}
				}
			} catch (Exception e) {
				e.printStackTrace();
			}

			// Create adapter for spinner
			ArrayAdapter<String> adapter = new ArrayAdapter<>(
					this,
					android.R.layout.simple_spinner_item,
					adapterNames
			);

			adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
			spDev9networkEthDevice.setAdapter(adapter);

			// Load saved value from native settings
			try {
				String savedValue = NativeApp.getSetting("DEV9/Eth", "EthDevice", "string");
				if (savedValue != null) {
					int pos = adapter.getPosition(savedValue);
					if (pos >= 0) {
						spDev9networkEthDevice.setSelection(pos);
					}
				}
			} catch (Exception ignored) {}

			// Handle selection events
			spDev9networkEthDevice.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
				@Override
				public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
					String selectedAdapter = adapterNames.get(position);
					// Call your NativeApp function with the selected network adapter
					NativeApp.setSetting("DEV9/Eth", "EthDevice", "string", selectedAdapter);
				}

				@Override
				public void onNothingSelected(AdapterView<?> parent) {
					// Optional: do nothing
				}
			});
		}
	}

	private void updateDev9HddPathSummary(@Nullable TextView target, boolean enabled) {
		if (target == null) {
			return;
		}
		String configured = null;
		try {
			configured = NativeApp.getSetting("DEV9/Hdd", "HddFile", "string");
		} catch (Exception ignored) {}
		if (TextUtils.isEmpty(configured)) {
			configured = "DEV9hdd.raw";
		}
		File resolved = new File(configured);
		if (!resolved.isAbsolute()) {
			File dataRoot = DataDirectoryManager.getDataRoot(getApplicationContext());
			if (dataRoot != null) {
				resolved = new File(dataRoot, configured);
			}
		}
		StringBuilder display = new StringBuilder(resolved.getAbsolutePath());
		if (!enabled) {
			display.append(" (disabled)");
		} else if (!resolved.exists()) {
			display.append(" (will be created on first use)");
		}
		target.setText(display.toString());
	}

	private void setupSectionNavigation(int initialSection) {
		sectionFlipper = findViewById(R.id.settings_view_flipper);
		if (sectionFlipper == null) {
			currentSection = SECTION_GENERAL;
			return;
		}
		sectionFlipper.setInAnimation(AnimationUtils.loadAnimation(this, android.R.anim.fade_in));
		sectionFlipper.setOutAnimation(AnimationUtils.loadAnimation(this, android.R.anim.fade_out));

		sectionToggleGroup = findViewById(R.id.settings_toggle_group);
		if (sectionToggleGroup != null && sectionToggleGroup.getVisibility() == View.VISIBLE) {
			sectionToggleGroup.addOnButtonCheckedListener((group, checkedId, isChecked) -> {
				if (!isChecked || suppressNavigationCallbacks) {
					return;
				}
				setSection(buttonIdToSection(checkedId));
			});
		}

		sectionTabs = findViewById(R.id.settings_tabs);
		if (sectionTabs != null && sectionTabs.getVisibility() == View.VISIBLE) {
			sectionTabs.clearOnTabSelectedListeners();
            if (sectionTabs.getTabCount() == 0) {
                sectionTabs.addTab(sectionTabs.newTab().setText(R.string.settings_section_general));
                sectionTabs.addTab(sectionTabs.newTab().setText(R.string.settings_section_graphics));
                sectionTabs.addTab(sectionTabs.newTab().setText(R.string.settings_section_performance));
                sectionTabs.addTab(sectionTabs.newTab().setText(R.string.settings_section_stats));
                sectionTabs.addTab(sectionTabs.newTab().setText(R.string.settings_section_controller));
                sectionTabs.addTab(sectionTabs.newTab().setText(R.string.settings_section_customization));
                sectionTabs.addTab(sectionTabs.newTab().setText(R.string.settings_section_storage));
                sectionTabs.addTab(sectionTabs.newTab().setText(R.string.settings_section_memory));
                sectionTabs.addTab(sectionTabs.newTab().setText(R.string.settings_section_achievements));
		}
			sectionTabs.addOnTabSelectedListener(new TabLayout.OnTabSelectedListener() {
				@Override
				public void onTabSelected(TabLayout.Tab tab) {
					if (tab != null && !suppressNavigationCallbacks) {
						setSection(tab.getPosition());
					}
				}

				@Override public void onTabUnselected(TabLayout.Tab tab) {}
				@Override public void onTabReselected(TabLayout.Tab tab) {}
			});
		}

		setSection(initialSection);
	}

	private void setSection(int section) {
		if (sectionFlipper == null) {
			currentSection = SECTION_GENERAL;
			return;
		}
        if (section < SECTION_GENERAL || section > SECTION_ACHIEVEMENTS) {
            section = SECTION_GENERAL;
        }
		if (section >= sectionFlipper.getChildCount()) {
			section = Math.max(SECTION_GENERAL, sectionFlipper.getChildCount() - 1);
		}
		if (sectionFlipper.getDisplayedChild() != section) {
			sectionFlipper.setDisplayedChild(section);
		}
		currentSection = section;
		updateToolbarSubtitle(section);
		syncNavigationState(section);
	}

	private void syncNavigationState(int section) {
		suppressNavigationCallbacks = true;
		try {
			if (sectionToggleGroup != null && sectionToggleGroup.getVisibility() == View.VISIBLE) {
				int buttonId = sectionToButtonId(section);
				if (buttonId != View.NO_ID && sectionToggleGroup.getCheckedButtonId() != buttonId) {
					sectionToggleGroup.check(buttonId);
				}
			}
			if (sectionTabs != null && sectionTabs.getVisibility() == View.VISIBLE) {
				TabLayout.Tab tab = sectionTabs.getTabAt(section);
				if (tab != null && !tab.isSelected()) {
					tab.select();
				}
			}
		} finally {
			suppressNavigationCallbacks = false;
		}
	}

	private void updateToolbarSubtitle(int section) {
		if (toolbar == null) {
			return;
		}
		int resId = getSectionTitleRes(section);
		toolbar.setSubtitle(resId != 0 ? getString(resId) : null);
	}

    private int getSectionTitleRes(int section) {
        switch (section) {
            case SECTION_GRAPHICS: return R.string.settings_section_graphics;
            case SECTION_PERFORMANCE: return R.string.settings_section_performance;
            case SECTION_STATS: return R.string.settings_section_stats;
            case SECTION_CONTROLLER: return R.string.settings_section_controller;
            case SECTION_CUSTOMIZATION: return R.string.settings_section_customization;
            case SECTION_STORAGE: return R.string.settings_section_storage;
            case SECTION_MEMORY: return R.string.settings_section_memory;
            case SECTION_ACHIEVEMENTS: return R.string.settings_section_achievements;
            case SECTION_GENERAL:
            default: return R.string.settings_section_general;
        }
	}

	private int buttonIdToSection(int buttonId) {
        if (buttonId == R.id.btn_section_graphics) return SECTION_GRAPHICS;
        if (buttonId == R.id.btn_section_performance) return SECTION_PERFORMANCE;
        if (buttonId == R.id.btn_section_stats) return SECTION_STATS;
        if (buttonId == R.id.btn_section_controller) return SECTION_CONTROLLER;
        if (buttonId == R.id.btn_section_customization) return SECTION_CUSTOMIZATION;
        if (buttonId == R.id.btn_section_storage) return SECTION_STORAGE;
        if (buttonId == R.id.btn_section_memory) return SECTION_MEMORY;
        if (buttonId == R.id.btn_section_achievements) return SECTION_ACHIEVEMENTS;
        return SECTION_GENERAL;
    }

	private int sectionToButtonId(int section) {
        switch (section) {
            case SECTION_GRAPHICS: return R.id.btn_section_graphics;
            case SECTION_PERFORMANCE: return R.id.btn_section_performance;
            case SECTION_STATS: return R.id.btn_section_stats;
            case SECTION_CONTROLLER: return R.id.btn_section_controller;
            case SECTION_CUSTOMIZATION: return R.id.btn_section_customization;
            case SECTION_STORAGE: return R.id.btn_section_storage;
            case SECTION_MEMORY: return R.id.btn_section_memory;
            case SECTION_ACHIEVEMENTS: return R.id.btn_section_achievements;
            case SECTION_GENERAL:
            default: return R.id.btn_section_general;
        }
	}

	private void launchDataDirectoryPicker() {
		Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
		intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
		startActivityResultPickDataDir.launch(intent);
	}

	private void handleDataDirectorySelection(@NonNull Uri tree) {
		String resolvedPath = DataDirectoryManager.resolveTreeUriToPath(this, tree);
		if (resolvedPath == null || resolvedPath.trim().isEmpty()) {
			try { Toast.makeText(this, R.string.onboarding_storage_unusable, Toast.LENGTH_LONG).show(); } catch (Throwable ignored) {}
			return;
		}
		File targetDir = new File(resolvedPath);
		if (!targetDir.exists() && !targetDir.mkdirs()) {
			try { Toast.makeText(this, R.string.onboarding_storage_create_failed, Toast.LENGTH_LONG).show(); } catch (Throwable ignored) {}
			return;
		}
		if (!DataDirectoryManager.canUseDirectFileAccess(targetDir)) {
			showStorageAccessError(targetDir);
			return;
		}
		File currentDir = DataDirectoryManager.getDataRoot(getApplicationContext());
		if (currentDir != null && currentDir.getAbsolutePath().equals(targetDir.getAbsolutePath())) {
			DataDirectoryManager.storeCustomDataRoot(getApplicationContext(), targetDir.getAbsolutePath(), tree.toString());
			NativeApp.setDataRootOverride(targetDir.getAbsolutePath());
			updateDataDirSummary();
			try { Toast.makeText(this, R.string.onboarding_storage_already_using, Toast.LENGTH_SHORT).show(); } catch (Throwable ignored) {}
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
				DataDirectoryManager.copyAssetAll(getApplicationContext(), "resources");
			}
			runOnUiThread(() -> {
				dismissDataDirProgressDialog();
				if (success) {
					try { Toast.makeText(this, R.string.onboarding_storage_moved, Toast.LENGTH_LONG).show(); } catch (Throwable ignored) {}
				} else {
					try { Toast.makeText(this, R.string.onboarding_storage_move_failed, Toast.LENGTH_LONG).show(); } catch (Throwable ignored) {}
				}
				updateDataDirSummary();
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
					.setTitle(R.string.onboarding_storage_moving_title)
					.setMessage(R.string.onboarding_storage_moving_message)
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

	private void updateDataDirSummary() {
		if (tvDataDirPath == null) {
			return;
		}
		File dir = DataDirectoryManager.getDataRoot(getApplicationContext());
        tvDataDirPath.setText(getString(R.string.onboarding_storage_status_default, dir.getAbsolutePath()));
	}

		private void openDataDirectoryInFilesApp() {
			try {
				Uri rootDocumentUri = Armsx2DocumentsProvider.buildRootDocumentUri(this);
				Uri rootUri = Armsx2DocumentsProvider.buildRootUri(this);
				Uri initialTreeUri = rootDocumentUri;
				try {
					String rootDocId = DocumentsContract.getDocumentId(rootDocumentUri);
					initialTreeUri = DocumentsContract.buildTreeDocumentUri(Armsx2DocumentsProvider.authorityFor(this), rootDocId);
				} catch (Exception ignored) {}

				if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
					Intent browseIntent = new Intent(ACTION_BROWSE_DOCUMENT_ROOT);
					browseIntent.setData(rootUri);
					browseIntent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
					if (browseIntent.resolveActivity(getPackageManager()) != null) {
						startActivity(browseIntent);
						return;
					}
				}

				Intent viewIntent = new Intent(Intent.ACTION_VIEW)
					.setDataAndType(rootDocumentUri, DocumentsContract.Document.MIME_TYPE_DIR)
					.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
				viewIntent.putExtra(DocumentsContract.EXTRA_INITIAL_URI, rootDocumentUri);
				viewIntent.putExtra(EXTRA_SHOW_ADVANCED, true);
				if (viewIntent.resolveActivity(getPackageManager()) != null) {
					startActivity(viewIntent);
					return;
				}

				Intent treeIntent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
				treeIntent.putExtra(DocumentsContract.EXTRA_INITIAL_URI, initialTreeUri);
				treeIntent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
				if (treeIntent.resolveActivity(getPackageManager()) != null) {
					startActivity(treeIntent);
					return;
				}
			} catch (FileNotFoundException ignored) {
			}
			try {
				Toast.makeText(this, R.string.settings_open_data_directory_error, Toast.LENGTH_LONG).show();
			} catch (Throwable ignoredToast) {}
		}

	@Override
	protected void onSaveInstanceState(@NonNull Bundle outState) {
		super.onSaveInstanceState(outState);
		outState.putInt(STATE_SELECTED_SECTION, currentSection);
	}

	private int dpToPx(int dp) {
		return Math.round(dp * getResources().getDisplayMetrics().density);
	}

	private void showStorageAccessError(File targetDir) {
		boolean canGrant = Build.VERSION.SDK_INT >= Build.VERSION_CODES.R && !DataDirectoryManager.hasAllFilesAccess();
		String message = getString(R.string.settings_storage_access_error, targetDir.getAbsolutePath());
        AlertDialog.Builder builder = new MaterialAlertDialogBuilder(this)
				.setTitle(R.string.onboarding_storage_access_title)
				.setMessage(message)
				.setNegativeButton(android.R.string.ok, (d, w) -> d.dismiss());
		if (canGrant) {
			builder.setPositiveButton(R.string.onboarding_storage_access_settings, (d, w) -> {
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

	private void initializeActionButtons() {
		Button btnAbout = findViewById(R.id.btn_about);
		btnAbout.setOnClickListener(v -> {
			String versionName = "";
			try { 
				versionName = getPackageManager().getPackageInfo(getPackageName(), 0).versionName; 
			} catch (Exception ignored) {}
			String message = "ARMSX2 (" + versionName + ")\n" +
        		"by ARMSX2 team\n\n" +
        		"Core contributors:\n" +
        		"- MoonPower â€” App developer\n" +
        		"- jpolo â€” Management\n" +
        		"- Medievalshell â€” Web developer\n" +
        		"- set l â€” Web developer\n" +
        		"- Alex â€” QA tester\n" +
        		"- Yua â€” QA tester\n\n" +
        		"Thanks to:\n" +
        		"- pontos2024 (emulator base)\n" +
        		"- PCSX2 v2.3.430 (core emulator)\n" +
        		"- SDL (SDL3)\n" +
        		"- Fffathur (icon design)\n" +
				"- vivimagic0 (icon design)";
			new MaterialAlertDialogBuilder(this)
					.setTitle(R.string.drawer_about)
					.setMessage(message)
					.setPositiveButton(android.R.string.ok, (d, w) -> d.dismiss())
					.show();
		});

		Button btnBack = findViewById(R.id.btn_back);
		btnBack.setOnClickListener(v -> finish());
	}

	private void showControllerCalibrationDialog() {
		View dialogView = getLayoutInflater().inflate(R.layout.dialog_controller_calibration, null);
		AlertDialog dialog = new MaterialAlertDialogBuilder(this)
				.setView(dialogView)
				.setCancelable(true)
				.create();

		// Left Stick Deadzone
		SeekBar sbLeftDeadzone = dialogView.findViewById(R.id.sb_left_deadzone);
		TextView tvLeftDeadzone = dialogView.findViewById(R.id.tv_left_deadzone_value);
		try {
			String deadzone = NativeApp.getSetting("InputSources/SDL", "ControllerDeadzone", "float");
			float value = deadzone == null || deadzone.isEmpty() ? 0.10f : Float.parseFloat(deadzone);
			int progress = Math.round(value * 100);
			sbLeftDeadzone.setProgress(progress);
			tvLeftDeadzone.setText(getString(R.string.settings_left_stick_deadzone_value, value));
		} catch (Exception ignored) {}
		sbLeftDeadzone.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
			@Override public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
				float value = progress / 100.0f;
				tvLeftDeadzone.setText(getString(R.string.settings_left_stick_deadzone_value, value));
			}
			@Override public void onStartTrackingTouch(SeekBar seekBar) {}
			@Override public void onStopTrackingTouch(SeekBar seekBar) {}
		});

		// Right Stick Deadzone
		SeekBar sbRightDeadzone = dialogView.findViewById(R.id.sb_right_deadzone);
		TextView tvRightDeadzone = dialogView.findViewById(R.id.tv_right_deadzone_value);
		try {
			String deadzone = NativeApp.getSetting("InputSources/SDL", "ControllerDeadzone", "float");
			float value = deadzone == null || deadzone.isEmpty() ? 0.10f : Float.parseFloat(deadzone);
			int progress = Math.round(value * 100);
			sbRightDeadzone.setProgress(progress);
			tvRightDeadzone.setText(getString(R.string.settings_right_stick_deadzone_value, value));
		} catch (Exception ignored) {}
		sbRightDeadzone.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
			@Override public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
				float value = progress / 100.0f;
				tvRightDeadzone.setText(getString(R.string.settings_right_stick_deadzone_value, value));
			}
			@Override public void onStartTrackingTouch(SeekBar seekBar) {}
			@Override public void onStopTrackingTouch(SeekBar seekBar) {}
		});

		// Left Stick Sensitivity
		SeekBar sbLeftSensitivity = dialogView.findViewById(R.id.sb_left_sensitivity);
		TextView tvLeftSensitivity = dialogView.findViewById(R.id.tv_left_sensitivity_value);
		try {
			String sensitivity = NativeApp.getSetting("InputSources/SDL", "ControllerSensitivity", "float");
			float value = sensitivity == null || sensitivity.isEmpty() ? 1.0f : Float.parseFloat(sensitivity);
			int progress = Math.round(value * 100);
			sbLeftSensitivity.setProgress(progress);
			tvLeftSensitivity.setText(getString(R.string.settings_left_stick_sensitivity_value, value));
		} catch (Exception ignored) {}
		sbLeftSensitivity.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
			@Override public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
				float value = progress / 100.0f;
				tvLeftSensitivity.setText(getString(R.string.settings_left_stick_sensitivity_value, value));
			}
			@Override public void onStartTrackingTouch(SeekBar seekBar) {}
			@Override public void onStopTrackingTouch(SeekBar seekBar) {}
		});

		// Right Stick Sensitivity
		SeekBar sbRightSensitivity = dialogView.findViewById(R.id.sb_right_sensitivity);
		TextView tvRightSensitivity = dialogView.findViewById(R.id.tv_right_sensitivity_value);
		try {
			String sensitivity = NativeApp.getSetting("InputSources/SDL", "ControllerSensitivity", "float");
			float value = sensitivity == null || sensitivity.isEmpty() ? 1.0f : Float.parseFloat(sensitivity);
			int progress = Math.round(value * 100);
			sbRightSensitivity.setProgress(progress);
			tvRightSensitivity.setText(getString(R.string.settings_right_stick_sensitivity_value, value));
		} catch (Exception ignored) {}
		sbRightSensitivity.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
			@Override public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
				float value = progress / 100.0f;
				tvRightSensitivity.setText(getString(R.string.settings_right_stick_sensitivity_value, value));
			}
			@Override public void onStartTrackingTouch(SeekBar seekBar) {}
			@Override public void onStopTrackingTouch(SeekBar seekBar) {}
		});

		// Dialog buttons
		Button btnCancel = dialogView.findViewById(R.id.btn_calibrate_cancel);
		Button btnApply = dialogView.findViewById(R.id.btn_calibrate_apply);

		btnCancel.setOnClickListener(v -> dialog.dismiss());
		btnApply.setOnClickListener(v -> {
			// Apply settings
			float leftDeadzone = sbLeftDeadzone.getProgress() / 100.0f;
			float rightDeadzone = sbRightDeadzone.getProgress() / 100.0f;
			float leftSensitivity = sbLeftSensitivity.getProgress() / 100.0f;
			float rightSensitivity = sbRightSensitivity.getProgress() / 100.0f;

			NativeApp.setSetting("InputSources/SDL", "ControllerDeadzone", "float", String.valueOf(Math.max(leftDeadzone, rightDeadzone)));
			NativeApp.setSetting("InputSources/SDL", "ControllerSensitivity", "float", String.valueOf(Math.max(leftSensitivity, rightSensitivity)));
			
			Toast.makeText(this, R.string.settings_controller_settings_applied, Toast.LENGTH_SHORT).show();
			dialog.dismiss();
		});

		dialog.show();
	}

	@Override
	protected void onActivityResult(int requestCode, int resultCode, @Nullable Intent data) {
		super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == REQ_IMPORT_MEMCARD && resultCode == RESULT_OK && data != null && data.getData() != null) {
            Uri uri = data.getData();
            String fileName = "imported_card.ps2";
            try (android.database.Cursor cursor = getContentResolver().query(uri, null, null, null, null)) {
                if (cursor != null && cursor.moveToFirst()) {
                    int nameIndex = cursor.getColumnIndex(android.provider.OpenableColumns.DISPLAY_NAME);
                    if (nameIndex != -1) fileName = cursor.getString(nameIndex);
                }
            } catch (Exception ignored) {}

            try {
                getContentResolver().takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
            } catch (Exception ignored) {}

            if (importMemcardToSlot1(uri, fileName)) {
                NativeApp.setSetting("MemoryCards", "Slot1_Filename", "string", fileName);
                updateMemoryCardUi();
                Toast.makeText(this, "Imported: " + fileName, Toast.LENGTH_SHORT).show();
            } else {
				Toast.makeText(this, R.string.settings_memory_card_import_failed, Toast.LENGTH_LONG).show();
			}
		} else if (requestCode == 9912 && resultCode == RESULT_OK && data != null && data.getData() != null) {
			Uri uri = data.getData();
			try {
				final int takeFlags = data.getFlags() & (Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
				getContentResolver().takePersistableUriPermission(uri, takeFlags);
				
				// Store the secondary game directory URI in shared preferences
				String uriString = uri.toString();
				android.content.SharedPreferences prefs = getSharedPreferences("armsx2", MODE_PRIVATE);
				java.util.Set<String> existingDirs = prefs.getStringSet("secondary_game_dirs", new java.util.HashSet<>());
				java.util.Set<String> newDirs = new java.util.HashSet<>(existingDirs);
				newDirs.add(uriString);
				prefs.edit().putStringSet("secondary_game_dirs", newDirs).apply();
				
				Toast.makeText(this, R.string.settings_secondary_game_directory_added, Toast.LENGTH_SHORT).show();
			} catch (Exception e) {
				Toast.makeText(this, R.string.settings_secondary_game_directory_add_failed, Toast.LENGTH_LONG).show();
			}
		}
	}

    private boolean importMemcardToSlot1(Uri uri, String fileName) {
        try {
            File base = DataDirectoryManager.getDataRoot(getApplicationContext());
            File memDir = new File(base, "memcards");
            if (!memDir.exists() && !memDir.mkdirs()) return false;
            File out = new File(memDir, fileName);
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

	private static final class AppIconOptionAdapter extends BaseAdapter {
		private final LayoutInflater inflater;
		private final List<AppIconManager.AppIconOption> options;
		private final Context context;
		private final Map<String, Bitmap> iconCache = new HashMap<>();
		private final int previewSizePx;

		AppIconOptionAdapter(@NonNull Context context, @NonNull List<AppIconManager.AppIconOption> options) {
			this.inflater = LayoutInflater.from(context);
			this.context = context;
			this.options = options;
			float density = context.getResources().getDisplayMetrics().density;
			this.previewSizePx = Math.max(1, Math.round(40f * density));
		}

		int indexOf(@NonNull String id) {
			for (int i = 0; i < options.size(); i++) {
				AppIconManager.AppIconOption option = options.get(i);
				if (TextUtils.equals(option.id, id)) {
					return i;
				}
			}
			return -1;
		}

		@Override
		public int getCount() {
			return options.size();
		}

		@Override
		public AppIconManager.AppIconOption getItem(int position) {
			return options.get(position);
		}

		@Override
		public long getItemId(int position) {
			return position;
		}

		@Override
		public View getView(int position, View convertView, ViewGroup parent) {
			View view = convertView;
			ViewHolder holder;
			if (view == null) {
				view = inflater.inflate(R.layout.dialog_list_item_app_icon, parent, false);
				holder = new ViewHolder();
				holder.icon = view.findViewById(R.id.img_app_icon_preview);
				holder.label = view.findViewById(R.id.tv_app_icon_label);
				view.setTag(holder);
			} else {
				holder = (ViewHolder) view.getTag();
			}

			AppIconManager.AppIconOption option = getItem(position);
			holder.label.setText(option.displayName);
			Bitmap bitmap = iconCache.get(option.id);
			if (bitmap == null) {
				bitmap = AppIconManager.loadIconBitmap(context, option, previewSizePx);
				if (bitmap != null) {
					iconCache.put(option.id, bitmap);
				}
			}
			if (bitmap != null) {
				holder.icon.setImageBitmap(bitmap);
			} else {
				holder.icon.setImageResource(R.mipmap.ic_launcher);
			}
			return view;
		}

		private static final class ViewHolder {
			ImageView icon;
			TextView label;
		}
	}
}

