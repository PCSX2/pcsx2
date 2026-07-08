/*
 * Copyright 2022 The Android Open Source Project
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

import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.View;
import android.widget.TextView;

import androidx.appcompat.app.AppCompatActivity;

public class TestErrorCallbackActivity extends AppCompatActivity {

    private TextView mStatusDeleteCallback;
    // This must match the value in TestErrorCallback.h
    private static final int MAGIC_GOOD = 0x600DCAFE;
    private MyStreamSniffer mStreamSniffer;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_error_callback);

        mStreamSniffer = new MyStreamSniffer();
        mStatusDeleteCallback = (TextView) findViewById(R.id.text_callback_status);
    }

    public void onTestDeleteCrash(View view) {
        mStatusDeleteCallback.setText(getString(R.string.plug_or_unplug));
        mStreamSniffer.startStreamSniffer();
        testDeleteCrash();
    }


    // Periodically query the status of the streams.
    protected class MyStreamSniffer {
        public static final int SNIFFER_UPDATE_PERIOD_MSEC = 150;
        public static final int SNIFFER_UPDATE_DELAY_MSEC = 300;

        private Handler mHandler;

        // Display status info for the stream.
        private Runnable runnableCode = new Runnable() {
            @Override
            public void run() {
                int magic = getCallbackMagic();
                updateMagicDisplay(magic);
                mHandler.postDelayed(runnableCode, SNIFFER_UPDATE_PERIOD_MSEC);
            }
        };

        private void startStreamSniffer() {
            stopStreamSniffer();
            mHandler = new Handler(Looper.getMainLooper());
            // Start the initial runnable task by posting through the handler
            mHandler.postDelayed(runnableCode, SNIFFER_UPDATE_DELAY_MSEC);
        }

        private void stopStreamSniffer() {
            if (mHandler != null) {
                mHandler.removeCallbacks(runnableCode);
            }
        }
    }

    private void updateMagicDisplay(int magic) {
        if (magic != 0) {
            String text = getString(R.string.report_magic_pass, MAGIC_GOOD);
            if (magic != MAGIC_GOOD) {
                text = getString(R.string.report_magic_fail,
                        magic, MAGIC_GOOD);
            }
            mStatusDeleteCallback.setText(text);
        }
    }

    @Override
    public void onPause() {
        super.onPause();
        mStreamSniffer.stopStreamSniffer();
    }

    private native void testDeleteCrash();
    private native int getCallbackMagic();

}
