package com.mobileer.oboetester;

public class NativeEngine {

    static native boolean isMMapSupported();

    static native boolean isMMapExclusiveSupported();

    static native void setWorkaroundsEnabled(boolean enabled);

    static native boolean areWorkaroundsEnabled();

    static native int getCpuCount();

    static native void setCpuAffinityMask(int mask);

    static native void setWorkloadReportingEnabled(boolean enabled);

    static native void setNotifyWorkloadIncreaseEnabled(boolean enabled);
}
