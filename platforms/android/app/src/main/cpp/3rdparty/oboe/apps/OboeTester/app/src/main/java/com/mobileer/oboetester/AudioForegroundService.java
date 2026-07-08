/*
 * Copyright 2024 The Android Open Source Project
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

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ServiceInfo;
import android.os.Build;
import android.os.IBinder;
import android.util.Log;

import java.util.HashSet;

public class AudioForegroundService extends Service {
    private static final String TAG = "OboeTester";
    public static final String ACTION_START = "ACTION_START";
    public static final String ACTION_STOP = "ACTION_STOP";
    public static final String KEY_ACTIVITY_ID = "KEY_ACTIVITY_ID";

    private int mServiceType;
    private final HashSet<Long> mActivityIds = new HashSet<>();

    @Override
    public IBinder onBind(Intent intent) {
        // We don't provide binding, so return null
        return null;
    }

    private Notification buildNotification() {
        NotificationManager manager = (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            manager.createNotificationChannel(new NotificationChannel(
                    "all",
                    "All Notifications",
                    NotificationManager.IMPORTANCE_NONE));

            return new Notification.Builder(this, "all")
                    .setContentTitle("Playing/recording audio")
                    .setContentText("playing/recording...")
                    .setSmallIcon(R.drawable.ic_notification)
                    .build();
        }
        return null;
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        Log.i(TAG, "Receive onStartCommand" + intent);
        switch (intent.getAction()) {
            case ACTION_START:
                Log.i(TAG, "Receive ACTION_START " + intent.getExtras());
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                    int serviceTypes = intent.getIntExtra("service_types", 0);
                    Log.i(TAG, "ServiceTypes: " + serviceTypes);
                    long activityId = intent.getLongExtra(KEY_ACTIVITY_ID, 0);
                    synchronized (mActivityIds) {
                        mActivityIds.add(activityId);
                        if (mActivityIds.size() == 1 || serviceTypes != mServiceType) {
                            startForeground(1, buildNotification(), serviceTypes);
                            mServiceType = serviceTypes;
                        }
                    }
                }
                break;
            case ACTION_STOP:
                Log.i(TAG, "Receive ACTION_STOP " + intent.getExtras());
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                    long activityId = intent.getLongExtra(KEY_ACTIVITY_ID, 0);
                    synchronized (mActivityIds) {
                        mActivityIds.remove(activityId);
                        if (mActivityIds.isEmpty()) {
                            stopForeground(STOP_FOREGROUND_REMOVE);
                        }
                    }
                }
                break;
            default:
                Log.w(TAG, "Unknown action: " + intent.getAction());
                break;
        }
        return START_NOT_STICKY;
    }
}
