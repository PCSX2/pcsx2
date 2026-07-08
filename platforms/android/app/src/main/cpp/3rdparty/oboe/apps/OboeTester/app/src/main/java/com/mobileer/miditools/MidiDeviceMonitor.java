/*
 * Copyright (C) 2016 The Android Open Source Project
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

package com.mobileer.miditools;

import android.media.midi.MidiDeviceInfo;
import android.media.midi.MidiDeviceStatus;
import android.media.midi.MidiManager;
import android.media.midi.MidiManager.DeviceCallback;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;

import java.util.HashMap;
import java.util.Map;

/**
 * Manage a list a of DeviceCallbacks that are called when a MIDI Device is
 * plugged in or unplugged.
 *
 * This class is used to workaround a bug in the M release of the Android MIDI API.
 * The MidiManager.unregisterDeviceCallback() method was not working. So if an app
 * was rotated, and the Activity destroyed and recreated, the DeviceCallbacks would
 * accumulate in the MidiServer. This would result in multiple callbacks whenever a
 * device was added. This class allow an app to register and unregister multiple times
 * using a local list of callbacks. It registers a single callback, which stays registered
 * until the app is dead.
 *
 * This code checks to see if the N release is being used. N has a fix for the bug.
 * For N, the register and unregister calls are passed directly to the MidiManager.
 *
 * Note that this code is not thread-safe. It should only be called from the UI thread.
 */
public class MidiDeviceMonitor {
    public final static String TAG = "MidiDeviceMonitor";

    private static MidiDeviceMonitor mInstance;
    private MidiManager mMidiManager;
    private HashMap<DeviceCallback, Handler> mCallbacks = new HashMap<DeviceCallback,Handler>();
    private MyDeviceCallback mMyDeviceCallback;
    // We only need the workaround for versions before N.
    private boolean mUseProxy = Build.VERSION.SDK_INT <= Build.VERSION_CODES.M;

    // Use an inner class so we do not clutter the API of MidiDeviceMonitor
    // with public DeviceCallback methods.
    protected class MyDeviceCallback extends DeviceCallback {

        @Override
        public void onDeviceAdded(final MidiDeviceInfo device) {
            // Call all of the locally registered callbacks.
            for(Map.Entry<DeviceCallback, Handler> item : mCallbacks.entrySet()) {
                final DeviceCallback callback = item.getKey();
                Handler handler = item.getValue();
                if(handler == null) {
                    callback.onDeviceAdded(device);
                } else {
                    handler.post(new Runnable() {
                        @Override
                        public void run() {
                            callback.onDeviceAdded(device);
                        }
                    });
                }
            }
        }

        @Override
        public void onDeviceRemoved(final MidiDeviceInfo device) {
            for(Map.Entry<DeviceCallback, Handler> item : mCallbacks.entrySet()) {
                final DeviceCallback callback = item.getKey();
                Handler handler = item.getValue();
                if(handler == null) {
                    callback.onDeviceRemoved(device);
                } else {
                    handler.post(new Runnable() {
                        @Override
                        public void run() {
                            callback.onDeviceRemoved(device);
                        }
                    });
                }
            }
        }

        @Override
        public void onDeviceStatusChanged(final MidiDeviceStatus status) {
            for(Map.Entry<DeviceCallback, Handler> item : mCallbacks.entrySet()) {
                final DeviceCallback callback = item.getKey();
                Handler handler = item.getValue();
                if(handler == null) {
                    callback.onDeviceStatusChanged(status);
                } else {
                    handler.post(new Runnable() {
                        @Override
                        public void run() {
                            callback.onDeviceStatusChanged(status);
                        }
                    });
                }
            }
        }
    }

    private MidiDeviceMonitor(MidiManager midiManager) {
        mMidiManager = midiManager;
        if (mUseProxy) {
            Log.i(TAG,"Running on M so we need to use the workaround.");
            mMyDeviceCallback = new MyDeviceCallback();
            mMidiManager.registerDeviceCallback(mMyDeviceCallback,
                    new Handler(Looper.getMainLooper()));
        }
    }

    public synchronized static MidiDeviceMonitor getInstance(MidiManager midiManager) {
        if (mInstance == null) {
            mInstance = new MidiDeviceMonitor(midiManager);
        }
        return mInstance;
    }

    public void registerDeviceCallback(DeviceCallback callback, Handler handler) {
        if (mUseProxy) {
            // Keep our own list of callbacks.
            mCallbacks.put(callback, handler);
        } else {
            mMidiManager.registerDeviceCallback(callback, handler);
        }
    }

    public void unregisterDeviceCallback(DeviceCallback callback) {
        if (mUseProxy) {
            mCallbacks.remove(callback);
        } else {
            // This works on N or later.
            mMidiManager.unregisterDeviceCallback(callback);
        }
    }
}
