
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

package kr.co.iefriends.pcsx2;

import android.app.Activity;
import android.app.Application;
import android.os.Bundle;
import android.content.Context;
import java.util.concurrent.atomic.AtomicBoolean;

import androidx.annotation.NonNull;

import com.google.android.material.color.DynamicColors;

import kr.co.iefriends.pcsx2.utils.DiscordBridge;

public class App extends Application {
    private static Context appContext;
    private static final AtomicBoolean sBootSplashPlayed = new AtomicBoolean(false);

    public static Context getContext() {
        return appContext;
    }

    public static boolean consumeBootSplashPlayToken() {
        return sBootSplashPlayed.compareAndSet(false, true);
    }

    @Override
    public void onCreate() {
        super.onCreate();

        // Save global application context
        appContext = getApplicationContext();

        // Apply Material You !
        DynamicColors.applyToActivitiesIfAvailable(this);
        DiscordBridge.initialize(this);
        registerActivityLifecycleCallbacks(new ActivityLifecycleCallbacks() {
            @Override
            public void onActivityCreated(@NonNull Activity activity, Bundle savedInstanceState) {
            }

            @Override
            public void onActivityStarted(@NonNull Activity activity) {
            }

            @Override
            public void onActivityResumed(@NonNull Activity activity) {
                DiscordBridge.onActivityResumed(activity);
            }

            @Override
            public void onActivityPaused(@NonNull Activity activity) {
                DiscordBridge.onActivityPaused(activity);
            }

            @Override
            public void onActivityStopped(@NonNull Activity activity) {
            }

            @Override
            public void onActivitySaveInstanceState(@NonNull Activity activity, @NonNull Bundle outState) {
            }

            @Override
            public void onActivityDestroyed(@NonNull Activity activity) {
            }
        });
    }
}
