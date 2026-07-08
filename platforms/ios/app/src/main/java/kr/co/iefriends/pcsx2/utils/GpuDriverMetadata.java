// SPDX-FileCopyrightText: 2025 ARMSX2 Team (SternXD)
// SPDX-License-Identifier: GPL-3.0+

package kr.co.iefriends.pcsx2.utils;

import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;

/**
 * GPU Driver metadata parsed from meta.json in driver ZIP files
 */
public class GpuDriverMetadata {
    private static final String META_JSON_FILENAME = "meta.json";
    
    @Nullable
    private String mName;
    
    @Nullable
    private String mLibraryName;
    
    private int mMinApi = 0;
    
    @Nullable
    private String mDescription;
    
    @Nullable
    private String mVersion;
    
    @Nullable
    private String mVulkanVersion;
    
    @Nullable
    private String mAuthor;
    
    @Nullable
    private String mVendor;
    
    public GpuDriverMetadata() {
        // don't need to do anything here for now
    }
    
    public GpuDriverMetadata(@NonNull InputStream inputStream, long size) {
        try {
            byte[] buffer = new byte[(int) size];
            int totalRead = 0;
            while (totalRead < size) {
                int read = inputStream.read(buffer, totalRead, (int) size - totalRead);
                if (read == -1) {
                    break;
                }
                totalRead += read;
            }
            
            String jsonString = new String(buffer, 0, totalRead, "UTF-8");
            parseJson(jsonString);
        } catch (IOException | JSONException e) {
            // Invalid metadata, leave fields as null
        }
    }
    
    public GpuDriverMetadata(@NonNull File metaJsonFile) {
        if (!metaJsonFile.exists() || !metaJsonFile.isFile()) {
            return;
        }
        
        try (FileInputStream fis = new FileInputStream(metaJsonFile)) {
            byte[] buffer = new byte[(int) metaJsonFile.length()];
            int totalRead = 0;
            while (totalRead < buffer.length) {
                int read = fis.read(buffer, totalRead, buffer.length - totalRead);
                if (read == -1) {
                    break;
                }
                totalRead += read;
            }
            
            String jsonString = new String(buffer, 0, totalRead, "UTF-8");
            parseJson(jsonString);
        } catch (IOException | JSONException e) {
            // Invalid metadata, leave fields as null
        }
    }
    
    void parseJson(@NonNull String jsonString) throws JSONException {
        JSONObject json = new JSONObject(jsonString);
        
        if (json.has("name")) {
            mName = json.getString("name");
        }
        
        if (json.has("library_name")) {
            mLibraryName = json.getString("library_name");
        } else if (json.has("libraryName")) {
            mLibraryName = json.getString("libraryName");
        }
        
        if (json.has("min_api")) {
            mMinApi = json.getInt("min_api");
        } else if (json.has("minApi")) {
            mMinApi = json.getInt("minApi");
        }
        
        if (json.has("description")) {
            mDescription = json.getString("description");
        }
        
        if (json.has("version")) {
            mVersion = json.getString("version");
        }
        
        if (json.has("driverVersion")) {
            String driverVer = json.getString("driverVersion");
            mVersion = driverVer;
            
            if (driverVer.toLowerCase().contains("vulkan")) {
                String vulkanPart = driverVer.replaceAll("(?i).*vulkan\\s*", "").trim();
                if (!vulkanPart.isEmpty()) {
                    mVulkanVersion = vulkanPart;
                } else {
                    mVulkanVersion = driverVer;
                }
            } else if (driverVer.matches("^\\d+\\.\\d+\\.\\d+.*$")) {
                mVulkanVersion = driverVer;
            }
        }
        
        if (TextUtils.isEmpty(mVulkanVersion)) {
            if (json.has("vulkan_version")) {
                mVulkanVersion = json.getString("vulkan_version");
            } else if (json.has("vulkanVersion")) {
                mVulkanVersion = json.getString("vulkanVersion");
            } else if (json.has("vulkan")) {
                mVulkanVersion = json.getString("vulkan");
            } else if (json.has("api_version")) {
                mVulkanVersion = json.getString("api_version");
            } else if (json.has("apiVersion")) {
                mVulkanVersion = json.getString("apiVersion");
            } else if (json.has("vulkanApiVersion")) {
                mVulkanVersion = json.getString("vulkanApiVersion");
            }
        }
        
        if (TextUtils.isEmpty(mVulkanVersion) && !TextUtils.isEmpty(mVersion)) {
            if (mVersion.matches("^\\d+\\.\\d+\\.\\d+.*$")) {
                mVulkanVersion = mVersion;
            }
        }
        
        // Parse author and vendor
        if (json.has("author")) {
            mAuthor = json.getString("author");
        }
        
        if (json.has("vendor")) {
            mVendor = json.getString("vendor");
        }
    }
    
    @Nullable
    public String getName() {
        return mName;
    }
    
    @Nullable
    public String getLibraryName() {
        return mLibraryName;
    }
    
    public int getMinApi() {
        return mMinApi;
    }
    
    @Nullable
    public String getDescription() {
        return mDescription;
    }
    
    @Nullable
    public String getVersion() {
        return mVersion;
    }
    
    public boolean isValid() {
        return !TextUtils.isEmpty(mName) && !TextUtils.isEmpty(mLibraryName);
    }
    
    @NonNull
    public String getDisplayName() {
        if (!TextUtils.isEmpty(mName)) {
            return mName;
        }
        return "Unknown Driver";
    }
    
    @Nullable
    public String getVulkanVersion() {
        return mVulkanVersion;
    }
    
    @Nullable
    public String getAuthor() {
        return mAuthor;
    }
    
    @Nullable
    public String getVendor() {
        return mVendor;
    }
}

