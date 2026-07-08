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

package com.mobileer.oboetester;

import android.content.pm.PackageManager;
import android.media.AudioManager;
import android.os.Build;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.Locale;

public class AudioQueryTools {
    private static String GETPROP_EXECUTABLE_PATH = "/system/bin/getprop";

    public static String getSystemProperty(String propName) {
        Process process = null;
        BufferedReader bufferedReader = null;
        try {
            process = new ProcessBuilder().command(GETPROP_EXECUTABLE_PATH, propName).redirectErrorStream(true).start();
            bufferedReader = new BufferedReader(new InputStreamReader(process.getInputStream()));
            String line = bufferedReader.readLine();
            if (line == null){
                line = ""; //prop not set
            }
            return line;
        } catch (Exception e) {
            return "";
        } finally{
            if (bufferedReader != null){
                try {
                    bufferedReader.close();
                } catch (IOException e) {}
            }
            if (process != null){
                process.destroy();
            }
        }
    }

    public static String getAudioFeatureReport(PackageManager packageManager) {
        StringBuffer report = new StringBuffer();
        report.append("\nProAudio Feature     : "
                + packageManager.hasSystemFeature(PackageManager.FEATURE_AUDIO_PRO));
        report.append("\nLowLatency Feature   : "
                + packageManager.hasSystemFeature(PackageManager.FEATURE_AUDIO_LOW_LATENCY));
        report.append("\nAudio Output Feature : "
                + packageManager.hasSystemFeature(PackageManager.FEATURE_AUDIO_OUTPUT));
        report.append("\nMicrophone Feature   : "
                + packageManager.hasSystemFeature(PackageManager.FEATURE_MICROPHONE));
        report.append("\nMIDI Feature         : "
                + packageManager.hasSystemFeature(PackageManager.FEATURE_MIDI));
        report.append("\nUSB Host Feature     : "
                + packageManager.hasSystemFeature(PackageManager.FEATURE_USB_HOST));
        report.append("\nUSB Accessory Feature: "
                + packageManager.hasSystemFeature(PackageManager.FEATURE_USB_ACCESSORY));
        report.append("\nBluetooth Feature    : "
                + packageManager.hasSystemFeature(PackageManager.FEATURE_BLUETOOTH));
        report.append("\nBluetooth LE Feature : "
                + packageManager.hasSystemFeature(PackageManager.FEATURE_BLUETOOTH_LE));
        if (android.os.Build.VERSION.SDK_INT > Build.VERSION_CODES.TIRAMISU) {
            report.append("\nTelecom Feature      : "
                    + packageManager.hasSystemFeature(PackageManager.FEATURE_TELECOM));
            report.append("\nTelephonyCall Feature: "
                    + packageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY_CALLING));
        }
        return report.toString();
    }

    public static String getAudioManagerReport(AudioManager audioManager) {
        StringBuffer report = new StringBuffer();
        String unprocessedSupport = audioManager.getProperty(
                AudioManager.PROPERTY_SUPPORT_AUDIO_SOURCE_UNPROCESSED);
        report.append("\nSUPPORT_AUDIO_SOURCE_UNPROCESSED  : " + ((unprocessedSupport == null) ?
                "null" : unprocessedSupport));
        String outputFramesPerBuffer = audioManager.getProperty(
                AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER);
        report.append("\nOUTPUT_FRAMES_PER_BUFFER  : " + ((outputFramesPerBuffer == null) ?
                "null" : outputFramesPerBuffer));
        String outputSampleRate = audioManager.getProperty(
                AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE);
        report.append("\nOUTPUT_SAMPLE_RATE  : " + ((outputSampleRate == null) ? "null" :
                outputSampleRate));
        return report.toString();
    }

    private static String formatKeyValueLine(String key, String value) {
        int numSpaces = Math.max(1, 21 - key.length());
        String spaces = String.format(Locale.getDefault(), "%0" + numSpaces + "d", 0).replace("0", " ");
        return "\n" + key + spaces + ": " + value;
    }

    private static String getSystemPropertyLine(String key) {
        return formatKeyValueLine(key, getSystemProperty(key));
    }

    public static String convertSdkToShortName(int sdk) {
        if (sdk < 16) return "early";
        if (sdk > 34) return "future";
        final String[] names = {
                "J",   // 16
                "J+",
                "J++",
                "K",
                "K+",
                "L",   // 21
                "L+",
                "M",
                "N",   // 24
                "N_MR1",
                "O",
                "O_MR1",
                "P",   // 28
                "Q",
                "R",
                "S",
                "S_V2",
                "T",   // 33
                "U"
        };
        return names[sdk - 16];
    }

    public static String getMediaPerformanceClass() {
        if (android.os.Build.VERSION.SDK_INT < android.os.Build.VERSION_CODES.S) {
            return formatKeyValueLine("Media Perf Class", "not supported");
        }
        int mpc = Build.VERSION.MEDIA_PERFORMANCE_CLASS;
        String text = (mpc == 0) ? "not declared" : convertSdkToShortName(mpc);
        return formatKeyValueLine("Media Perf Class",
                mpc + " (" + text + ")");
    }

    public static String getAudioPropertyReport() {
        StringBuffer report = new StringBuffer();
        report.append(getSystemPropertyLine("aaudio.mmap_policy"));
        report.append(getSystemPropertyLine("aaudio.mmap_exclusive_policy"));
        report.append(getSystemPropertyLine("aaudio.mixer_bursts"));
        report.append(getSystemPropertyLine("aaudio.wakeup_delay_usec"));
        report.append(getSystemPropertyLine("aaudio.minimum_sleep_usec"));
        report.append(getSystemPropertyLine("aaudio.hw_burst_min_usec"));
        report.append(getSystemPropertyLine("aaudio.in_mmap_offset_usec"));
        report.append(getSystemPropertyLine("aaudio.out_mmap_offset_usec"));
        report.append(getSystemPropertyLine("ro.product.manufacturer"));
        report.append(getSystemPropertyLine("ro.product.brand"));
        report.append(getSystemPropertyLine("ro.product.model"));
        report.append(getSystemPropertyLine("ro.product.name"));
        report.append(getSystemPropertyLine("ro.product.device"));
        report.append(getSystemPropertyLine("ro.product.cpu.abi"));
        report.append(getSystemPropertyLine("ro.soc.manufacturer"));
        report.append(getSystemPropertyLine("ro.soc.model"));
        report.append(getSystemPropertyLine("ro.arch"));
        report.append(getSystemPropertyLine("ro.hardware"));
        report.append(getSystemPropertyLine("ro.hardware.chipname"));
        report.append(getSystemPropertyLine("ro.board.platform"));
        report.append(getSystemPropertyLine("ro.build.changelist"));
        report.append(getSystemPropertyLine("ro.build.description"));
        report.append(getSystemPropertyLine("ro.build.date"));
        return report.toString();
    }
}
