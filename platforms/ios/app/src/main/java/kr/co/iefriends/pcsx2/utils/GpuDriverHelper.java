// SPDX-FileCopyrightText: 2025 ARMSX2 Team (SternXD)
// SPDX-License-Identifier: GPL-3.0+

package kr.co.iefriends.pcsx2.utils;

import android.content.Context;
import android.net.Uri;
import android.os.Build;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import java.io.BufferedInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

import kr.co.iefriends.pcsx2.NativeApp;

public final class GpuDriverHelper {
    private static final String TAG = "GpuDriverHelper";
    private static final String META_JSON_FILENAME = "meta.json";
    private static final String DRIVERS_STORAGE_DIR_NAME = "gpu_drivers";
    private static final String DRIVERS_INSTALL_DIR_NAME = "gpu_driver";
    
    private final Context mContext;
    private final File mDriversStorageDir;  // Where ZIP files are stored
    private final File mDriversInstallDir;  // Where drivers are extracted/installed
    
    public GpuDriverHelper(@NonNull Context context) {
        mContext = context.getApplicationContext();
        mDriversStorageDir = new File(mContext.getFilesDir(), DRIVERS_STORAGE_DIR_NAME);
        mDriversInstallDir = new File(mContext.getFilesDir(), DRIVERS_INSTALL_DIR_NAME);
        
        if (!mDriversStorageDir.exists()) {
            mDriversStorageDir.mkdirs();
        }
        if (!mDriversInstallDir.exists()) {
            mDriversInstallDir.mkdirs();
        }
    }
    
    /**
     * Get the directory where GPU driver ZIP files are stored
     */
    @NonNull
    public File getDriversStorageDirectory() {
        return mDriversStorageDir;
    }
    
    /**
     * Get the directory where GPU drivers are installed/extracted
     */
    @NonNull
    public File getDriversInstallDirectory() {
        return mDriversInstallDir;
    }
    
    /**
     * Get the currently selected custom driver path
     */
    @Nullable
    public String getCustomDriverPath() {
        return NativeApp.getCustomDriverPath();
    }
    
    /**
     * Set the custom driver path
     */
    public void setCustomDriverPath(@Nullable String path) {
        if (TextUtils.isEmpty(path)) {
            NativeApp.setCustomDriverPath("");
        } else {
            NativeApp.setCustomDriverPath(path);
        }
    }
    
    /**
     * Get list of available GPU drivers from ZIP files
     */
    @NonNull
    public List<GpuDriver> getAvailableDrivers() {
        List<GpuDriver> drivers = new ArrayList<>();
        
        if (!mDriversStorageDir.exists() || !mDriversStorageDir.isDirectory()) {
            return drivers;
        }
        
        File[] files = mDriversStorageDir.listFiles();
        if (files == null) {
            return drivers;
        }
        
        for (File file : files) {
            if (file.isFile() && file.getName().endsWith(".zip")) {
                try {
                    GpuDriverMetadata metadata = getMetadataFromZip(new FileInputStream(file));
                    if (metadata.isValid()) {
                        GpuDriver driver = new GpuDriver(
                            metadata.getDisplayName(),
                            file.getAbsolutePath(),
                            file.length(),
                            file.lastModified(),
                            metadata
                        );
                        drivers.add(driver);
                    }
                } catch (IOException e) {
                    // Skip invalid ZIP files
                }
            }
        }
        
        // Sort by name
        Collections.sort(drivers, (a, b) -> a.getName().compareToIgnoreCase(b.getName()));
        
        return drivers;
    }
    
    /**
     * Get metadata from a driver ZIP file
     */
    @NonNull
    public GpuDriverMetadata getMetadataFromZip(@NonNull InputStream zipInputStream) {
        try (ZipInputStream zis = new ZipInputStream(new BufferedInputStream(zipInputStream))) {
            ZipEntry entry;
            while ((entry = zis.getNextEntry()) != null) {
                String entryName = entry.getName();
                if (!entry.isDirectory() && entryName.toLowerCase().endsWith("meta.json")) {
                    ByteArrayOutputStream baos = new ByteArrayOutputStream();
                    byte[] buffer = new byte[8192];
                    int read;
                    while ((read = zis.read(buffer)) != -1) {
                        baos.write(buffer, 0, read);
                    }
                    byte[] data = baos.toByteArray();
                    if (data.length > 0 && data.length < 1024 * 1024) {
                        String jsonString = new String(data, "UTF-8");
                        GpuDriverMetadata metadata = new GpuDriverMetadata();
                        try {
                            metadata.parseJson(jsonString);
                            return metadata;
                        } catch (org.json.JSONException e) {
                            // Invalid JSON, continue searching
                        }
                    }
                }
                zis.closeEntry();
            }
        } catch (IOException e) {
            // Ignore
        }
        return new GpuDriverMetadata();
    }
    
