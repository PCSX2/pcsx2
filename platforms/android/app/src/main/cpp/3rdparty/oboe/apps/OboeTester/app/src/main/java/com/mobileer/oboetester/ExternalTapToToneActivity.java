package com.mobileer.oboetester;

import android.os.Bundle;
import android.view.View;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;

import java.io.IOException;

/**
 * Measure the tap-to-tone latency for other apps or devices.
 */
public class ExternalTapToToneActivity extends AppCompatActivity {
    private static final String TAG = "OboeTester";

    protected TapToToneTester mTapToToneTester;
    private Button mStopButton;
    private Button mStartButton;
    private Button mAnalyzeButton;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_external_tap_to_tone);

        mTapToToneTester = new TapToToneTester(this,
                getResources().getString(R.string.external_tap_instructions));

        mStartButton = (Button) findViewById(R.id.button_start);
        mStopButton = (Button) findViewById(R.id.button_stop);
        mAnalyzeButton = (Button) findViewById(R.id.button_analyze);
        updateButtons(false);
    }

    private void updateButtons(boolean running) {
        mStartButton.setEnabled(!running);
        mAnalyzeButton.setEnabled(running);
        mStopButton.setEnabled(running);
    }

    public void analyseAndShowResults() {
        TapToToneTester.TestResult result = mTapToToneTester.analyzeCapturedAudio();
        if (result != null) {
            mTapToToneTester.showTestResults(result);
        }
    }

    public void analyze(View view) {
        analyseAndShowResults();
    }

    public void startTest(View view)  {
        try {
            mTapToToneTester.resetLatency();
            mTapToToneTester.start();
            updateButtons(true);
            getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        } catch (IOException e) {
            e.printStackTrace();
            showErrorToast("Start audio failed! " + e.getMessage());
            return;
        }
    }

    public void stopTest(View view) {
        mTapToToneTester.stop();
        updateButtons(false);
        getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
    }

    @Override
    public void onStop() {
        mTapToToneTester.stop();
        getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        super.onStop();
    }


    protected void showErrorToast(String message) {
        showToast("Error: " + message);
    }

    protected void showToast(final String message) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                Toast.makeText(ExternalTapToToneActivity.this,
                        message,
                        Toast.LENGTH_SHORT).show();
            }
        });
    }


}
