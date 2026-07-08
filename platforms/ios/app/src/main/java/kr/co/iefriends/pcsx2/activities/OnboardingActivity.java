package kr.co.iefriends.pcsx2.activities;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.provider.Settings;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.TextView;
import android.widget.Toast;

import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.annotation.NonNull;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.recyclerview.widget.RecyclerView;
import androidx.viewpager2.widget.ViewPager2;

import com.google.android.material.appbar.MaterialToolbar;
import com.google.android.material.button.MaterialButton;
import com.google.android.material.dialog.MaterialAlertDialogBuilder;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;

import kr.co.iefriends.pcsx2.BuildConfig;
import kr.co.iefriends.pcsx2.utils.DataDirectoryManager;
import kr.co.iefriends.pcsx2.utils.LogcatRecorder;
import kr.co.iefriends.pcsx2.NativeApp;
import kr.co.iefriends.pcsx2.R;

public class OnboardingActivity extends AppCompatActivity {

    static final String EXTRA_BIOS_CONFIGURED = "bios_configured";
    static final String EXTRA_STORAGE_CONFIGURED = "storage_configured";

    private static final int PAGE_WELCOME = 0;
    private static final int PAGE_BIOS = 1;
    private static final int PAGE_STORAGE = 2;

    private ViewPager2 viewPager;
    private LinearLayout dotsContainer;
    private MaterialButton btnNext;
    private MaterialButton btnBack;
    private OnboardingPagerAdapter adapter;
    private int currentPage = 0;

    private boolean biosConfigured;
    private boolean storageConfigured;
    private String storageStatusText;
    private AlertDialog dataDirProgressDialog;

    private final ActivityResultLauncher<Intent> pickBiosLauncher =
            registerForActivityResult(new ActivityResultContracts.StartActivityForResult(), result -> {
                if (result.getResultCode() == Activity.RESULT_OK && result.getData() != null) {
                    Uri uri = result.getData().getData();
                    if (uri != null) {
                        saveBiosFromUri(uri);
                        updateBiosStatus();
                    }
                }
            });

    private final ActivityResultLauncher<Intent> pickDataDirLauncher =
            registerForActivityResult(new ActivityResultContracts.StartActivityForResult(), result -> {
                if (result.getResultCode() == Activity.RESULT_OK && result.getData() != null) {
                    Uri tree = result.getData().getData();
                    if (tree != null) {
                        final int takeFlags = result.getData().getFlags() & (Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
                        try {
                            getContentResolver().takePersistableUriPermission(tree, takeFlags);
                        } catch (SecurityException ignored) {
                        }
                        handleDataDirectorySelection(tree);
                    }
                }
            });

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_onboarding);

        MaterialToolbar toolbar = findViewById(R.id.onboarding_toolbar);
        if (toolbar != null) {
            toolbar.setTitle(R.string.onboarding_title);
            setSupportActionBar(toolbar);
        }

        viewPager = findViewById(R.id.onboarding_pager);
        dotsContainer = findViewById(R.id.onboarding_dots_container);
        btnNext = findViewById(R.id.btn_onboarding_next);
        btnBack = findViewById(R.id.btn_onboarding_back);

        biosConfigured = hasBios();
        storageConfigured = DataDirectoryManager.isPromptDone(this);
        storageStatusText = buildInitialStorageStatus();
        if (BuildConfig.IS_GOOGLE_PLAY_BUILD) {
            storageConfigured = true;
        }

        adapter = new OnboardingPagerAdapter(new OnboardingCallbacks() {
            @Override
            public void onSelectBios() {
                openBiosPicker();
            }

            @Override
            public void onChooseStorage() {
                if (!BuildConfig.IS_GOOGLE_PLAY_BUILD) {
                    openStoragePicker();
                }
            }

            @Override
            public void onUseDefaultStorage() {
                if (!BuildConfig.IS_GOOGLE_PLAY_BUILD) {
                    applyDefaultStorage();
                }
            }
        });
        adapter.setBiosConfigured(biosConfigured);
        adapter.setStorageStatus(storageConfigured, storageStatusText);
        viewPager.setAdapter(adapter);

        renderDots(PAGE_WELCOME);
        updateButtons(PAGE_WELCOME);

        viewPager.registerOnPageChangeCallback(new ViewPager2.OnPageChangeCallback() {
            @Override
            public void onPageSelected(int position) {
                currentPage = position;
                renderDots(position);
                updateButtons(position);
            }
        });

