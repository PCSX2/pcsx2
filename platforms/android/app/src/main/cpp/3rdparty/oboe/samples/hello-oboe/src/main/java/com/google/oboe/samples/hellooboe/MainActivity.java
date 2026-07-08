/*
 * Copyright 2017 The Android Open Source Project
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

package com.google.oboe.samples.hellooboe;

import android.app.Activity;
import android.content.Context;
import android.media.AudioDeviceInfo;
import android.media.AudioManager;
import android.os.Build;
import android.os.Bundle;

import androidx.annotation.RequiresApi;

import android.os.Message;
import android.util.Log;
import android.view.MotionEvent;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.SimpleAdapter;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;

import com.google.oboe.samples.audio_device.AudioDeviceListEntry;
import com.google.oboe.samples.audio_device.AudioDeviceSpinner;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Objects;
import java.util.Timer;
import java.util.TimerTask;

public class MainActivity extends Activity {
    private static final String TAG = "HelloOboe";
    private static final long UPDATE_LATENCY_EVERY_MILLIS = 1000;
    private static final Integer[] CHANNEL_COUNT_OPTIONS = {1, 2, 3, 4, 5, 6, 7, 8};
    // Default to Stereo (OPTIONS is zero-based array so index 1 = 2 channels)
    private static final int CHANNEL_COUNT_DEFAULT_OPTION_INDEX = 1;
    private static final int[] BUFFER_SIZE_OPTIONS = {0, 1, 2, 4, 8};
    private static final String[] AUDIO_API_OPTIONS = {"Unspecified", "OpenSL ES", "AAudio"};
    private static final int OBOE_API_OPENSL_ES = 1;
    // Default all other spinners to the first option on the list
    private static final int SPINNER_DEFAULT_OPTION_INDEX = 0;

    private Spinner mAudioApiSpinner;
    private AudioDeviceSpinner mPlaybackDeviceSpinner;
    private Spinner mChannelCountSpinner;
    private Spinner mBufferSizeSpinner;
    private TextView mLatencyText;
    private Timer mLatencyUpdater;
    private boolean mScoStarted = false;

    /** Commands for background thread. */
    private static final int WHAT_START = 100;
    private static final int WHAT_STOP = 101;
    private static final int WHAT_SET_DEVICE_ID = 102;
    private static final int WHAT_SET_AUDIO_API = 103;
    private static final int WHAT_SET_CHANNEL_COUNT = 104;
    private BackgroundRunner mRunner = new MyBackgroundRunner();

    private class MyBackgroundRunner extends BackgroundRunner {
        // These are initialized to zero by Java.
        // Zero matches the oboe::Unspecified value.
        int audioApi;
        int deviceId;
        int channelCount;

        @Override
            /* Execute this in a background thread to avoid ANRs. */
        void handleMessageInBackground(Message message) {
            int what = message.what;
            int arg1 = message.arg1;
            Log.i(MainActivity.TAG, "got background message, what = " + what + ", arg1 = " + arg1);
            int result = 0;
            boolean restart = false;
            switch (what) {
                case WHAT_START:
                    result = PlaybackEngine.startEngine(audioApi, deviceId, channelCount);
                    break;
                case WHAT_STOP:
                    result = PlaybackEngine.stopEngine();
                    break;
                case WHAT_SET_AUDIO_API:
                    if (audioApi != arg1) {
                        audioApi = arg1;
                        restart = true;
                    }
                    break;
                case WHAT_SET_DEVICE_ID:
                    if (deviceId != arg1) {
                        deviceId = arg1;
                        restart = true;
                    }
                    break;
                case WHAT_SET_CHANNEL_COUNT:
                    if (channelCount != arg1) {
                        channelCount = arg1;
                        restart = true;
                    }
                    break;
            }
            if (restart) {
                int result1 = PlaybackEngine.stopEngine();
                int result2 = PlaybackEngine.startEngine(audioApi, deviceId, channelCount);
                result = (result2 != 0) ? result2 : result1;
            }
            if (result != 0) {
                Log.e(TAG, "audio error " + result);
                showToast("Error in audio =" + result);
            }
        }
    }

    /*
     * Hook to user control to start / stop audio playback:
     *    touch-down: start, and keeps on playing
     *    touch-up: stop.
     * simply pass the events to native side.
     */
    @Override
    public boolean onTouchEvent(MotionEvent event) {
        switch (event.getAction()) {
            case (MotionEvent.ACTION_DOWN):
                PlaybackEngine.setToneOn(true);
                break;
            case (MotionEvent.ACTION_UP):
                PlaybackEngine.setToneOn(false);
                break;
        }
        return super.onTouchEvent(event);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        mLatencyText = findViewById(R.id.latencyText);
        setupAudioApiSpinner();
        setupPlaybackDeviceSpinner();
        setupChannelCountSpinner();
        setupBufferSizeSpinner();
    }

    /*
    * Creating engine in onResume() and destroying in onPause() so the stream retains exclusive
    * mode only while in focus. This allows other apps to reclaim exclusive stream mode.
    */
    @Override
    protected void onResume() {
        super.onResume();
        PlaybackEngine.setDefaultStreamValues(this);
        setupLatencyUpdater();

        // Return the spinner states to their default value
        mChannelCountSpinner.setSelection(CHANNEL_COUNT_DEFAULT_OPTION_INDEX);
        mPlaybackDeviceSpinner.setSelection(SPINNER_DEFAULT_OPTION_INDEX);
        mBufferSizeSpinner.setSelection(SPINNER_DEFAULT_OPTION_INDEX);
        if (PlaybackEngine.isAAudioRecommended()) {
            mAudioApiSpinner.setSelection(SPINNER_DEFAULT_OPTION_INDEX);
        } else {
            mAudioApiSpinner.setSelection(OBOE_API_OPENSL_ES);
            mAudioApiSpinner.setEnabled(false);
        }

        startAudioAsync();
    }

    @Override
    protected void onPause() {
        if (mLatencyUpdater != null) mLatencyUpdater.cancel();
        stopAudioAsync();

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            clearCommunicationDevice();
        } else {
            if (mScoStarted) {
                stopBluetoothSco();
                mScoStarted = false;
            }
        }
        super.onPause();
    }

    private void setupChannelCountSpinner() {
        mChannelCountSpinner = findViewById(R.id.channelCountSpinner);

        ArrayAdapter<Integer> channelCountAdapter = new ArrayAdapter<>(this, R.layout.channel_counts_spinner, CHANNEL_COUNT_OPTIONS);
        mChannelCountSpinner.setAdapter(channelCountAdapter);
        mChannelCountSpinner.setSelection(CHANNEL_COUNT_DEFAULT_OPTION_INDEX);

        mChannelCountSpinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> adapterView, View view, int i, long l) {
                setChannelCountAsync(CHANNEL_COUNT_OPTIONS[mChannelCountSpinner.getSelectedItemPosition()]);
            }

            @Override
            public void onNothingSelected(AdapterView<?> adapterView) {
            }
        });
    }

    private void setupBufferSizeSpinner() {
        mBufferSizeSpinner = findViewById(R.id.bufferSizeSpinner);
        mBufferSizeSpinner.setAdapter(new SimpleAdapter(
                this,
                createBufferSizeOptionsList(), // list of buffer size options
                R.layout.buffer_sizes_spinner, // the xml layout
                new String[]{getString(R.string.buffer_size_description_key)}, // field to display
                new int[]{R.id.bufferSizeOption} // View to show field in
        ));

        mBufferSizeSpinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> adapterView, View view, int i, long l) {
                PlaybackEngine.setBufferSizeInBursts(getBufferSizeInBursts());
            }

            @Override
            public void onNothingSelected(AdapterView<?> adapterView) {
            }
        });
    }

    private void setupPlaybackDeviceSpinner() {
        mPlaybackDeviceSpinner = findViewById(R.id.playbackDevicesSpinner);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            mPlaybackDeviceSpinner.setDirectionType(AudioManager.GET_DEVICES_OUTPUTS);
            mPlaybackDeviceSpinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
                @Override
                public void onItemSelected(AdapterView<?> adapterView, View view, int i, long l) {
                    // To use Bluetooth SCO, setCommunicationDevice() or startBluetoothSco() must
                    // be called. The AudioManager.startBluetoothSco() is deprecated in Android T.
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                        if (isScoDevice(getPlaybackDeviceId())){
                            setCommunicationDevice(getPlaybackDeviceId());
                        } else {
                            clearCommunicationDevice();
                        }
                    } else {
                        // Start Bluetooth SCO if needed.
                        if (isScoDevice(getPlaybackDeviceId()) && !mScoStarted) {
                            startBluetoothSco();
                            mScoStarted = true;
                        } else if (!isScoDevice(getPlaybackDeviceId()) && mScoStarted) {
                            stopBluetoothSco();
                            mScoStarted = false;
                        }
                    }
                    setAudioDeviceIdAsync(getPlaybackDeviceId());
                }

                @Override
                public void onNothingSelected(AdapterView<?> adapterView) {
                }
            });
        } else {
            mPlaybackDeviceSpinner.setEnabled(false);
        }
    }

    private void setupAudioApiSpinner() {
        mAudioApiSpinner = findViewById(R.id.audioApiSpinner);
        mAudioApiSpinner.setAdapter(new SimpleAdapter(
                this,
                createAudioApisOptionsList(),
                R.layout.audio_apis_spinner, // the xml layout
                new String[]{getString(R.string.audio_api_description_key)}, // field to display
                new int[]{R.id.audioApiOption} // View to show field in
        ));

        mAudioApiSpinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> adapterView, View view, int i, long l) {
                setAudioApiAsync(i);
                if (i == OBOE_API_OPENSL_ES) {
                    mPlaybackDeviceSpinner.setSelection(SPINNER_DEFAULT_OPTION_INDEX);
                    mPlaybackDeviceSpinner.setEnabled(false);
                } else {
                    mPlaybackDeviceSpinner.setEnabled(true);
                }
            }

            @Override
            public void onNothingSelected(AdapterView<?> adapterView) {
            }
        });
    }

    private int getPlaybackDeviceId() {
        return ((AudioDeviceListEntry) mPlaybackDeviceSpinner.getSelectedItem()).getId();
    }

    private int getBufferSizeInBursts() {
        @SuppressWarnings("unchecked")
        HashMap<String, String> selectedOption = (HashMap<String, String>)
                mBufferSizeSpinner.getSelectedItem();

        String valueKey = getString(R.string.buffer_size_value_key);

        // parseInt will throw a NumberFormatException if the string doesn't contain a valid integer
        // representation. We don't need to worry about this because the values are derived from
        // the BUFFER_SIZE_OPTIONS int array.
        return Integer.parseInt(Objects.requireNonNull(selectedOption.get(valueKey)));
    }

    private void setupLatencyUpdater() {
        //Update the latency every 1s
        TimerTask latencyUpdateTask = new TimerTask() {
            @Override
            public void run() {
                final String latencyStr;
                if (PlaybackEngine.isLatencyDetectionSupported()) {
                    double latency = PlaybackEngine.getCurrentOutputLatencyMillis();
                    if (latency >= 0) {
                        latencyStr = String.format(Locale.getDefault(), "%.2fms", latency);
                    } else {
                        latencyStr = "Unknown";
                    }
                } else {
                    latencyStr = getString(R.string.only_supported_on_api_26);
                }

                runOnUiThread(() -> mLatencyText.setText(getString(R.string.latency, latencyStr)));
            }
        };
        mLatencyUpdater = new Timer();
        mLatencyUpdater.schedule(latencyUpdateTask, 0, UPDATE_LATENCY_EVERY_MILLIS);
    }

    /**
     * Creates a list of buffer size options which can be used to populate a SimpleAdapter.
     * Each option has a description and a value. The description is always equal to the value,
     * except when the value is zero as this indicates that the buffer size should be set
     * automatically by the audio engine
     *
     * @return list of buffer size options
     */
    private List<HashMap<String, String>> createBufferSizeOptionsList() {

        ArrayList<HashMap<String, String>> bufferSizeOptions = new ArrayList<>();

        for (int i : BUFFER_SIZE_OPTIONS) {
            HashMap<String, String> option = new HashMap<>();
            String strValue = String.valueOf(i);
            String description = (i == 0) ? getString(R.string.automatic) : strValue;
            option.put(getString(R.string.buffer_size_description_key), description);
            option.put(getString(R.string.buffer_size_value_key), strValue);

            bufferSizeOptions.add(option);
        }

        return bufferSizeOptions;
    }

    private List<HashMap<String, String>> createAudioApisOptionsList() {

        ArrayList<HashMap<String, String>> audioApiOptions = new ArrayList<>();

        for (int i = 0; i < AUDIO_API_OPTIONS.length; i++) {
            HashMap<String, String> option = new HashMap<>();
            option.put(getString(R.string.buffer_size_description_key), AUDIO_API_OPTIONS[i]);
            option.put(getString(R.string.buffer_size_value_key), String.valueOf(i));
            audioApiOptions.add(option);
        }
        return audioApiOptions;
    }

    protected void showToast(final String message) {
        runOnUiThread(() -> Toast.makeText(MainActivity.this,
                message,
                Toast.LENGTH_SHORT).show());
    }

    @RequiresApi(api = Build.VERSION_CODES.S)
    private void setCommunicationDevice(int deviceId) {
        AudioManager audioManager = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
        final AudioDeviceInfo[] devices;
        devices = audioManager.getDevices(AudioManager.GET_DEVICES_OUTPUTS);
        for (AudioDeviceInfo device : devices) {
            if (device.getId() == deviceId) {
                audioManager.setCommunicationDevice(device);
                return;
            }
        }
    }

    @RequiresApi(api = Build.VERSION_CODES.S)
    private void clearCommunicationDevice() {
        AudioManager audioManager = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
        audioManager.clearCommunicationDevice();
    }

    @RequiresApi(api = Build.VERSION_CODES.M)
    private boolean isScoDevice(int deviceId) {
        if (deviceId == 0) return false; // Unspecified
        AudioManager audioManager = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
        final AudioDeviceInfo[] devices;
        devices = audioManager.getDevices(AudioManager.GET_DEVICES_OUTPUTS);
        for (AudioDeviceInfo device : devices) {
            if (device.getId() == deviceId) {
                return device.getType() == AudioDeviceInfo.TYPE_BLUETOOTH_SCO;
            }
        }
        return false;
    }

    private void startBluetoothSco() {
        AudioManager myAudioMgr = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
        myAudioMgr.startBluetoothSco();
    }

    private void stopBluetoothSco() {
        AudioManager myAudioMgr = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
        myAudioMgr.stopBluetoothSco();
    }


    void startAudioAsync() {
        mRunner.sendMessage(WHAT_START);
    }

    void stopAudioAsync() {
        mRunner.sendMessage(WHAT_STOP);
    }

    void setAudioApiAsync(int audioApi){
        mRunner.sendMessage(WHAT_SET_AUDIO_API, audioApi);
    }

    void setAudioDeviceIdAsync(int deviceId){
        mRunner.sendMessage(WHAT_SET_DEVICE_ID, deviceId);
    }

    void setChannelCountAsync(int channelCount) {
        mRunner.sendMessage(WHAT_SET_CHANNEL_COUNT, channelCount);
    }
}
