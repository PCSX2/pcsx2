package com.mobileer.oboetester;

import android.annotation.TargetApi;
import android.media.MicrophoneInfo;
import android.os.Build;
import android.util.Pair;

import java.util.List;
import java.util.Locale;

public class MicrophoneInfoConverter {
    @TargetApi(Build.VERSION_CODES.P)
    static String convertDirectionality(int directionality) {
        switch(directionality) {
            case MicrophoneInfo.DIRECTIONALITY_BI_DIRECTIONAL:
                return "Bidirectional";
            case MicrophoneInfo.DIRECTIONALITY_OMNI:
                return "Omni";
            case MicrophoneInfo.DIRECTIONALITY_CARDIOID:
                return "Cardioid";
            case MicrophoneInfo.DIRECTIONALITY_SUPER_CARDIOID:
                return "SuperCardioid";
            case MicrophoneInfo.DIRECTIONALITY_HYPER_CARDIOID:
                return "HyperCardioid";
            default:
                return "Unknown";
        }
    }

    @TargetApi(Build.VERSION_CODES.P)
    static String convertLocation(int location) {
        switch(location) {
            case MicrophoneInfo.LOCATION_MAINBODY:
                return "Main Body";
            case MicrophoneInfo.LOCATION_MAINBODY_MOVABLE:
                return "Main Body Movable";
            case MicrophoneInfo.LOCATION_PERIPHERAL:
                return "Peripheral";
            default:
                return "Unknown";
        }
    }

    @TargetApi(Build.VERSION_CODES.P)
    static String convertCoordinates(MicrophoneInfo.Coordinate3F coordinates) {
        if (coordinates == MicrophoneInfo.POSITION_UNKNOWN) return "Unknown";
        return String.format(Locale.getDefault(), "{ %6.4g, %5.3g, %5.3g }",
                coordinates.x, coordinates.y, coordinates.z);
    }

    @TargetApi(Build.VERSION_CODES.P)
    public static String reportMicrophoneInfo(MicrophoneInfo micInfo) {
        StringBuffer sb = new StringBuffer();
        sb.append("\n==== Microphone ========= " + micInfo.getId());
        sb.append("\nAddress    : " + micInfo.getAddress());
        sb.append("\nDescription: " + micInfo.getDescription());
        sb.append("\nDirection  : " + convertDirectionality(micInfo.getDirectionality()));
        sb.append("\nLocation   : " + convertLocation(micInfo.getLocation()));
        sb.append("\nMinSPL     : " + micInfo.getMinSpl());
        sb.append("\nMaxSPL     : " + micInfo.getMaxSpl());
        sb.append("\nSensitivity: " + micInfo.getSensitivity());
        sb.append("\nGroup      : " + micInfo.getGroup());
        sb.append("\nIndexInTheGroup: " + micInfo.getIndexInTheGroup());
        sb.append("\nOrientation: " + convertCoordinates(micInfo.getOrientation()));
        sb.append("\nPosition   : " + convertCoordinates(micInfo.getPosition()));
        sb.append("\nType       : " + micInfo.getType());

        List<Pair<Integer, Integer>> mapping = micInfo.getChannelMapping();
        if (mapping == null) {
            throw new RuntimeException("MicrophoneInfo. getChannelMapping() returned null!");
        }
        sb.append("\nChannelMapping: {");
        for (Pair<Integer, Integer> pair : mapping) {
            sb.append("[" + pair.first + "," + pair.second + "], ");
        }
        sb.append("}");

        sb.append("\n");
        return sb.toString();
    }

}
