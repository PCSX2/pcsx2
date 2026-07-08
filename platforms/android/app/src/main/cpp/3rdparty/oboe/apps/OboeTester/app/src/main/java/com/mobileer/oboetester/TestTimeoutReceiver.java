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

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.util.Log;
import androidx.localbroadcastmanager.content.LocalBroadcastManager;

public class TestTimeoutReceiver extends BroadcastReceiver {
    private static final String TAG = "TestTimeoutReceiver";
    public static final String ACTION_STOP_TEST = "com.mobileer.oboetester.ACTION_STOP_TEST";

    @Override
    public void onReceive(Context context, Intent intent) {
        Log.d(TAG, "Alarm received. Stopping test via local broadcast.");
        Intent stopIntent = new Intent(ACTION_STOP_TEST);
        LocalBroadcastManager.getInstance(context).sendBroadcast(stopIntent);
    }
}
