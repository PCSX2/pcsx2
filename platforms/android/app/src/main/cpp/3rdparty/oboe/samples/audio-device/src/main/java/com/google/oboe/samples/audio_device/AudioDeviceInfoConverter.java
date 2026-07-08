package com.google.oboe.samples.audio_device;
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

import android.media.AudioDeviceInfo;

class AudioDeviceInfoConverter {

    /**
     * Converts an {@link AudioDeviceInfo} object into a human readable representation
     *
     * @param adi The AudioDeviceInfo object to be converted to a String
     * @return String containing all the information from the AudioDeviceInfo object
     */
    static String toString(AudioDeviceInfo adi){

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
        sb.append(intArrayToString(channelMasks));

        sb.append("\nChannel index masks: ");
        int[] channelIndexMasks = adi.getChannelIndexMasks();
        sb.append(intArrayToString(channelIndexMasks));

        sb.append("\nEncodings: ");
        int[] encodings = adi.getEncodings();
        sb.append(intArrayToString(encodings));

        sb.append("\nSample Rates: ");
        int[] sampleRates = adi.getSampleRates();
        sb.append(intArrayToString(sampleRates));

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
            if (i != integerArray.length -1) sb.append(" ");
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
    static String typeToString(int type){
        switch (type) {
            case AudioDeviceInfo.TYPE_AUX_LINE:
                return "auxiliary line-level connectors";
            case AudioDeviceInfo.TYPE_BLUETOOTH_A2DP:
                return "Bluetooth device supporting the A2DP profile";
            case AudioDeviceInfo.TYPE_BLUETOOTH_SCO:
                return "Bluetooth device typically used for telephony";
            case AudioDeviceInfo.TYPE_BUILTIN_EARPIECE:
                return "built-in earphone speaker";
            case AudioDeviceInfo.TYPE_BUILTIN_MIC:
                return "built-in microphone";
            case AudioDeviceInfo.TYPE_BUILTIN_SPEAKER:
                return "built-in speaker";
            case AudioDeviceInfo.TYPE_BUS:
                return "BUS";
            case AudioDeviceInfo.TYPE_DOCK:
                return "DOCK";
            case AudioDeviceInfo.TYPE_FM:
                return "FM";
            case AudioDeviceInfo.TYPE_FM_TUNER:
                return "FM tuner";
            case AudioDeviceInfo.TYPE_HDMI:
                return "HDMI";
            case AudioDeviceInfo.TYPE_HDMI_ARC:
                return "HDMI audio return channel";
            case AudioDeviceInfo.TYPE_IP:
                return "IP";
            case AudioDeviceInfo.TYPE_LINE_ANALOG:
                return "line analog";
            case AudioDeviceInfo.TYPE_LINE_DIGITAL:
                return "line digital";
            case AudioDeviceInfo.TYPE_TELEPHONY:
                return "telephony";
            case AudioDeviceInfo.TYPE_TV_TUNER:
                return "TV tuner";
            case AudioDeviceInfo.TYPE_USB_ACCESSORY:
                return "USB accessory";
            case AudioDeviceInfo.TYPE_USB_DEVICE:
                return "USB device";
            case AudioDeviceInfo.TYPE_WIRED_HEADPHONES:
                return "wired headphones";
            case AudioDeviceInfo.TYPE_WIRED_HEADSET:
                return "wired headset";
            default:
            case AudioDeviceInfo.TYPE_UNKNOWN:
                return "unknown";
        }
    }
}
