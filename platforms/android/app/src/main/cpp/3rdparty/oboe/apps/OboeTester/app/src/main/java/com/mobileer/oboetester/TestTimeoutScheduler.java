/*
 * Copyright 2025 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.mobileer.oboetester;

import android.app.AlarmManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.SystemClock;
import android.provider.Settings;
import android.util.Log;
import android.widget.Toast;

public class TestTimeoutScheduler {

    private static final int REQUEST_CODE = 54321;
    private static final String TAG = "TestTimeoutScheduler";

    public void scheduleTestTimeout(Context context, int durationSeconds) {
        Log.d(TAG, "Attempting to schedule test timeout.");
        AlarmManager alarmManager = (AlarmManager) context.getSystemService(Context.ALARM_SERVICE);
        if (alarmManager == null) {
            Log.e(TAG, "AlarmManager is null.");
            return;
        }

        // On Android 12 (S) and higher, we need to check for the permission.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            if (!alarmManager.canScheduleExactAlarms()) {
                Log.w(TAG, "Missing SCHEDULE_EXACT_ALARM permission.");
                Toast.makeText(context, "Permission needed to schedule test timeout", Toast.LENGTH_LONG).show();
                // Guide the user to the settings screen to grant the permission.
                Intent intent = new Intent(Settings.ACTION_REQUEST_SCHEDULE_EXACT_ALARM);
                context.startActivity(intent);
                return; // Stop execution until the permission is granted.
            }
        }

        Intent intent = new Intent(context, TestTimeoutReceiver.class);
        PendingIntent pendingIntent = PendingIntent.getBroadcast(
                context,
                REQUEST_CODE,
                intent,
                PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE
        );

        long triggerAtMillis = SystemClock.elapsedRealtime() + durationSeconds * 1000L;

        try {
            alarmManager.setExactAndAllowWhileIdle(AlarmManager.ELAPSED_REALTIME_WAKEUP, triggerAtMillis, pendingIntent);
            Log.i(TAG, "Exact alarm scheduled successfully for " + durationSeconds + " seconds.");
        } catch (SecurityException se) {
            // This catch is now a fallback, as the check above should prevent this.
            Log.e(TAG, "SecurityException while setting alarm. This should not happen after the check.", se);
        }
    }

    public void cancelTestTimeout(Context context) {
        AlarmManager alarmManager = (AlarmManager) context.getSystemService(Context.ALARM_SERVICE);
        if (alarmManager == null) {
            Log.e(TAG, "AlarmManager is null when trying to cancel.");
            return;
        }

        Intent intent = new Intent(context, TestTimeoutReceiver.class);
        PendingIntent pendingIntent = PendingIntent.getBroadcast(
                context,
                REQUEST_CODE,
                intent,
                PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE
        );

        alarmManager.cancel(pendingIntent);
        Log.i(TAG, "Canceled test timeout alarm.");
    }
}