    /**
     * Install a driver from a ZIP file URI
     */
    public boolean installDriverFromUri(@NonNull Uri driverUri) {
        try (InputStream inputStream = mContext.getContentResolver().openInputStream(driverUri)) {
            if (inputStream == null) {
                return false;
            }
            
            // First, validate the driver
            GpuDriverMetadata metadata = getMetadataFromZip(new BufferedInputStream(inputStream));
            if (!metadata.isValid()) {
                return false;
            }
            
            // Check minimum API level
            if (metadata.getMinApi() > Build.VERSION.SDK_INT) {
                return false;
            }
            
            // Copy ZIP to storage directory
            String fileName = getFileNameFromUri(driverUri);
            if (TextUtils.isEmpty(fileName) || !fileName.endsWith(".zip")) {
                fileName = metadata.getName() != null ? metadata.getName() + ".zip" : "driver.zip";
            }
            
            File storageFile = new File(mDriversStorageDir, fileName);
            try (InputStream in = mContext.getContentResolver().openInputStream(driverUri);
                 FileOutputStream out = new FileOutputStream(storageFile)) {
                byte[] buffer = new byte[8192];
                int length;
                while ((length = in.read(buffer)) > 0) {
                    out.write(buffer, 0, length);
                }
            }
            
            // Extract the driver
            return installCustomDriverComplete(storageFile);
            
        } catch (Exception e) {
            return false;
        }
    }
    
    /**
     * Install a driver from a ZIP file
     */
    public boolean installCustomDriverComplete(@NonNull File zipFile) {
        // Revert to system default first
        installDefaultDriver();
        
        // Ensure directories exist
        initializeDirectories();
        
        // Validate driver
        try {
            GpuDriverMetadata metadata = getMetadataFromZip(new FileInputStream(zipFile));
            if (!metadata.isValid()) {
                return false;
            }
            
            if (metadata.getMinApi() > Build.VERSION.SDK_INT) {
                return false;
            }
            
            // Extract the driver
            unzipToDirectory(zipFile, mDriversInstallDir);
            
            // Initialize driver parameters
            initializeDriverParameters(metadata);
            
            return true;
        } catch (Exception e) {
            return false;
        }
    }
    
    /**
     * Install default/system driver (removes custom driver)
     */
    public void installDefaultDriver() {
        // Remove installed driver files
        if (mDriversInstallDir.exists()) {
            deleteDirectory(mDriversInstallDir);
            mDriversInstallDir.mkdirs();
        }
        
        // Clear custom driver path
        setCustomDriverPath(null);
    }
    
    /**
     * Initialize driver parameters after installation
     */
    private void initializeDriverParameters(@NonNull GpuDriverMetadata metadata) {
        String libraryName = metadata.getLibraryName();
        if (!TextUtils.isEmpty(libraryName)) {
            // Find the library file in the install directory
            File libFile = findLibraryFile(libraryName);
            if (libFile != null && libFile.exists()) {
                setCustomDriverPath(libFile.getAbsolutePath());
            }
        }
    }
    
    /**
     * Find the library file in the install directory
     */
    @Nullable
    private File findLibraryFile(@NonNull String libraryName) {
        // Try exact name first
        File libFile = new File(mDriversInstallDir, libraryName);
        if (libFile.exists()) {
            return libFile;
        }
        
        // Try with .so extension if not present
        if (!libraryName.endsWith(".so")) {
            libFile = new File(mDriversInstallDir, libraryName + ".so");
            if (libFile.exists()) {
                return libFile;
            }
        }
        
        // Search for any .so file
        File[] files = mDriversInstallDir.listFiles();
        if (files != null) {
            for (File file : files) {
                if (file.isFile() && file.getName().endsWith(".so")) {
                    return file;
                }
            }
        }
        
        return null;
    }
    
    /**
     * Extract ZIP file to directory
     */
    private void unzipToDirectory(@NonNull File zipFile, @NonNull File destDir) throws IOException {
        try (ZipInputStream zis = new ZipInputStream(new BufferedInputStream(new FileInputStream(zipFile)))) {
            byte[] buffer = new byte[8192];
            ZipEntry entry;
            
            while ((entry = zis.getNextEntry()) != null) {
                File outFile = new File(destDir, entry.getName());
                
                if (!outFile.getCanonicalPath().startsWith(destDir.getCanonicalPath())) {
                    zis.closeEntry();
                    continue;
                }
                
                if (entry.isDirectory()) {
                    if (!outFile.exists() && !outFile.mkdirs()) {
                        throw new IOException("Failed to create directory: " + outFile);
                    }
                } else {
                    File parent = outFile.getParentFile();
                    if (parent != null && !parent.exists() && !parent.mkdirs()) {
                        throw new IOException("Failed to create parent directory: " + parent);
                    }
                    
                    try (OutputStream out = new FileOutputStream(outFile)) {
                        int count;
                        while ((count = zis.read(buffer)) != -1) {
                            out.write(buffer, 0, count);
                        }
                    }
                    
                    // Make executable if it's a .so file
                    if (outFile.getName().endsWith(".so")) {
                        outFile.setExecutable(true, false);
                        outFile.setReadable(true, false);
                    }
                }
                zis.closeEntry();
            }
        }
    }
    
