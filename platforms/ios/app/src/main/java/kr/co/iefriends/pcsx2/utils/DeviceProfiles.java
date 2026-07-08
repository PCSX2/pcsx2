
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

import android.app.UiModeManager;
import android.content.Context;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.os.Build;
import android.text.TextUtils;

import kr.co.iefriends.pcsx2.R;

public final class DeviceProfiles {
    private static final String FEATURE_SAMSUNG_DEX = "com.samsung.android.feature.SEM_DESKTOP_MODE";
    private static final String FEATURE_SAMSUNG_DEX_ALT = "com.samsung.android.feature.CONTROL_DEX";
    private static final String FEATURE_GOOGLE_DESKTOP = PackageManager.FEATURE_PC;
    private static final String FEATURE_CHROME_OS = "org.chromium.arc";

    private DeviceProfiles() {}

    public static boolean isAndroidTV(Context context) {
        if (context == null) return false;
        UiModeManager uiModeManager = (UiModeManager) context.getSystemService(Context.UI_MODE_SERVICE);
        if (uiModeManager == null) return false;
        int mode = uiModeManager.getCurrentModeType();
        return mode == Configuration.UI_MODE_TYPE_TELEVISION || mode == Configuration.UI_MODE_TYPE_APPLIANCE;
    }

    public static boolean isSamsungDex(Context context) {
        if (context == null) return false;
        PackageManager pm = context.getPackageManager();
        if (pm == null) return false;
        boolean hasDexFeature = pm.hasSystemFeature(FEATURE_SAMSUNG_DEX) || pm.hasSystemFeature(FEATURE_SAMSUNG_DEX_ALT);
        if (!hasDexFeature) return false;
        // Some Samsung devices report the feature even when Dex is not active. If the manufacturer isn't Samsung, bail out.
        return TextUtils.equals(Build.MANUFACTURER, "samsung") || TextUtils.equals(Build.BRAND, "samsung");
    }

    public static boolean isChromebook(Context context) {
        if (context == null) return false;
        PackageManager pm = context.getPackageManager();
        if (pm == null) return false;
        return pm.hasSystemFeature(FEATURE_GOOGLE_DESKTOP) || pm.hasSystemFeature(FEATURE_CHROME_OS);
    }

    public static boolean isDesktopExperience(Context context) {
        return isSamsungDex(context) || isChromebook(context);
    }

    public static boolean isTvOrDesktop(Context context) {
        return isAndroidTV(context) || isDesktopExperience(context);
    }

    public static boolean isTouchOptimized(Context context) {
        return !isTvOrDesktop(context);
    }

    public static String getProductDisplayName(Context context, String defaultName) {
        if (context == null) {
            return defaultName;
        }
        if (isAndroidTV(context)) {
            return context.getString(R.string.app_name_tv);
        }
        if (isDesktopExperience(context)) {
            return context.getString(R.string.app_name_desktop);
        }
        return defaultName;
    }
}
