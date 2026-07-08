
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

import android.util.Log;
import kr.co.iefriends.pcsx2.BuildConfig;

public class DebugLog {

    private static final boolean ENABLED;

    static {
        boolean enabled;
        try {
            enabled = BuildConfig.LOGS_ENABLED;
        } catch (Throwable t) {
            enabled = Log.isLoggable("ARMSX2_DEBUG", Log.VERBOSE);
        }
        ENABLED = enabled;
    }

    public static int d(String tag, String msg) { return ENABLED ? Log.d(tag, msg) : 0; }
    public static int w(String tag, String msg) { return ENABLED ? Log.w(tag, msg) : 0; }
    public static int e(String tag, String msg) { return ENABLED ? Log.e(tag, msg) : 0; }
    public static int e(String tag, String msg, Throwable tr) { return ENABLED ? Log.e(tag, msg, tr) : 0; }
    public static int i(String tag, String msg) { return ENABLED ? Log.i(tag, msg) : 0; }
    public static int v(String tag, String msg) { return ENABLED ? Log.v(tag, msg) : 0; }
}
