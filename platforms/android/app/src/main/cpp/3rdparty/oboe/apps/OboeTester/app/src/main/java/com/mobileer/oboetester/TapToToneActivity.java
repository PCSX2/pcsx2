/*
 * Copyright 2015 The Android Open Source Project
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

import static com.mobileer.oboetester.MidiTapTester.NoteListener;

import android.content.pm.PackageManager;
import android.graphics.Color;
import android.media.AudioDeviceInfo;
import android.media.AudioManager;
import android.media.midi.MidiDevice;
import android.media.midi.MidiDeviceInfo;
import android.media.midi.MidiInputPort;
import android.media.midi.MidiManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.Toast;

import com.mobileer.audio_device.AudioDeviceListEntry;
import com.mobileer.audio_device.AudioDeviceSpinner;
import com.mobileer.miditools.MidiOutputPortConnectionSelector;
import com.mobileer.miditools.MidiPortConnector;
import com.mobileer.miditools.MidiTools;

import java.io.IOException;
import java.sql.Timestamp;

public class TapToToneActivity extends TestOutputActivityBase {
    // Names from obsolete version of Oboetester.
    public static final String OLD_PRODUCT_NAME = "AudioLatencyTester";
    public static final String OLD_MANUFACTURER_NAME = "AndroidTest";

    private MidiManager mMidiManager;
    private MidiInputPort mInputPort;

    protected MidiTapTester mMidiTapTester;
    protected TapToToneTester mTapToToneTester;

    private Button mStopButton;
    private Button mStartButton;
    private CheckBox mUseNoisePulseCheckBox;

    private MidiOutputPortConnectionSelector mPortSelector;
    private final MyNoteListener mTestListener = new MyNoteListener();

    private AudioDeviceSpinner mInputDeviceSpinner;

    @Override
    protected void inflateActivity() {
        setContentView(R.layout.activity_tap_to_tone);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mAudioOutTester = addAudioOutputTester();

        mTapToToneTester = new TapToToneTester(this,
                getResources().getString(R.string.tap_to_tone_instructions));

        if (getPackageManager().hasSystemFeature(PackageManager.FEATURE_MIDI)) {
            setupMidi();
        } else {
            Toast.makeText(TapToToneActivity.this,
                    "MIDI not supported!", Toast.LENGTH_LONG)
                    .show();
        }


        // Start a blip test when the waveform view is tapped.
        WaveformView mWaveformView = (WaveformView) findViewById(R.id.waveview_audio_original);
        WaveformView mFastWaveformView = (WaveformView) findViewById(R.id.waveview_audio_fast_avg);
        WaveformView mSlowWaveformView = (WaveformView) findViewById(R.id.waveview_audio_slow_avg);
        WaveformView mLowThresholdWaveformView = (WaveformView) findViewById(R.id.waveview_audio_lowThreshold);
        WaveformView mArmedWaveformView = (WaveformView) findViewById(R.id.waveview_audio_armed_waveform);

        update(R.id.waveview_audio_original, Color.BLUE, Color.argb(128,0, 120, 0), Color.TRANSPARENT);
        update(R.id.waveview_audio_fast_avg, Color.argb(255,0, 247, 255), Color.TRANSPARENT, Color.argb(70, 255, 238, 0));
        update(R.id.waveview_audio_slow_avg, Color.argb(255, 174, 0, 255), Color.TRANSPARENT, Color.argb(70, 255, 238, 0));
        update(R.id.waveview_audio_lowThreshold, Color.argb(255, 255, 132, 0), Color.TRANSPARENT, Color.argb(70, 255, 238, 0));
        update(R.id.waveview_audio_armed_waveform, Color.argb(50, 255, 238, 0), Color.TRANSPARENT, Color.RED);

        mWaveformView.setOnTouchListener((view, event) -> {
            // Do not call view.performClick() because it may trigger a touch sound!
            int action = event.getActionMasked();
            switch (action) {
                case MotionEvent.ACTION_DOWN:
                case MotionEvent.ACTION_POINTER_DOWN:
                    trigger();
                    break;
                case MotionEvent.ACTION_MOVE:
                    break;
                case MotionEvent.ACTION_UP:
                case MotionEvent.ACTION_POINTER_UP:
                    break;
            }
            // Must return true or we do not get the ACTION_MOVE and
            // ACTION_UP events.
            return true;
        });

        mCommunicationDeviceView = (CommunicationDeviceView) findViewById(R.id.comm_device_view);

        mStartButton = (Button) findViewById(R.id.button_start);
        mStopButton = (Button) findViewById(R.id.button_stop);
        updateButtons(false);

        updateEnabledWidgets();

        mInputDeviceSpinner = (AudioDeviceSpinner) findViewById(R.id.input_devices_spinner);
        mInputDeviceSpinner.setDirectionType(AudioManager.GET_DEVICES_INPUTS);

        mUseNoisePulseCheckBox = (CheckBox) findViewById(R.id.checkbox_use_noise_pulse);
        useNoisePulse(mUseNoisePulseCheckBox.isChecked());
    }

    private void update(int waveformViewId, int waveColor, int backgroundColor, int cursorColor) {
        WaveformView waveformView = (WaveformView) findViewById(waveformViewId);
        waveformView.updateTheme(waveColor, backgroundColor, cursorColor);
    }

    private void updateButtons(boolean running) {
        mStartButton.setEnabled(!running);
        mStopButton.setEnabled(running);
    }

    void trigger() {
        if (mTapToToneTester.isArmed()) {
            mAudioOutTester.trigger();
            mTapToToneTester.analyzeLater(getString(R.string.please_wait));
            Timestamp timestamp = new Timestamp(System.currentTimeMillis());
            Log.d(TAG, "Tap to Tone Triggered. Timestamp: " + timestamp);
        } else {
            showToast(getString(R.string.no_double_tap));
        }
    }

    @Override
    int getActivityType() {
        return ACTIVITY_TAP_TO_TONE;
    }

    @Override
    protected void onDestroy() {
        mMidiTapTester.removeTestListener(mTestListener);
        closeMidiResources();
        super.onDestroy();
    }

    private void setupMidi() {
        // Setup MIDI
        mMidiManager = (MidiManager) getSystemService(MIDI_SERVICE);
        MidiDeviceInfo[] infos = mMidiManager.getDevices();

        // Warn if old version of OboeTester found.
        for (MidiDeviceInfo info : infos) {
            Log.i(TAG, "MIDI info = " + info);
            Bundle properties = info.getProperties();
            String product = properties
                    .getString(MidiDeviceInfo.PROPERTY_PRODUCT);
            String manufacturer = properties
                    .getString(MidiDeviceInfo.PROPERTY_MANUFACTURER);
            if (OLD_PRODUCT_NAME.equals(product) && OLD_MANUFACTURER_NAME.equals(manufacturer)) {
                showErrorToast("Please uninstall old version of OboeTester.");
                break;
            }
        }

        // Open the port now so that the MidiTapTester gets created.
        for (MidiDeviceInfo info : infos) {
            Bundle properties = info.getProperties();
            String product = properties
                    .getString(MidiDeviceInfo.PROPERTY_PRODUCT);
            if (MidiTapTester.PRODUCT_NAME.equals(product)) {
                String manufacturer = properties
                        .getString(MidiDeviceInfo.PROPERTY_MANUFACTURER);
                if (MidiTapTester.MANUFACTURER_NAME.equals(manufacturer)) {
                    openPortTemporarily(info);
                    break;
                }
            }
        }
    }

    // These should only be set after mAudioMidiTester is set.
    private void setSpinnerListeners() {
        MidiDeviceInfo synthInfo = MidiTools.findDevice(mMidiManager, MidiTapTester.MANUFACTURER_NAME,
                MidiTapTester.PRODUCT_NAME);
        Log.i(TAG, "found tester virtual device info: " + synthInfo);
        int portIndex = 0;
        mPortSelector = new MidiOutputPortConnectionSelector(mMidiManager, this,
                R.id.spinner_synth_sender, synthInfo, portIndex);
        mPortSelector.setConnectedListener(new MyPortsConnectedListener());

    }

    private class MyNoteListener implements NoteListener {
        @Override
        public void onNoteOn(final int pitch) {
            runOnUiThread(() -> {
                trigger();
                mStreamContexts.get(0).configurationView.setStatusText("MIDI pitch = " + pitch);
            });
        }
    }

    private void openPortTemporarily(final MidiDeviceInfo info) {
        Log.i(TAG, "MIDI openPort() info = " + info);
        mMidiManager.openDevice(info, device -> {
            if (device == null) {
                Log.e(TAG, "could not open device " + info);
            } else {
                mInputPort = device.openInputPort(0);
                Log.i(TAG, "opened MIDI port = " + mInputPort + " on " + info);
                mMidiTapTester = MidiTapTester.getInstanceOrNull();
                if (mMidiTapTester == null) {
                    Log.e(TAG, "MidiTapTester Service was not created! info = " + info);
                    showErrorToast("MidiTapTester Service was not created!");
                } else {
                    Log.i(TAG, "openPort() mMidiTapTester = " + mMidiTapTester);
                    // Now that we have created the MidiTapTester, close the port so we can
                    // open it later.
                    try {
                        mInputPort.close();
                    } catch (IOException e) {
                        e.printStackTrace();
                    }
                    mMidiTapTester.addTestListener(mTestListener);
                    setSpinnerListeners();
                }
            }
        }, new Handler(Looper.getMainLooper())
        );
    }

    // TODO Listen to the synth server
    // for open/close events and then disable/enable the spinner.
    private class MyPortsConnectedListener
            implements MidiPortConnector.OnPortsConnectedListener {
        @Override
        public void onPortsConnected(final MidiDevice.MidiConnection connection) {
            Log.i(TAG, "onPortsConnected, connection = " + connection);
            runOnUiThread(() -> {
                if (connection == null) {
                    Toast.makeText(TapToToneActivity.this,
                            R.string.error_port_busy, Toast.LENGTH_LONG)
                            .show();
                    mPortSelector.clearSelection();
                } else {
                    Toast.makeText(TapToToneActivity.this,
                            R.string.port_open_ok, Toast.LENGTH_LONG)
                            .show();
                }
            });
        }
    }

    private void closeMidiResources() {
        if (mPortSelector != null) {
            mPortSelector.close();
        }
    }

    public void startTest(View view) {
        try {
            openAudio();
        } catch (IOException e) {
            e.printStackTrace();
            showErrorToast("Open audio failed!");
            return;
        }
        try {
            super.startAudio();
            startTapToToneTester();
            updateButtons(true);
        } catch (IOException e) {
            e.printStackTrace();
            showErrorToast("Start audio failed! " + e.getMessage());
            return;
        }
    }

    public void stopTest(View view) {
        stopTapToToneTester();
        stopAudio();
        closeAudio();
        updateButtons(false);
    }

    private void startTapToToneTester() throws IOException {
        AudioDeviceInfo deviceInfo =
                ((AudioDeviceListEntry) mInputDeviceSpinner.getSelectedItem()).getDeviceInfo();
        mTapToToneTester.setInputDevice(deviceInfo);
        mInputDeviceSpinner.setEnabled(false);
        mUseNoisePulseCheckBox.setEnabled(false);
        mTapToToneTester.resetLatency();
        mTapToToneTester.start();
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
    }

    private void stopTapToToneTester() {
        getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        mInputDeviceSpinner.setEnabled(true);
        mUseNoisePulseCheckBox.setEnabled(true);
        mTapToToneTester.stop();
    }

    public void onUseNoisePulseClicked(View view) {
        useNoisePulse(mUseNoisePulseCheckBox.isChecked());
    }

    public native void useNoisePulse(boolean enabled);
}
