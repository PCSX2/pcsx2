package com.google.oboe.samples.hellooboe;

import android.os.Handler;
import android.os.HandlerThread;
import android.os.Message;

/** Run Audio function in a background thread.
 * This will avoid ANRs that can occur if the AudioServer or HAL crashes
 * and takes a long time to recover.
 *
 * This thread will run for the lifetime of the app so only make one BackgroundRunner
 * and reuse it.
 */
public abstract class BackgroundRunner {
    private Handler mHandler;
    private HandlerThread mThread = new HandlerThread("BackgroundRunner");

    public BackgroundRunner() {
        mThread.start();
        mHandler = new Handler(mThread.getLooper()) {
            public void handleMessage(Message message) {
                super.handleMessage(message);
                handleMessageInBackground(message);
            }
        };
    }

    /**
     * @param message
     * @return true if the message was successfully queued
     */
    public boolean sendMessage(Message message) {
        return mHandler.sendMessage(message);
    }

    /**
     * @param what command code for background operation
     * @param arg1 optional argument
     * @return true if the message was successfully queued
     */
    public boolean sendMessage(int what, int arg1) {
        Message message = mHandler.obtainMessage();
        message.what = what;
        message.arg1 = arg1;
        return sendMessage(message);
    }

    public boolean sendMessage(int what) {
        return sendMessage(what, 0);
    }

    /**
     * Implement this in your app.
     */
    abstract void handleMessageInBackground(Message message);
}