    /**
     * Delete a driver ZIP file
     */
    public boolean deleteDriver(@NonNull String driverPath) {
        File driverFile = new File(driverPath);
        if (driverFile.exists() && driverFile.getParentFile().equals(mDriversStorageDir)) {
            // Only allow deletion of files in the storage directory
            return driverFile.delete();
        }
        return false;
    }
    
    /**
     * Check if a driver ZIP is currently selected
     * @param zipFilePath Path to the driver ZIP file
     * @return true if the driver from this ZIP is currently selected
     */
    public boolean isDriverSelected(@NonNull String zipFilePath) {
        String currentPath = getCustomDriverPath();
        if (TextUtils.isEmpty(currentPath)) {
            return false;
        }
        
        // Check if the currently selected driver path is in the install directory
        File currentDriverFile = new File(currentPath);
        if (!currentDriverFile.exists() || !currentDriverFile.getParentFile().equals(mDriversInstallDir)) {
            // Selected driver is not from our install directory, so this ZIP is not selected
            return false;
        }
        
        // Get metadata from the ZIP file to find the expected library name
        File zipFile = new File(zipFilePath);
        if (!zipFile.exists() || !zipFile.isFile()) {
            return false;
        }
        
        try (FileInputStream zipInputStream = new FileInputStream(zipFile)) {
            GpuDriverMetadata metadata = getMetadataFromZip(zipInputStream);
            if (!metadata.isValid() || TextUtils.isEmpty(metadata.getLibraryName())) {
                return false;
            }
            
            // Find the expected library file path for this ZIP's driver
            File expectedLibFile = findLibraryFile(metadata.getLibraryName());
            if (expectedLibFile == null || !expectedLibFile.exists()) {
                return false;
            }

            String expectedPath = expectedLibFile.getCanonicalPath();
            String currentCanonicalPath = currentDriverFile.getCanonicalPath();
            return expectedPath.equals(currentCanonicalPath);
            
        } catch (Exception e) {
            try {
                String fileName = currentDriverFile.getName();
                File zipFileCheck = new File(zipFilePath);
                if (zipFileCheck.exists()) {
                    try (FileInputStream zipInputStream = new FileInputStream(zipFileCheck)) {
                        GpuDriverMetadata metadata = getMetadataFromZip(zipInputStream);
                        if (metadata.isValid() && !TextUtils.isEmpty(metadata.getLibraryName())) {
                            String expectedName = metadata.getLibraryName();
                            if (!expectedName.endsWith(".so")) {
                                expectedName = expectedName + ".so";
                            }
                            return fileName.equals(expectedName);
                        }
                    }
                }
            } catch (Exception ignored) {
                // Fallback failed, return false
            }
            return false;
        }
    }
    
    /**
     * Get system driver info (default/system driver)
     */
    @NonNull
    public GpuDriver getSystemDriver() {
        return new GpuDriver(
            "System Driver",
            "",
            0,
            0,
            null
        );
    }
    
    /**
     * Initialize directories
     */
    private void initializeDirectories() {
        if (!mDriversStorageDir.exists()) {
            mDriversStorageDir.mkdirs();
        }
        if (!mDriversInstallDir.exists()) {
            mDriversInstallDir.mkdirs();
        }
    }
    
    /**
     * Set the native library directory
     */
    public void setNativeLibraryDir(@NonNull String path) {
        NativeApp.setNativeLibraryDir(path);
    }
    
    @Nullable
    private String getFileNameFromUri(@NonNull Uri uri) {
        String result = null;
        if ("content".equals(uri.getScheme())) {
            try (android.database.Cursor cursor = mContext.getContentResolver().query(uri, null, null, null, null)) {
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
    
    private void deleteDirectory(@NonNull File directory) {
        if (directory.exists()) {
            File[] files = directory.listFiles();
            if (files != null) {
                for (File file : files) {
                    if (file.isDirectory()) {
                        deleteDirectory(file);
                    } else {
                        file.delete();
                    }
                }
            }
            directory.delete();
        }
    }
    
    /**
     * GPU Driver information class
     */
    public static class GpuDriver {
        private final String mName;
        private final String mPath;
        private final long mSize;
        private final long mLastModified;
        @Nullable
        private final GpuDriverMetadata mMetadata;
        
        public GpuDriver(@NonNull String name, @NonNull String path, long size, long lastModified, @Nullable GpuDriverMetadata metadata) {
            mName = name;
            mPath = path;
            mSize = size;
            mLastModified = lastModified;
            mMetadata = metadata;
        }
        
        @NonNull
        public String getName() {
            return mName;
        }
        
        @NonNull
        public String getPath() {
            return mPath;
        }
        
        public long getSize() {
            return mSize;
        }
        
        public long getLastModified() {
            return mLastModified;
        }
        
        @Nullable
        public GpuDriverMetadata getMetadata() {
            return mMetadata;
        }
        
        public boolean isSystemDriver() {
            return TextUtils.isEmpty(mPath);
        }
        
        @NonNull
        public String getFormattedSize() {
            if (mSize < 1024) {
                return mSize + " B";
            } else if (mSize < 1024 * 1024) {
                return String.format("%.2f KB", mSize / 1024.0);
            } else {
                return String.format("%.2f MB", mSize / (1024.0 * 1024.0));
            }
        }
    }
}