        btnNext.setOnClickListener(v -> onNextPressed());
        btnBack.setOnClickListener(v -> {
            if (currentPage > 0) {
                viewPager.setCurrentItem(currentPage - 1, true);
            }
        });
    }

    @Override
    protected void onDestroy() {
        dismissDataDirProgressDialog();
        super.onDestroy();
    }

    private void onNextPressed() {
        if (currentPage < adapter.getItemCount() - 1) {
            viewPager.setCurrentItem(currentPage + 1, true);
        } else {
            completeOnboarding();
        }
    }

    private void completeOnboarding() {
        DataDirectoryManager.markPromptDone(this);
        Intent result = new Intent();
        result.putExtra(EXTRA_BIOS_CONFIGURED, biosConfigured);
        result.putExtra(EXTRA_STORAGE_CONFIGURED, storageConfigured);
        setResult(Activity.RESULT_OK, result);
        finish();
    }

    private void updateButtons(int position) {
        boolean isLast = position == adapter.getItemCount() - 1;
        btnNext.setText(isLast ? R.string.onboarding_finish : R.string.onboarding_continue);
        btnBack.setVisibility(position > 0 ? View.VISIBLE : View.GONE);
    }

    private void renderDots(int position) {
        int count = adapter.getItemCount();
        if (dotsContainer.getChildCount() != count) {
            dotsContainer.removeAllViews();
            for (int i = 0; i < count; i++) {
                View dot = new View(this);
                LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(dpToPx(10), dpToPx(10));
                if (i > 0) {
                    lp.setMarginStart(dpToPx(8));
                }
                dot.setLayoutParams(lp);
                dot.setBackgroundResource(i == position ? R.drawable.onboarding_dot_active : R.drawable.onboarding_dot_inactive);
                dotsContainer.addView(dot);
            }
        } else {
            for (int i = 0; i < count; i++) {
                View dot = dotsContainer.getChildAt(i);
                if (dot != null) {
                    dot.setBackgroundResource(i == position ? R.drawable.onboarding_dot_active : R.drawable.onboarding_dot_inactive);
                }
            }
        }
    }

    private void updateBiosStatus() {
        biosConfigured = hasBios();
        adapter.setBiosConfigured(biosConfigured);
    }

    private void updateStorageStatus(String statusText, boolean configured) {
        storageConfigured = configured;
        storageStatusText = statusText;
        adapter.setStorageStatus(configured, statusText);
    }

    private void openBiosPicker() {
        Intent intent = new Intent(Intent.ACTION_GET_CONTENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("application/octet-stream");
        String[] mimeTypes = new String[]{"application/octet-stream", "application/x-binary"};
        intent.putExtra(Intent.EXTRA_MIME_TYPES, mimeTypes);
        try {
            pickBiosLauncher.launch(Intent.createChooser(intent, getString(R.string.onboarding_bios_select)));
        } catch (Exception e) {
            Toast.makeText(this, R.string.onboarding_bios_picker_error, Toast.LENGTH_SHORT).show();
        }
    }

    private void saveBiosFromUri(@NonNull Uri uri) {
        Context ctx = getApplicationContext();
        File base = DataDirectoryManager.getDataRoot(ctx);
        File biosDir = new File(base, "bios");
        if (!biosDir.exists() && !biosDir.mkdirs()) {
            Toast.makeText(this, R.string.onboarding_bios_write_error, Toast.LENGTH_LONG).show();
            return;
        }
        String outName = "ps2_bios.bin";
        try (InputStream in = getContentResolver().openInputStream(uri);
             OutputStream out = new FileOutputStream(new File(biosDir, outName))) {
            if (in == null) {
                throw new IllegalStateException("Unable to open BIOS URI");
            }
            byte[] buf = new byte[8192];
            int n;
            while ((n = in.read(buf)) > 0) {
                out.write(buf, 0, n);
            }
            out.flush();
            Toast.makeText(this, R.string.onboarding_bios_saved, Toast.LENGTH_SHORT).show();
        } catch (Exception e) {
            Toast.makeText(this, R.string.onboarding_bios_write_error, Toast.LENGTH_LONG).show();
        }
    }

    private boolean hasBios() {
        File base = DataDirectoryManager.getDataRoot(getApplicationContext());
        File biosDir = new File(base, "bios");
        if (!biosDir.exists()) {
            return false;
        }
        File[] files = biosDir.listFiles((dir, name) -> name != null && name.toLowerCase().endsWith(".bin"));
        return files != null && files.length > 0;
    }

    private void openStoragePicker() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION |
                Intent.FLAG_GRANT_WRITE_URI_PERMISSION |
                Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
        try {
            pickDataDirLauncher.launch(intent);
        } catch (Exception e) {
            Toast.makeText(this, R.string.onboarding_storage_picker_error, Toast.LENGTH_SHORT).show();
        }
    }

    private void applyDefaultStorage() {
        File defaultRoot = DataDirectoryManager.getDefaultDataRoot(getApplicationContext());
        DataDirectoryManager.clearCustomDataRoot(getApplicationContext());
        DataDirectoryManager.markPromptDone(this);
        String status = defaultRoot != null
                ? getString(R.string.onboarding_storage_status_default, defaultRoot.getAbsolutePath())
                : getString(R.string.onboarding_storage_status_missing);
        updateStorageStatus(status, true);
        Toast.makeText(this, R.string.onboarding_storage_default_set, Toast.LENGTH_SHORT).show();
    }

    private void handleDataDirectorySelection(@NonNull Uri tree) {
        String resolvedPath = DataDirectoryManager.resolveTreeUriToPath(this, tree);
        if (resolvedPath == null || resolvedPath.trim().isEmpty()) {
            Toast.makeText(this, R.string.onboarding_storage_unusable, Toast.LENGTH_LONG).show();
            return;
        }
        File targetDir = new File(resolvedPath);
        if (!targetDir.exists() && !targetDir.mkdirs()) {
            Toast.makeText(this, R.string.onboarding_storage_create_failed, Toast.LENGTH_LONG).show();
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
            DataDirectoryManager.markPromptDone(this);
            String status = getString(R.string.onboarding_storage_status_custom, targetDir.getAbsolutePath());
            updateStorageStatus(status, true);
            Toast.makeText(this, R.string.onboarding_storage_already_using, Toast.LENGTH_SHORT).show();
            return;
        }
        beginDataDirectoryMigration(currentDir, targetDir, tree.toString());
    }

    private void beginDataDirectoryMigration(File currentDir, File targetDir, String uriString) {
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
                    String status = getString(R.string.onboarding_storage_status_custom, targetDir.getAbsolutePath());
                    updateStorageStatus(status, true);
                    Toast.makeText(this, R.string.onboarding_storage_moved, Toast.LENGTH_LONG).show();
                } else {
                    Toast.makeText(this, R.string.onboarding_storage_move_failed, Toast.LENGTH_LONG).show();
                }
            });
        }, "OnboardingDataMigration").start();
    }

    private void showDataDirProgressDialog() {
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
    }

    private void dismissDataDirProgressDialog() {
        if (dataDirProgressDialog != null) {
            dataDirProgressDialog.dismiss();
            dataDirProgressDialog = null;
        }
    }

    private void showStorageAccessError(File targetDir) {
        boolean canGrant = Build.VERSION.SDK_INT >= Build.VERSION_CODES.R && !DataDirectoryManager.hasAllFilesAccess();
        String message = getString(R.string.onboarding_storage_access_error, targetDir.getAbsolutePath());
        MaterialAlertDialogBuilder builder = new MaterialAlertDialogBuilder(this)
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
                return;
            } catch (Exception ignored) {
            }
            try {
                Intent intent = new Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION);
                startActivity(intent);
            } catch (Exception ignored) {
            }
        }
    }

    private String buildInitialStorageStatus() {
        if (BuildConfig.IS_GOOGLE_PLAY_BUILD) {
            File dataRoot = DataDirectoryManager.getDataRoot(getApplicationContext());
            String path = (dataRoot != null && !TextUtils.isEmpty(dataRoot.getAbsolutePath()))
                    ? dataRoot.getAbsolutePath()
                    : getString(R.string.onboarding_storage_status_missing);
            return getString(R.string.onboarding_storage_google_play_unavailable, path);
        }
        File dataRoot = DataDirectoryManager.getDataRoot(getApplicationContext());
        boolean custom = DataDirectoryManager.hasCustomDataRoot(getApplicationContext());
        if (dataRoot == null || TextUtils.isEmpty(dataRoot.getAbsolutePath())) {
            return getString(R.string.onboarding_storage_status_missing);
        }
        int resId = custom ? R.string.onboarding_storage_status_custom : R.string.onboarding_storage_status_default;
        return getString(resId, dataRoot.getAbsolutePath());
    }

    private int dpToPx(int dp) {
        float density = getResources().getDisplayMetrics().density;
        return Math.round(dp * density);
    }

    private interface OnboardingCallbacks {
        void onSelectBios();
        void onChooseStorage();
        void onUseDefaultStorage();
    }

    private static class OnboardingPagerAdapter extends RecyclerView.Adapter<OnboardingPagerAdapter.PageViewHolder> {

        private static final int[] PAGE_LAYOUTS = {
                R.layout.onboarding_page_welcome,
                R.layout.onboarding_page_bios,
                R.layout.onboarding_page_storage
        };

        private final OnboardingCallbacks callbacks;
        private boolean biosConfigured;
        private boolean storageConfigured;
        private String storageStatusText;

        OnboardingPagerAdapter(OnboardingCallbacks callbacks) {
            this.callbacks = callbacks;
            this.storageStatusText = "";
        }

        void setBiosConfigured(boolean configured) {
            this.biosConfigured = configured;
            notifyItemChanged(PAGE_BIOS);
        }

        void setStorageStatus(boolean configured, String statusText) {
            this.storageConfigured = configured;
            this.storageStatusText = statusText;
            notifyItemChanged(PAGE_STORAGE);
        }

        @Override
        public int getItemCount() {
            return PAGE_LAYOUTS.length;
        }

        @Override
        public int getItemViewType(int position) {
            return PAGE_LAYOUTS[position];
        }

        @NonNull
        @Override
        public PageViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
            LayoutInflater inflater = LayoutInflater.from(parent.getContext());
            View view = inflater.inflate(viewType, parent, false);
            return new PageViewHolder(view);
        }

        @Override
        public void onBindViewHolder(@NonNull PageViewHolder holder, int position) {
            int layout = holder.getItemViewType();
            if (layout == R.layout.onboarding_page_bios) {
                MaterialButton button = holder.itemView.findViewById(R.id.btn_onboarding_select_bios);
                if (button != null) {
                    button.setOnClickListener(v -> callbacks.onSelectBios());
                }
                TextView status = holder.itemView.findViewById(R.id.tv_onboarding_bios_status);
                if (status != null) {
                    int textRes = biosConfigured ? R.string.onboarding_bios_status_ready : R.string.onboarding_bios_status_missing;
                    status.setText(status.getContext().getString(textRes));
                }
            } else if (layout == R.layout.onboarding_page_storage) {
                MaterialButton choose = holder.itemView.findViewById(R.id.btn_onboarding_choose_storage);
                if (choose != null) {
                    if (BuildConfig.IS_GOOGLE_PLAY_BUILD) {
                        choose.setEnabled(false);
                        choose.setClickable(false);
                        choose.setAlpha(0.6f);
                        choose.setText(R.string.settings_storage_google_play_disabled);
                        choose.setOnClickListener(null);
                    } else {
                        choose.setEnabled(true);
                        choose.setAlpha(1f);
                        choose.setText(R.string.onboarding_storage_choose);
                        choose.setOnClickListener(v -> callbacks.onChooseStorage());
                    }
                }
                MaterialButton useDefault = holder.itemView.findViewById(R.id.btn_onboarding_use_default);
                if (useDefault != null) {
                    if (BuildConfig.IS_GOOGLE_PLAY_BUILD) {
                        useDefault.setEnabled(false);
                        useDefault.setClickable(false);
                        useDefault.setAlpha(0.6f);
                        useDefault.setText(R.string.settings_storage_google_play_disabled);
                        useDefault.setOnClickListener(null);
                    } else {
                        useDefault.setEnabled(true);
                        useDefault.setAlpha(1f);
                        useDefault.setText(R.string.onboarding_storage_use_default);
                        useDefault.setOnClickListener(v -> callbacks.onUseDefaultStorage());
                    }
                }
                TextView status = holder.itemView.findViewById(R.id.tv_onboarding_storage_status);
                if (status != null) {
                    if (!TextUtils.isEmpty(storageStatusText)) {
                        status.setText(storageStatusText);
                    } else {
                        status.setText(R.string.onboarding_storage_status_missing);
                    }
                }
            }
        }

        static class PageViewHolder extends RecyclerView.ViewHolder {
            PageViewHolder(@NonNull View itemView) {
                super(itemView);
            }
        }
    }
}
