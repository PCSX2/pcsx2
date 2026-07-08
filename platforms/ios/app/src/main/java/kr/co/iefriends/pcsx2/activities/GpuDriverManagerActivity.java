// SPDX-FileCopyrightText: 2025 ARMSX2 Team (SternXD)
// SPDX-License-Identifier: GPL-3.0+

package kr.co.iefriends.pcsx2.activities;

import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageButton;
import android.widget.RadioButton;
import android.widget.TextView;
import android.widget.Toast;

import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;

import com.google.android.material.appbar.MaterialToolbar;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.google.android.material.dialog.MaterialAlertDialogBuilder;
import com.google.android.material.floatingactionbutton.FloatingActionButton;

import java.io.File;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.List;

import kr.co.iefriends.pcsx2.NativeApp;
import kr.co.iefriends.pcsx2.R;
import kr.co.iefriends.pcsx2.utils.GpuDriverHelper;
import kr.co.iefriends.pcsx2.utils.GpuDriverMetadata;

public class GpuDriverManagerActivity extends AppCompatActivity {
    private GpuDriverHelper mDriverHelper;
    private RecyclerView mDriverList;
    private DriverAdapter mAdapter;
    private ActivityResultLauncher<String> mFilePickerLauncher;
    private String mSelectedDriverPath;
    
    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_gpu_driver_manager);
        
        // Set native library directory for libadrenotools
        String nativeLibDir = getApplicationInfo().nativeLibraryDir;
        if (!TextUtils.isEmpty(nativeLibDir)) {
            NativeApp.setNativeLibraryDir(nativeLibDir);
        }
        
        mDriverHelper = new GpuDriverHelper(this);
        mSelectedDriverPath = mDriverHelper.getCustomDriverPath();
        
        MaterialToolbar toolbar = findViewById(R.id.toolbar);
        setSupportActionBar(toolbar);
        if (getSupportActionBar() != null) {
            getSupportActionBar().setDisplayHomeAsUpEnabled(true);
        }
        toolbar.setNavigationOnClickListener(v -> finish());
        
        mDriverList = findViewById(R.id.driver_list);
        FloatingActionButton fabInstall = findViewById(R.id.fab_install_driver);
        
        mAdapter = new DriverAdapter();
        mDriverList.setLayoutManager(new LinearLayoutManager(this));
        mDriverList.setAdapter(mAdapter);
        
        fabInstall.setOnClickListener(v -> showInstallDialog());
        
        mFilePickerLauncher = registerForActivityResult(
            new ActivityResultContracts.GetContent(),
            uri -> {
                if (uri != null) {
                    installDriverFromUri(uri);
                }
            }
        );
        
        refreshDriverList();
    }
    
    @Override
    public boolean onOptionsItemSelected(@NonNull MenuItem item) {
        if (item.getItemId() == android.R.id.home) {
            finish();
            return true;
        }
        return super.onOptionsItemSelected(item);
    }
    
    private void refreshDriverList() {
        mSelectedDriverPath = mDriverHelper.getCustomDriverPath();
        mAdapter.updateDrivers();
    }
    
    private void selectDriver(@NonNull GpuDriverHelper.GpuDriver driver) {
        if (driver.isSystemDriver()) {
            // Use system driver
            mDriverHelper.installDefaultDriver();
            mSelectedDriverPath = null;
            Toast.makeText(this, R.string.gpu_driver_system_selected, Toast.LENGTH_SHORT).show();
        } else {
            // Install and use custom driver from ZIP
            File zipFile = new File(driver.getPath());
            if (zipFile.exists()) {
                boolean success = mDriverHelper.installCustomDriverComplete(zipFile);
                if (success) {
                    // Find the installed library path
                    GpuDriverMetadata metadata = driver.getMetadata();
                    if (metadata != null && !TextUtils.isEmpty(metadata.getLibraryName())) {
                        File libFile = new File(mDriverHelper.getDriversInstallDirectory(), metadata.getLibraryName());
                        if (!libFile.exists() && !metadata.getLibraryName().endsWith(".so")) {
                            libFile = new File(mDriverHelper.getDriversInstallDirectory(), metadata.getLibraryName() + ".so");
                        }
                        if (libFile.exists()) {
                            mSelectedDriverPath = libFile.getAbsolutePath();
                        }
                    }
                    Toast.makeText(this, getString(R.string.gpu_driver_selected, driver.getName()), Toast.LENGTH_SHORT).show();
                } else {
                    Toast.makeText(this, R.string.gpu_driver_install_failed, Toast.LENGTH_LONG).show();
                }
            } else {
                Toast.makeText(this, R.string.gpu_driver_install_failed, Toast.LENGTH_LONG).show();
            }
        }
        
        refreshDriverList();
    }
    
    private void deleteDriver(@NonNull GpuDriverHelper.GpuDriver driver) {
        if (driver.isSystemDriver()) {
            return;
        }
        
        new MaterialAlertDialogBuilder(this)
            .setTitle(R.string.gpu_driver_delete_title)
            .setMessage(getString(R.string.gpu_driver_delete_message, driver.getName()))
            .setPositiveButton(android.R.string.yes, (dialog, which) -> {
                boolean deleted = mDriverHelper.deleteDriver(driver.getPath());
                if (deleted) {
                    // If this was the selected driver, switch to system
                    if (!TextUtils.isEmpty(mSelectedDriverPath) && mSelectedDriverPath.contains(driver.getPath())) {
                        mDriverHelper.installDefaultDriver();
                    }
                    Toast.makeText(this, R.string.gpu_driver_deleted, Toast.LENGTH_SHORT).show();
                    refreshDriverList();
                } else {
                    Toast.makeText(this, R.string.gpu_driver_delete_failed, Toast.LENGTH_LONG).show();
                }
            })
            .setNegativeButton(android.R.string.no, null)
            .show();
    }
    
    private void showInstallDialog() {
        new MaterialAlertDialogBuilder(this)
            .setTitle(R.string.gpu_driver_install_title)
            .setMessage(R.string.gpu_driver_install_message)
            .setPositiveButton(R.string.gpu_driver_install_from_file, (dialog, which) -> {
                mFilePickerLauncher.launch("application/zip");
            })
            .setNegativeButton(android.R.string.cancel, null)
            .show();
    }
    
    private void installDriverFromUri(@NonNull Uri uri) {
        try {
            // Validate the driver ZIP first
            try (InputStream inputStream = getContentResolver().openInputStream(uri)) {
                if (inputStream == null) {
                    Toast.makeText(this, R.string.gpu_driver_install_failed, Toast.LENGTH_LONG).show();
                    return;
                }
                
                GpuDriverMetadata metadata = mDriverHelper.getMetadataFromZip(inputStream);
                if (!metadata.isValid()) {
                    Toast.makeText(this, R.string.gpu_driver_invalid_zip, Toast.LENGTH_LONG).show();
                    return;
                }
                
                if (metadata.getMinApi() > android.os.Build.VERSION.SDK_INT) {
                    Toast.makeText(this, getString(R.string.gpu_driver_api_too_high, metadata.getMinApi(), android.os.Build.VERSION.SDK_INT), Toast.LENGTH_LONG).show();
                    return;
                }
            }
            
            // Install the driver
            boolean success = mDriverHelper.installDriverFromUri(uri);
            if (success) {
                String fileName = getFileNameFromUri(uri);
                if (TextUtils.isEmpty(fileName)) {
                    fileName = "driver.zip";
                }
                Toast.makeText(this, getString(R.string.gpu_driver_installed, fileName), Toast.LENGTH_SHORT).show();
                refreshDriverList();
            } else {
                Toast.makeText(this, R.string.gpu_driver_install_failed, Toast.LENGTH_LONG).show();
            }
            
        } catch (Exception e) {
            Toast.makeText(this, R.string.gpu_driver_install_failed, Toast.LENGTH_LONG).show();
        }
    }
    
    @Nullable
    private String getFileNameFromUri(@NonNull Uri uri) {
        String result = null;
        if (uri.getScheme().equals("content")) {
            try (android.database.Cursor cursor = getContentResolver().query(uri, null, null, null, null)) {
                if (cursor != null && cursor.moveToFirst()) {
                    int nameIndex = cursor.getColumnIndex(android.provider.OpenableColumns.DISPLAY_NAME);
                    if (nameIndex >= 0) {
                        result = cursor.getString(nameIndex);
                    }
                }
            }
        }
        if (result == null) {
            result = uri.getPath();
            if (result != null) {
                int cut = result.lastIndexOf('/');
                if (cut != -1) {
                    result = result.substring(cut + 1);
                }
            }
        }
        return result;
    }
    
    private class DriverAdapter extends RecyclerView.Adapter<DriverAdapter.DriverViewHolder> {
        private final List<GpuDriverHelper.GpuDriver> mDrivers = new ArrayList<>();
        
        void updateDrivers() {
            mDrivers.clear();
            
            // Add system driver first
            mDrivers.add(mDriverHelper.getSystemDriver());
            
            // Add custom drivers
            mDrivers.addAll(mDriverHelper.getAvailableDrivers());
            
            notifyDataSetChanged();
        }
        
        @NonNull
        @Override
        public DriverViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
            View view = LayoutInflater.from(parent.getContext()).inflate(R.layout.item_gpu_driver, parent, false);
            return new DriverViewHolder(view);
        }
        
        @Override
        public void onBindViewHolder(@NonNull DriverViewHolder holder, int position) {
            GpuDriverHelper.GpuDriver driver = mDrivers.get(position);
            holder.bind(driver);
        }
        
        @Override
        public int getItemCount() {
            return mDrivers.size();
        }
        
        private class DriverViewHolder extends RecyclerView.ViewHolder {
            private final RadioButton mRadioButton;
            private final TextView mDriverName;
            private final TextView mVulkanVersion;
            private final TextView mDriverInfo;
            private final TextView mDescription;
            private final ImageButton mDeleteButton;
            private final View mCardView;
            
            DriverViewHolder(@NonNull View itemView) {
                super(itemView);
                mCardView = itemView;
                mRadioButton = itemView.findViewById(R.id.radio_driver);
                mDriverName = itemView.findViewById(R.id.tv_driver_name);
                mVulkanVersion = itemView.findViewById(R.id.tv_driver_vulkan);
                mDriverInfo = itemView.findViewById(R.id.tv_driver_info);
                mDescription = itemView.findViewById(R.id.tv_driver_description);
                mDeleteButton = itemView.findViewById(R.id.btn_delete_driver);
            }
            
            void bind(@NonNull GpuDriverHelper.GpuDriver driver) {
                boolean isSystemDriver = driver.isSystemDriver();
                boolean isSelected;
                if (isSystemDriver) {
                    isSelected = TextUtils.isEmpty(mSelectedDriverPath);
                } else {
                    // Check if this driver's ZIP is installed and selected
                    GpuDriverMetadata metadata = driver.getMetadata();
                    if (metadata != null && !TextUtils.isEmpty(metadata.getLibraryName())) {
                        File libFile = new File(GpuDriverManagerActivity.this.mDriverHelper.getDriversInstallDirectory(), metadata.getLibraryName());
                        if (!libFile.exists() && !metadata.getLibraryName().endsWith(".so")) {
                            libFile = new File(GpuDriverManagerActivity.this.mDriverHelper.getDriversInstallDirectory(), metadata.getLibraryName() + ".so");
                        }
                        isSelected = libFile.exists() && !TextUtils.isEmpty(mSelectedDriverPath) && 
                            mSelectedDriverPath.equals(libFile.getAbsolutePath());
                    } else {
                        isSelected = false;
                    }
                }
                
                // Set driver name
                if (isSystemDriver) {
                    mDriverName.setText(R.string.gpu_driver_system_name);
                } else {
                    mDriverName.setText(driver.getName());
                }
                
                // Set Vulkan version and description
                GpuDriverMetadata metadata = driver.getMetadata();
                if (metadata != null) {
                    String vulkanVer = metadata.getVulkanVersion();
                    if (TextUtils.isEmpty(vulkanVer)) {
                        String driverVer = metadata.getVersion();
                        if (!TextUtils.isEmpty(driverVer)) {
                            if (driverVer.matches("^\\d+\\.\\d+\\.\\d+.*$")) {
                                vulkanVer = driverVer;
                            }
                        }
                    }
                    
                    // Display Vulkan version
                    if (!TextUtils.isEmpty(vulkanVer)) {
                        String displayText = vulkanVer.startsWith("Vulkan ") ? vulkanVer : "Vulkan " + vulkanVer;
                        mVulkanVersion.setText(displayText);
                        mVulkanVersion.setVisibility(android.view.View.VISIBLE);
                    } else {
                        // Show driver version as fallback if available
                        String driverVer = metadata.getVersion();
                        if (!TextUtils.isEmpty(driverVer)) {
                            mVulkanVersion.setText("Driver v" + driverVer);
                            mVulkanVersion.setVisibility(android.view.View.VISIBLE);
                        } else {
                            mVulkanVersion.setVisibility(android.view.View.GONE);
                        }
                    }
                    
                    StringBuilder infoBuilder = new StringBuilder();
                    String vendor = metadata.getVendor();
                    String author = metadata.getAuthor();
                    
                    if (!TextUtils.isEmpty(vendor)) {
                        infoBuilder.append(vendor);
                    }
                    if (!TextUtils.isEmpty(author)) {
                        if (infoBuilder.length() > 0) {
                            infoBuilder.append(" • ");
                        }
                        infoBuilder.append(author);
                    }
                    
                    if (infoBuilder.length() > 0) {
                        mDriverInfo.setText(infoBuilder.toString());
                        mDriverInfo.setVisibility(android.view.View.VISIBLE);
                    } else {
                        mDriverInfo.setVisibility(android.view.View.GONE);
                    }
                    
                    // Display description
                    String desc = metadata.getDescription();
                    if (!TextUtils.isEmpty(desc)) {
                        mDescription.setText(desc);
                        mDescription.setVisibility(android.view.View.VISIBLE);
                    } else {
                        mDescription.setVisibility(android.view.View.GONE);
                    }
                } else {
                    if (isSystemDriver) {
                        mVulkanVersion.setText(R.string.gpu_driver_system_description);
                        mVulkanVersion.setVisibility(android.view.View.VISIBLE);
                        mDescription.setVisibility(android.view.View.GONE);
                    } else {
                        mVulkanVersion.setVisibility(android.view.View.GONE);
                        mDescription.setVisibility(android.view.View.GONE);
                    }
                }
                
                mRadioButton.setChecked(isSelected);
                mDeleteButton.setVisibility(isSystemDriver ? android.view.View.GONE : android.view.View.VISIBLE);
                mCardView.setOnClickListener(v -> selectDriver(driver));
                mRadioButton.setOnClickListener(v -> selectDriver(driver));
                mDeleteButton.setOnClickListener(v -> deleteDriver(driver));
            }
        }
    }
}
