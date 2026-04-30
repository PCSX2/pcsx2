
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

import android.content.Context;
import android.content.SharedPreferences;
import android.content.res.AssetManager;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.os.storage.StorageManager;
import android.os.storage.StorageVolume;
import android.text.TextUtils;

import androidx.annotation.Nullable;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.lang.reflect.Method;

public final class DataDirectoryManager {
    private static final String PREFS = "armsx2";
    private static final String KEY_CUSTOM_PATH = "data_dir_path";
    private static final String KEY_CUSTOM_URI = "data_dir_uri";
    private static final String KEY_PROMPT_DONE = "data_dir_prompt_done";
    private static final String TAG = "DataDirManager";

    private DataDirectoryManager() {}

    public static File getDataRoot(Context context) {
        SharedPreferences prefs = getPrefs(context);
        String custom = prefs.getString(KEY_CUSTOM_PATH, null);
        if (!TextUtils.isEmpty(custom)) {
            File dir = new File(custom);
            if (ensureDir(dir)) {
                return dir;
            }
        }
        File fallback = getDefaultDataRoot(context);
        ensureDir(fallback);
        return fallback;
    }

    @Nullable
    public static String resolveUriToPath(Context context, Uri uri) {
        if (uri == null) return null;
        String path = resolveTreeUriToPath(context, uri);
        if (path != null) return path;
        try {
            if (android.provider.DocumentsContract.isDocumentUri(context, uri)) {
                String docId = android.provider.DocumentsContract.getDocumentId(uri);
                if (docId.startsWith("raw:")) {
                    return docId.substring(4);
                }
                String[] split = docId.split(":");
                String type = split[0];
                String relPath = split.length > 1 ? split[1] : "";

                if ("primary".equalsIgnoreCase(type)) {
                    return android.os.Environment.getExternalStorageDirectory() + "/" + relPath;
                } else {
                    return "/storage/" + type + "/" + relPath;
                }
            }
        } catch (Exception ignored) {}

        // 3. Handle traditional file:// URIs
        if ("file".equalsIgnoreCase(uri.getScheme())) {
            return uri.getPath();
        }

        return null;
    }

    public static File getDefaultDataRoot(Context context) {
        File base = context.getExternalFilesDir(null);
        if (base == null) {
            base = context.getDataDir();
        }
        return base != null ? base : new File("");
    }

    public static boolean hasCustomDataRoot(Context context) {
        SharedPreferences prefs = getPrefs(context);
        return !TextUtils.isEmpty(prefs.getString(KEY_CUSTOM_PATH, null));
    }

    public static void storeCustomDataRoot(Context context, String absolutePath, @Nullable String uriString) {
        SharedPreferences.Editor editor = getPrefs(context).edit();
        editor.putString(KEY_CUSTOM_PATH, absolutePath);
        if (!TextUtils.isEmpty(uriString)) {
            editor.putString(KEY_CUSTOM_URI, uriString);
        }
        editor.apply();
        markPromptDone(context);
    }

    public static void clearCustomDataRoot(Context context) {
        getPrefs(context).edit().remove(KEY_CUSTOM_PATH).remove(KEY_CUSTOM_URI).apply();
    }

    public static boolean isPromptDone(Context context) {
        return getPrefs(context).getBoolean(KEY_PROMPT_DONE, false);
    }

    public static void markPromptDone(Context context) {
        getPrefs(context).edit().putBoolean(KEY_PROMPT_DONE, true).apply();
    }

    static void resetPrompt(Context context) {
        getPrefs(context).edit().remove(KEY_PROMPT_DONE).apply();
    }

    public static boolean migrateData(File source, File target) {
        if (source == null || target == null) {
            return false;
        }
        String sourcePath = source.getAbsolutePath();
        String targetPath = target.getAbsolutePath();
        if (TextUtils.isEmpty(sourcePath) || TextUtils.isEmpty(targetPath)) {
            return false;
        }
        if (sourcePath.equals(targetPath)) {
            return true;
        }
        if (targetPath.startsWith(sourcePath + File.separator)) {
            try { DebugLog.e(TAG, "Target is nested inside source: " + targetPath); } catch (Throwable ignored) {}
            return false;
        }
        if (!source.exists()) {
            return ensureDir(target);
        }
        if (!target.exists()) {
            File parent = target.getParentFile();
            if (parent != null && !parent.exists() && !parent.mkdirs()) {
                try { DebugLog.e(TAG, "Failed to create parent directory: " + parent); } catch (Throwable ignored) {}
                return false;
            }
            if (source.renameTo(target)) {
                try { DebugLog.d(TAG, "Renamed data root to " + targetPath); } catch (Throwable ignored) {}
                return true;
            }
        }
        if (!ensureDir(target)) {
            try { DebugLog.e(TAG, "Unable to ensure target directory: " + targetPath); } catch (Throwable ignored) {}
            return false;
        }
        if (!copyRecursively(source, target)) {
            try { DebugLog.e(TAG, "Recursive copy failed from " + sourcePath + " to " + targetPath); } catch (Throwable ignored) {}
            return false;
        }
        clearDirectory(source);
        return true;
    }

