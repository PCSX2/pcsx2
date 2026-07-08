package com.mobileer.audio_device;
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

import android.media.AudioDescriptor;
import android.media.AudioDeviceInfo;
import android.media.AudioManager;
import android.media.AudioMixerAttributes;
import android.media.AudioProfile;
import android.os.Build;

import java.util.List;
import java.util.Locale;

public class AudioDeviceInfoConverter {

    /**
     * Converts an {@link AudioDeviceInfo} object into a human readable representation
     *
     * @param adi The AudioDeviceInfo object to be converted to a String
     * @return String containing all the information from the AudioDeviceInfo object
     */
    public static String toString(AudioManager audioManager, AudioDeviceInfo adi){

        StringBuilder sb = new StringBuilder();
        sb.append("Id: ");
        sb.append(adi.getId());

        sb.append("\nProduct name: ");
        sb.append(adi.getProductName());

        sb.append("\nType: ");
        sb.append(typeToString(adi.getType()));

        sb.append("\nIs source: ");
        sb.append((adi.isSource() ? "Yes" : "No"));

        sb.append("\nIs sink: ");
        sb.append((adi.isSink() ? "Yes" : "No"));

        sb.append("\nChannel counts: ");
        int[] channelCounts = adi.getChannelCounts();
        sb.append(intArrayToString(channelCounts));

        sb.append("\nChannel masks: ");
        int[] channelMasks = adi.getChannelMasks();
        sb.append(intArrayToStringHex(channelMasks));

        sb.append("\nChannel index masks: ");
        int[] channelIndexMasks = adi.getChannelIndexMasks();
        sb.append(intArrayToStringHex(channelIndexMasks));

        sb.append("\nEncodings: ");
        int[] encodings = adi.getEncodings();
        sb.append(intArrayToString(encodings));

        sb.append("\nSample Rates: ");
        int[] sampleRates = adi.getSampleRates();
        sb.append(intArrayToString(sampleRates));

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            sb.append("\nAddress: ");
            sb.append(adi.getAddress());
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            sb.append("\nEncapsulation Metadata Types: ");
            int[] encapsulationMetadataTypes = adi.getEncapsulationMetadataTypes();
            sb.append(intArrayToString(encapsulationMetadataTypes));
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            sb.append("\nEncapsulation Modes: ");
            int[] encapsulationModes = adi.getEncapsulationModes();
            sb.append(intArrayToString(encapsulationModes));
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            sb.append("\nAudio Descriptors: ");
            List<AudioDescriptor> audioDescriptors = adi.getAudioDescriptors();
            sb.append(audioDescriptors);
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            sb.append("\nAudio Profiles: ");
            List<AudioProfile> audioProfiles = adi.getAudioProfiles();
            sb.append(audioProfiles);
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
            sb.append("\nSupported Mixer Attributes: ");
            List<AudioMixerAttributes> audioMixerAttributes =
                    audioManager.getSupportedMixerAttributes(adi);
            sb.append(audioMixerAttributes);
        }

        sb.append("\n");
        return sb.toString();
    }

    /**
     * Converts an integer array into a string where each int is separated by a space
     *
     * @param integerArray the integer array to convert to a string
     * @return string containing all the integer values separated by spaces
     */
    private static String intArrayToString(int[] integerArray){
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < integerArray.length; i++){
            sb.append(integerArray[i]);
            if (i != integerArray.length - 1) sb.append(" ");
        }
        return sb.toString();
    }

    /**
     * Converts an integer array into a hexadecimal string where each int is separated by a space
     *
     * @param integerArray the integer array to convert to a string
     * @return string containing all the integer values separated by spaces
     */
    private static String intArrayToStringHex(int[] integerArray){
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < integerArray.length; i++){
            sb.append(String.format(Locale.getDefault(), "0x%02X", integerArray[i]));
            if (i != integerArray.length - 1) sb.append(" ");
        }
        return sb.toString();
    }

    /**
     * Converts the value from {@link AudioDeviceInfo#getType()} into a human
     * readable string
     * @param type One of the {@link AudioDeviceInfo}.TYPE_* values
     *             e.g. AudioDeviceInfo.TYPE_BUILT_IN_SPEAKER
     * @return string which describes the type of audio device
     */
    public static String typeToString(int type){
        switch (type) {
            case AudioDeviceInfo.TYPE_AUX_LINE:
                return "auxiliary line-level connectors";
            case AudioDeviceInfo.TYPE_BLE_BROADCAST:
                return "BLE broadcast";
            case AudioDeviceInfo.TYPE_BLE_HEADSET:
                return "BLE headset";
            case AudioDeviceInfo.TYPE_BLE_SPEAKER:
                return "BLE speaker";
            case AudioDeviceInfo.TYPE_BLUETOOTH_A2DP:
                return "Bluetooth A2DP";
            case AudioDeviceInfo.TYPE_BLUETOOTH_SCO:
                return "Bluetooth telephony SCO";
            case AudioDeviceInfo.TYPE_BUILTIN_EARPIECE:
                return "built-in earpiece";
            case AudioDeviceInfo.TYPE_BUILTIN_MIC:
                return "built-in microphone";
            case AudioDeviceInfo.TYPE_BUILTIN_SPEAKER:
                return "built-in speaker";
            case AudioDeviceInfo.TYPE_BUILTIN_SPEAKER_SAFE:
                return "built-in speaker safe";
            case AudioDeviceInfo.TYPE_BUS:
                return "bus";
            case AudioDeviceInfo.TYPE_DOCK:
                return "dock";
            case 31: // AudioDeviceInfo.TYPE_DOCK_ANALOG:
                return "dock analog";
            case 28: // AudioDeviceInfo.TYPE_ECHO_REFERENCE:
                return "echo reference";
            case AudioDeviceInfo.TYPE_FM:
                return "FM";
            case AudioDeviceInfo.TYPE_FM_TUNER:
                return "FM tuner";
            case AudioDeviceInfo.TYPE_HDMI:
                return "HDMI";
            case AudioDeviceInfo.TYPE_HEARING_AID:
                return "hearing aid";
            case AudioDeviceInfo.TYPE_HDMI_ARC:
                return "HDMI audio return channel";
            case AudioDeviceInfo.TYPE_HDMI_EARC:
                return "HDMI enhanced ARC";
            case AudioDeviceInfo.TYPE_IP:
                return "IP";
            case AudioDeviceInfo.TYPE_LINE_ANALOG:
                return "line analog";
            case AudioDeviceInfo.TYPE_LINE_DIGITAL:
                return "line digital";
            case AudioDeviceInfo.TYPE_REMOTE_SUBMIX:
                return "remote submix";
            case AudioDeviceInfo.TYPE_TELEPHONY:
                return "telephony";
            case AudioDeviceInfo.TYPE_TV_TUNER:
                return "TV tuner";
            case AudioDeviceInfo.TYPE_USB_ACCESSORY:
                return "USB accessory";
            case AudioDeviceInfo.TYPE_USB_DEVICE:
                return "USB device";
            case AudioDeviceInfo.TYPE_USB_HEADSET:
                return "USB headset";
            case AudioDeviceInfo.TYPE_WIRED_HEADPHONES:
                return "wired headphones";
            case AudioDeviceInfo.TYPE_WIRED_HEADSET:
                return "wired headset";
            default:
            case AudioDeviceInfo.TYPE_UNKNOWN:
                return "unknown=" + type;
        }
    }
}
