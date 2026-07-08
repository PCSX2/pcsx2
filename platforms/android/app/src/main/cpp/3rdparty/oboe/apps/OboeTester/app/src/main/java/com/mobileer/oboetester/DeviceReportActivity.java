/*
 * Copyright 2019 The Android Open Source Project
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

import android.annotation.TargetApi;
import android.content.Context;
import android.content.Intent;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbManager;
import android.media.AudioDeviceCallback;
import android.media.AudioDeviceInfo;
import android.media.AudioManager;
import android.media.MediaCodecInfo;
import android.media.MediaCodecList;
import android.media.MicrophoneInfo;
import android.media.midi.MidiDeviceInfo;
import android.media.midi.MidiManager;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.TextView;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;

import com.mobileer.audio_device.AudioDeviceInfoConverter;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.TreeMap;

/**
 * Print a report of all the available audio devices.
 */
public class DeviceReportActivity extends AppCompatActivity {

    class MyAudioDeviceCallback extends AudioDeviceCallback {
        private HashMap<Integer, AudioDeviceInfo> mDevices
                = new HashMap<Integer, AudioDeviceInfo>();

        @Override
        public void onAudioDevicesAdded(AudioDeviceInfo[] addedDevices) {
            for (AudioDeviceInfo info : addedDevices) {
                mDevices.put(info.getId(), info);
            }
            reportDeviceInfo(mDevices.values());
        }

        public void onAudioDevicesRemoved(AudioDeviceInfo[] removedDevices) {
            for (AudioDeviceInfo info : removedDevices) {
                mDevices.remove(info.getId());
            }
            reportDeviceInfo(mDevices.values());
        }
    }

