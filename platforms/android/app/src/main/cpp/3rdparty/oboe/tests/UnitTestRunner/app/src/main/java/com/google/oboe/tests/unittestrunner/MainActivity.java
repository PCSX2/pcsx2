package com.google.oboe.tests.unittestrunner;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;

import android.Manifest;
import android.content.pm.PackageManager;
import android.content.res.AssetManager;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import android.view.WindowManager;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.Toast;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;

public class MainActivity extends AppCompatActivity {

    private final String TAG = MainActivity.class.getName();
    private static final String TEST_BINARY_FILENAME = "testOboe.so";
    private static final int APP_PERMISSION_REQUEST = 0;

    private TextView outputText;
    private ScrollView scrollView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        outputText = findViewById(R.id.output_view_text);
        scrollView = findViewById(R.id.scroll_view);
        runCommand();
    }

    private void runCommand(){
        if (!isRecordPermissionGranted()){
            requestPermissions();
        } else {
            Log.d(TAG, "Got RECORD_AUDIO permission");
            Thread commandThread = new Thread(new UnitTestCommand());
            commandThread.start();
        }
    }

    private String executeBinary() {
        StringBuilder output = new StringBuilder();

        try {
            String executablePath;
            if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.O) {
                executablePath = getApplicationInfo().nativeLibraryDir + "/" + TEST_BINARY_FILENAME;
            } else {
                executablePath = getExecutablePathFromAssets();
            }

            Log.d(TAG, "Attempting to execute " + executablePath);

            Process process = Runtime.getRuntime().exec(executablePath);

            BufferedReader stdInput = new BufferedReader(new
                    InputStreamReader(process.getInputStream()));

            BufferedReader stdError = new BufferedReader(new
                    InputStreamReader(process.getErrorStream()));

            // read the output from the command
            String s;
            while ((s = stdInput.readLine()) != null) {
                Log.d(TAG, s);
                output.append(s).append("\n");
            }

            // read any errors from the attempted command
            while ((s = stdError.readLine()) != null) {
                Log.e(TAG, "ERROR: " + s);
                output.append("ERROR: ").append(s).append("\n");
            }

            process.waitFor();
            Log.d(TAG, "Finished executing binary");
        } catch (IOException e){
            Log.e(TAG, "Could not execute binary ", e);
        } catch (InterruptedException e) {
            Log.e(TAG, "Interrupted", e);
        }

        return output.toString();
    }

    // Legacy method to get asset path.
    // This will not work on more recent Android releases.
    private String getExecutablePathFromAssets() {
        AssetManager assetManager = getAssets();

        String abi = Build.SUPPORTED_ABIS[0];
        String extraStringForDebugBuilds = "-hwasan";
        if (abi.endsWith(extraStringForDebugBuilds)) {
            abi = abi.substring(0, abi.length() - extraStringForDebugBuilds.length());
        }
        String filesDir = getFilesDir().getPath();
        String testBinaryPath = abi + "/" + TEST_BINARY_FILENAME;

        try {
            InputStream inStream = assetManager.open(testBinaryPath);
            Log.d(TAG, "Opened " + testBinaryPath);

            // Copy this file to an executable location
            File outFile = new File(filesDir, TEST_BINARY_FILENAME);

            OutputStream outStream = new FileOutputStream(outFile);

            byte[] buffer = new byte[1024];
            int read;
            while ((read = inStream.read(buffer)) != -1) {
                outStream.write(buffer, 0, read);
            }
            inStream.close();
            outStream.flush();
            outStream.close();
            Log.d(TAG, "Copied " + testBinaryPath + " to " + filesDir);

            String executablePath = filesDir + "/" + TEST_BINARY_FILENAME;
            Log.d(TAG, "Setting execute permission on " + executablePath);
            boolean success = new File(executablePath).setExecutable(true, false);
            if (!success) {
                Log.d(TAG, "Could not set execute permission on " + executablePath);
            }
            return executablePath;
        } catch (IOException e) {
            e.printStackTrace();
        }
        return "";
    }

    private boolean isRecordPermissionGranted() {
        return (ActivityCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO) ==
                PackageManager.PERMISSION_GRANTED);
    }

    private void requestPermissions(){
        ActivityCompat.requestPermissions(
                this,
                new String[]{Manifest.permission.RECORD_AUDIO},
                APP_PERMISSION_REQUEST);
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions,
                                           @NonNull int[] grantResults) {

        if (APP_PERMISSION_REQUEST != requestCode) {
            super.onRequestPermissionsResult(requestCode, permissions, grantResults);
            return;
        }

        if (grantResults.length != 1 ||
                grantResults[0] != PackageManager.PERMISSION_GRANTED) {

            // User denied the permission, without this we cannot record audio
            // Show a toast and update the status accordingly
            outputText.setText(R.string.status_record_audio_denied);
            Toast.makeText(getApplicationContext(),
                    getString(R.string.need_record_audio_permission),
                    Toast.LENGTH_SHORT)
                    .show();
        } else {
            // Permission was granted, run the command
            outputText.setText(R.string.status_record_audio_granted);
            runCommand();
        }
    }

    class UnitTestCommand implements Runnable {

        @Override
        public void run() {
            final String output = executeBinary();

            runOnUiThread(() -> {
                outputText.setText(output);

                // Scroll to the bottom so we can see the test result
                scrollView.postDelayed(() -> scrollView.scrollTo(0, outputText.getBottom()), 100);
            });
        }
    }
}