    @Nullable
    public static String resolveTreeUriToPath(Context context, Uri treeUri) {
        if (treeUri == null) {
            return null;
        }
        try {
            String docId = android.provider.DocumentsContract.getTreeDocumentId(treeUri);
            if (TextUtils.isEmpty(docId)) {
                return null;
            }
            if (docId.startsWith("raw:")) {
                return docId.substring(4);
            }
            String[] split = docId.split(":", 2);
            String type = split[0];
            String relPath = split.length > 1 ? split[1] : "";
            if ("primary".equalsIgnoreCase(type)) {
                File primary = Environment.getExternalStorageDirectory();
                return relPath.isEmpty() ? primary.getAbsolutePath() : new File(primary, relPath).getAbsolutePath();
            }
            if ("home".equalsIgnoreCase(type)) {
                File documents = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOCUMENTS);
                return relPath.isEmpty() ? documents.getAbsolutePath() : new File(documents, relPath).getAbsolutePath();
            }
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
                StorageManager sm = (StorageManager) context.getSystemService(Context.STORAGE_SERVICE);
                if (sm != null) {
                    for (StorageVolume volume : sm.getStorageVolumes()) {
                        boolean match = volume.isPrimary() && "primary".equalsIgnoreCase(type);
                        String uuid = volume.getUuid();
                        if (!match && uuid != null) {
                            match = uuid.equalsIgnoreCase(type);
                        }
                        if (match) {
                            File dir = getVolumeDirectory(volume);
                            if (dir != null) {
                                return relPath.isEmpty() ? dir.getAbsolutePath() : new File(dir, relPath).getAbsolutePath();
                            }
                        }
                    }
                }
            }
            File storageRoot = new File("/storage/" + type);
            if (storageRoot.exists()) {
                return relPath.isEmpty() ? storageRoot.getAbsolutePath() : new File(storageRoot, relPath).getAbsolutePath();
            }
        } catch (Exception ignored) {
        }
        return null;
    }

    private static SharedPreferences getPrefs(Context context) {
        return context.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
    }

    private static boolean ensureDir(@Nullable File dir) {
        if (dir == null) {
            return false;
        }
        if (dir.exists()) {
            return dir.isDirectory();
        }
        return dir.mkdirs();
    }

    private static boolean copyRecursively(File source, File target) {
        if (source.isDirectory()) {
            if (!target.exists() && !target.mkdirs()) {
                try { DebugLog.e(TAG, "Failed to create directory: " + target); } catch (Throwable ignored) {}
                return false;
            }
            File[] children = source.listFiles();
            if (children == null) {
                try { DebugLog.e(TAG, "Cannot list directory contents: " + source); } catch (Throwable ignored) {}
                return false;
            }
            for (File child : children) {
                if (!copyRecursively(child, new File(target, child.getName()))) {
                    return false;
                }
            }
            return true;
        }
        File parent = target.getParentFile();
        if (parent != null && !parent.exists() && !parent.mkdirs()) {
            try { DebugLog.e(TAG, "Failed to create parent for file: " + parent); } catch (Throwable ignored) {}
            return false;
        }
        try (InputStream in = new FileInputStream(source);
             OutputStream out = new FileOutputStream(target)) {
            byte[] buffer = new byte[1024 * 1024];
            int read;
            while ((read = in.read(buffer)) != -1) {
                out.write(buffer, 0, read);
            }
            out.flush();
        } catch (IOException e) {
            try { DebugLog.e(TAG, "Copy failed for " + source + " -> " + target + ": " + e.getMessage()); } catch (Throwable ignored) {}
            return false;
        }
        return true;
    }

    private static void clearDirectory(File dir) {
        if (dir == null || !dir.exists() || !dir.isDirectory()) {
            return;
        }
        File[] children = dir.listFiles();
        if (children == null) {
            return;
        }
        for (File child : children) {
            deleteRecursively(child);
        }
    }

    private static void deleteRecursively(File file) {
        if (file == null || !file.exists()) {
            return;
        }
        if (file.isDirectory()) {
            File[] children = file.listFiles();
            if (children != null) {
                for (File child : children) {
                    deleteRecursively(child);
                }
            }
        }
        file.delete();
    }

    public static void copyAssetAll(Context context, String srcPath) {
        AssetManager assetMgr = context.getAssets();
        try {
            String[] assets = assetMgr.list(srcPath);
            if (assets == null) {
                try { DebugLog.e(TAG, "Asset list returned null for " + srcPath); } catch (Throwable ignored) {}
                return;
            }
            File destPath = new File(getDataRoot(context), srcPath);
            if (assets.length == 0) {
                if (!copyFile(context, srcPath, destPath.getAbsolutePath())) {
                    try { DebugLog.e(TAG, "Failed to copy asset file " + srcPath + " to " + destPath); } catch (Throwable ignored) {}
                }
            } else {
                if (!destPath.exists()) {
                    if (!destPath.mkdirs()) {
                        try { DebugLog.e(TAG, "Failed to create destination directory for assets: " + destPath); } catch (Throwable ignored) {}
                        return;
                    }
                }
                for (String element : assets) {
                    copyAssetAll(context, srcPath + File.separator + element);
                }
            }
        } catch (IOException ignored) {
            try { DebugLog.e(TAG, "IOException while copying assets: " + ignored.getMessage()); } catch (Throwable ignored2) {}
        }
    }

    static boolean copyFile(Context context, String srcFile, String destFile) {
        AssetManager assetMgr = context.getAssets();
        InputStream is = null;
        FileOutputStream os = null;
        boolean success = false;
        try {
            is = assetMgr.open(srcFile);
            File outFile = new File(destFile);
            File parent = outFile.getParentFile();
            if (parent != null && !parent.exists()) {
                if (!parent.mkdirs()) {
                    try { DebugLog.e(TAG, "Failed to create parent for asset: " + parent); } catch (Throwable ignored) {}
                    return false;
                }
            }
            boolean exists = outFile.exists();
            if (srcFile.contains("shaders")) {
                exists = false;
            }
            if (!exists) {
                os = new FileOutputStream(outFile);
                byte[] buffer = new byte[1024];
                int read;
                while ((read = is.read(buffer)) != -1) {
                    os.write(buffer, 0, read);
                }
                os.flush();
            }
            success = true;
        } catch (IOException ignored) {
            try { DebugLog.e(TAG, "Failed to copy asset " + srcFile + " -> " + destFile + ": " + ignored.getMessage()); } catch (Throwable ignored2) {}
            success = false;
        } finally {
            if (is != null) {
                try { is.close(); } catch (IOException ignored) {}
            }
            if (os != null) {
                try { os.close(); } catch (IOException ignored) {}
            }
        }
        return success;
    }

    public static boolean canUseDirectFileAccess(File dir) {
        if (dir == null) {
            return false;
        }
        if (!dir.exists() && !dir.mkdirs()) {
            try { DebugLog.e(TAG, "Unable to create directory for access probe: " + dir); } catch (Throwable ignored) {}
            return false;
        }
        File probe = new File(dir, ".armsx2_write_probe");
        FileOutputStream fos = null;
        try {
            fos = new FileOutputStream(probe);
            fos.write(0x41);
            fos.flush();
            return true;
        } catch (IOException e) {
            try { DebugLog.e(TAG, "Write probe failed for " + dir + ": " + e.getMessage()); } catch (Throwable ignored) {}
            return false;
        } finally {
            if (fos != null) {
                try { fos.close(); } catch (IOException ignored) {}
            }
            if (probe.exists() && !probe.delete()) {
                try { DebugLog.d(TAG, "Failed to delete probe file " + probe); } catch (Throwable ignored) {}
            }
        }
    }

    public static boolean hasAllFilesAccess() {
        return Build.VERSION.SDK_INT < Build.VERSION_CODES.R || Environment.isExternalStorageManager();
    }

    @Nullable
    private static File getVolumeDirectory(StorageVolume volume) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            File dir = volume.getDirectory();
            if (dir != null) {
                return dir;
            }
        }
        try {
            Method getPath = StorageVolume.class.getDeclaredMethod("getPath");
            getPath.setAccessible(true);
            String path = (String) getPath.invoke(volume);
            if (!TextUtils.isEmpty(path)) {
                return new File(path);
            }
        } catch (Exception ignored) {
        }
        return null;
    }
}