    MyAudioDeviceCallback mDeviceCallback = new MyAudioDeviceCallback();
    private TextView      mAutoTextView;
    private AudioManager  mAudioManager;
    private UsbManager    mUsbManager;
    private MidiManager mMidiManager;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_device_report);
        mAutoTextView = (TextView) findViewById(R.id.text_log_device_report);
        mAudioManager = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
        mUsbManager = (UsbManager) getSystemService(Context.USB_SERVICE);
        mMidiManager = (MidiManager) getSystemService(Context.MIDI_SERVICE);
    }

    public void onShareButtonClick(View view) {
        if(mAutoTextView !=null) {
            Intent sendIntent = new Intent();
            sendIntent.setAction(Intent.ACTION_SEND);
            sendIntent.putExtra(Intent.EXTRA_TEXT, mAutoTextView.getText().toString());
            sendIntent.setType("text/plain");
            Intent shareIntent = Intent.createChooser(sendIntent, null);
            startActivity(shareIntent);
        }
    }

    @Override
    protected void onStart() {
        super.onStart();
        addAudioDeviceCallback();
    }

    @Override
    protected void onStop() {
        removeAudioDeviceCallback();
        super.onStop();
    }

    @TargetApi(23)
    private void addAudioDeviceCallback(){
        // Note that we will immediately receive a call to onDevicesAdded with the list of
        // devices which are currently connected.
        mAudioManager.registerAudioDeviceCallback(mDeviceCallback, null);
    }

    @TargetApi(23)
    private void removeAudioDeviceCallback(){
        mAudioManager.unregisterAudioDeviceCallback(mDeviceCallback);
    }

    private void reportDeviceInfo(Collection<AudioDeviceInfo> devices) {
        logClear();
        StringBuffer report = new StringBuffer();
        report.append("Device Report:\n");
        report.append("App: ").append(MainActivity.getVersionText()).append("\n");
        report.append("Device: ").append(Build.MANUFACTURER).append(", ").append(Build.MODEL)
                .append(", ").append(Build.PRODUCT).append("\n");

        report.append(reportExtraDeviceInfo());
        report.append("\n");

        for (AudioDeviceInfo deviceInfo : devices) {
            report.append("\n==== Device =================== " + deviceInfo.getId() + "\n");
            String item = AudioDeviceInfoConverter.toString(mAudioManager, deviceInfo);
            report.append(item);
        }
        report.append(reportAllMicrophones());
        report.append(reportUsbDevices());
        report.append(reportMidiDevices());
        report.append(reportMediaCodecs());
        report.append(CpuInfoReader.reportCpuInfo());
        log(report.toString());
    }

    String toHex4(int n) {
        return String.format("0x%04X", n);
    }

    public String reportUsbDevices() {
        StringBuffer report = new StringBuffer();
        report.append("\n############################");
        report.append("\nUsb Device Report:\n");
        try {
            HashMap<String, UsbDevice> usbDeviceList = mUsbManager.getDeviceList();
            for (UsbDevice usbDevice : usbDeviceList.values()) {
                report.append("\n==== USB Device ========= " + usbDevice.getDeviceId());
                report.append("\nProduct Name       : " + usbDevice.getProductName());
                report.append("\nProduct ID         : " + toHex4(usbDevice.getProductId()));
                report.append("\nManufacturer Name  : " + usbDevice.getManufacturerName());
                report.append("\nVendor ID          : " + toHex4(usbDevice.getVendorId()));
                report.append("\nDevice Name        : " + usbDevice.getDeviceName());
                report.append("\nDevice Protocol    : " + usbDevice.getDeviceProtocol());
                report.append("\nDevice Class       : " + usbDevice.getDeviceClass());
                report.append("\nDevice Subclass    : " + usbDevice.getDeviceSubclass());
                report.append("\nVersion            : " + usbDevice.getVersion());
                report.append("\n" + usbDevice);
                report.append("\n");
            }
        } catch (Exception e) {
            Log.e(TestAudioActivity.TAG, "Caught ", e);
            showErrorToast(e.getMessage());
            report.append("\nERROR: " + e.getMessage() + "\n");
        }
        return report.toString();
    }

    public String reportMidiDevices() {
        StringBuffer report = new StringBuffer();
        report.append("\n############################");
        report.append("\nMidi Device Report:\n");
        try {
            MidiDeviceInfo[] midiDeviceInfos = mMidiManager.getDevices();
            for (MidiDeviceInfo midiDeviceInfo : midiDeviceInfos) {
                report.append("\n==== MIDI Device ========= " + midiDeviceInfo.getId());
                addMidiDeviceInfoToDeviceReport(midiDeviceInfo, report);
            }
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                Set<MidiDeviceInfo> umpDeviceInfos =
                        mMidiManager.getDevicesForTransport(MidiManager.TRANSPORT_UNIVERSAL_MIDI_PACKETS);
                for (MidiDeviceInfo midiDeviceInfo : umpDeviceInfos) {
                    report.append("\n==== UMP Device ========= " + midiDeviceInfo.getId());
                    addMidiDeviceInfoToDeviceReport(midiDeviceInfo, report);
                }
            }
        } catch (Exception e) {
            Log.e(TestAudioActivity.TAG, "Caught ", e);
            showErrorToast(e.getMessage());
            report.append("\nERROR: " + e.getMessage() + "\n");
        }
        return report.toString();
    }

    private void addMidiDeviceInfoToDeviceReport(MidiDeviceInfo midiDeviceInfo,
                                                 StringBuffer report){
        report.append("\nInput Count        : " + midiDeviceInfo.getInputPortCount());
        report.append("\nOutput Count       : " + midiDeviceInfo.getOutputPortCount());
        report.append("\nType               : " + midiDeviceInfo.getType());
        report.append("\nIs Private         : " + midiDeviceInfo.isPrivate());
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            report.append("\nDefault Protocol   : " + midiDeviceInfo.getDefaultProtocol());
        }
        report.append("\n" + midiDeviceInfo);
        report.append("\n");
    }

    public String reportAllMicrophones() {
        StringBuffer report = new StringBuffer();
        report.append("\n############################");
        report.append("\nMicrophone Report:\n");
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.P) {
            try {
                List<MicrophoneInfo> micList = mAudioManager.getMicrophones();
                for (MicrophoneInfo micInfo : micList) {
                    String micItem = MicrophoneInfoConverter.reportMicrophoneInfo(micInfo);
                    report.append(micItem);
                }
            } catch (IOException e) {
                Log.e(TestAudioActivity.TAG, "Caught ", e);
                return e.getMessage();
            } catch (Exception e) {
                Log.e(TestAudioActivity.TAG, "Caught ", e);
                showErrorToast(e.getMessage());
                report.append("\nERROR: " + e.getMessage() + "\n");
            }
        } else {
            report.append("\nMicrophoneInfo not available on V" + android.os.Build.VERSION.SDK_INT);
        }
        return report.toString();
    }

    private String reportExtraDeviceInfo() {
        StringBuffer report = new StringBuffer();
        report.append("\n\n############################");
        report.append("\nAudioManager:");
        report.append(AudioQueryTools.getAudioManagerReport(mAudioManager));
        report.append("\n\nFeatures:");
        report.append(AudioQueryTools.getAudioFeatureReport(getPackageManager()));
        report.append(AudioQueryTools.getMediaPerformanceClass());
        report.append("\n\nProperties:");
        report.append(AudioQueryTools.getAudioPropertyReport());
        return report.toString();
    }

    public String reportMediaCodecs() {
        StringBuffer report = new StringBuffer();
        report.append("\n############################");
        report.append("\nMedia Codec Device Report:\n");
        try {
            MediaCodecList mediaCodecList = new MediaCodecList(MediaCodecList.REGULAR_CODECS);
            MediaCodecInfo[] mediaCodecInfos = mediaCodecList.getCodecInfos();
            for (MediaCodecInfo mediaCodecInfo : mediaCodecInfos) {
                report.append("\n==== MediaCodecInfo ========= " + mediaCodecInfo.getName());
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                    report.append("\nCanonical Name         : " + mediaCodecInfo.getCanonicalName());
                    report.append("\nIs Alias               : " + mediaCodecInfo.isAlias());
                    report.append("\nIs Hardware Accelerated: " + mediaCodecInfo.isHardwareAccelerated());
                    report.append("\nIs Software Only       : " + mediaCodecInfo.isSoftwareOnly());
                    report.append("\nIs Vendor              : " + mediaCodecInfo.isVendor());
                }
                report.append("\nIs Encoder             : " + mediaCodecInfo.isEncoder());
                report.append("\nSupported Types        : " + Arrays.toString(mediaCodecInfo.getSupportedTypes()));
                for(String type : mediaCodecInfo.getSupportedTypes()) {
                    MediaCodecInfo.CodecCapabilities codecCapabilities =
                            mediaCodecInfo.getCapabilitiesForType(type);
                    MediaCodecInfo.AudioCapabilities audioCapabilities =
                            codecCapabilities.getAudioCapabilities();
                    if (audioCapabilities != null) {
                        report.append("\nAudio Type: " + type);
                        report.append("\nBitrate Range: " + audioCapabilities.getBitrateRange());
                        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                            report.append("\nInput Channel Count Ranges: " + Arrays.toString(audioCapabilities.getInputChannelCountRanges()));
                            report.append("\nMin Input Channel Count: " + audioCapabilities.getMinInputChannelCount());
                        }
                        report.append("\nMax Input Channel Count: " + audioCapabilities.getMaxInputChannelCount());
                        report.append("\nSupported Sample Rate Ranges: " + Arrays.toString(audioCapabilities.getSupportedSampleRateRanges()));
                        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                            // Avoid bug b/122116282
                            report.append("\nSupported Sample Rates: " + Arrays.toString(audioCapabilities.getSupportedSampleRates()));
                        }
                    }
                    report.append("\nIs Encoder             : " + mediaCodecInfo.isEncoder());
                }
                report.append("\n");
            }
        } catch (Exception e) {
            Log.e(TestAudioActivity.TAG, "Caught ", e);
            showErrorToast(e.getMessage());
            report.append("\nERROR: " + e.getMessage() + "\n");
        }
        return report.toString();
    }

    public static class CpuInfoReader {

        private static final String CPU_CAPACITY_PATH_FORMAT = "/sys/devices/system/cpu/cpu%d/cpu_capacity";
        private static final String CPU_MIN_FREQ_PATH_FORMAT = "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_min_freq";
        private static final String CPU_MAX_FREQ_PATH_FORMAT = "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_max_freq";
        private static final String CPU_CURRENT_FREQ_PATH_FORMAT = "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq";

        // Inner class to hold all the details for a single CPU core
        private static class CoreDetails {
            int index;
            int capacity = -1; // -1 indicates not found
            int minFreq = -1;
            int maxFreq = -1;
            int currentFreq = -1;

            CoreDetails(int index) {
                this.index = index;
            }

            @Override
            public String toString() {
                // Format frequencies from Hz to KHz for readability
                String min = minFreq != -1 ? String.valueOf(minFreq / 1000) : "N/A";
                String max = maxFreq != -1 ? String.valueOf(maxFreq / 1000) : "N/A";
                String cur = currentFreq != -1 ? String.valueOf(currentFreq / 1000) : "N/A";
                String cap = capacity != -1 ? String.valueOf(capacity) : "N/A";

                return String.format("Idx %d Cap %s | Freq(KHz): Mn %s Mx %s Cur %s",
                        index, cap, min, max, cur);
            }
        }

        /**
         * Reads a single integer value from a specified sysfs path.
         * Returns -1 if the file doesn't exist, can't be read, or contains invalid data.
         */
        private static int readSysfsInt(String path) {
            File file = new File(path);
            if (!file.exists()) {
                return -1; // File not found
            }
            try (BufferedReader br = new BufferedReader(new FileReader(file))) {
                String line = br.readLine();
                if (line != null) {
                    return Integer.parseInt(line.trim());
                }
            } catch (IOException | NumberFormatException e) {
                // Log.e(TAG, "Error reading " + path, e); // Use Android's Log if in an Android context
                System.err.println("Error reading or parsing " + path + ": " + e.getMessage());
            }
            return -1; // Error or invalid data
        }

        /**
         * Gathers detailed CPU core information and formats it into a string.
         * This method requires appropriate file system permissions to access /sys paths.
         *
         * @return A neatly formatted string containing CPU core details, or an error message.
         */
        public static String reportCpuInfo() {
            Map<Integer, CoreDetails> cpuDetailsMap = new TreeMap<>(); // Use TreeMap to keep cores sorted by index

            int cpuIndex = 0;
            boolean foundAnyCore = false;

            while (true) {
                String capacityPath = String.format(CPU_CAPACITY_PATH_FORMAT, cpuIndex);
                String minFreqPath = String.format(CPU_MIN_FREQ_PATH_FORMAT, cpuIndex);
                String maxFreqPath = String.format(CPU_MAX_FREQ_PATH_FORMAT, cpuIndex);
                String curFreqPath = String.format(CPU_CURRENT_FREQ_PATH_FORMAT, cpuIndex);

                // Check if at least one common file exists for this CPU index
                if (!new File(capacityPath).exists() &&
                        !new File(minFreqPath).exists() &&
                        !new File(maxFreqPath).exists() &&
                        !new File(curFreqPath).exists()) {
                    // If none of the paths exist for this CPU index, assume no more CPUs
                    break;
                }
                foundAnyCore = true;

                // Get or create CoreDetails for the current CPU index
                CoreDetails details = cpuDetailsMap.get(cpuIndex);
                if (details == null) {
                    details = new CoreDetails(cpuIndex);
                    cpuDetailsMap.put(cpuIndex, details);
                }

                details.capacity = readSysfsInt(capacityPath);
                details.minFreq = readSysfsInt(minFreqPath);
                details.maxFreq = readSysfsInt(maxFreqPath);
                details.currentFreq = readSysfsInt(curFreqPath);

                cpuIndex++;
            }

            if (!foundAnyCore) {
                return "############################\nCould not retrieve CPU core information.";
            }

            StringBuilder resultBuilder = new StringBuilder();
            resultBuilder.append("############################\n");
            resultBuilder.append("CPU Cores:\n");

            for (CoreDetails details : cpuDetailsMap.values()) {
                resultBuilder.append(details.toString()).append("\n");
            }

            return resultBuilder.toString();
        }
    }

    // Write to scrollable TextView
    private void log(final String text) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mAutoTextView.append(text);
                mAutoTextView.append("\n");
            }
        });
    }

    private void logClear() {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mAutoTextView.setText("");
            }
        });
    }

    protected void showErrorToast(String message) {
        String text = "Error: " + message;
        Log.e(TestAudioActivity.TAG, text);
        showToast(text);
    }

    protected void showToast(final String message) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                Toast.makeText(DeviceReportActivity.this,
                        message,
                        Toast.LENGTH_SHORT).show();
            }
        });
    }
}
