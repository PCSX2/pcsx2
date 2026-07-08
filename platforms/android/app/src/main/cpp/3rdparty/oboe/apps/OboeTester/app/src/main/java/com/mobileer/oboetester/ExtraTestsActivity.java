package com.mobileer.oboetester;

import android.os.Bundle;
import android.view.View;

public class ExtraTestsActivity extends BaseOboeTesterActivity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_extra_tests);
    }

    public void onLaunchMainActivity(View view) {
        launchTestActivity(MainActivity.class);
    }

    public void onLaunchExternalTapTest(View view) {
        launchTestThatDoesRecording(ExternalTapToToneActivity.class);
    }

    public void onLaunchPlugLatencyTest(View view) {
        launchTestActivity(TestPlugLatencyActivity.class);
    }

    public void onLaunchErrorCallbackTest(View view) {
        launchTestActivity(TestErrorCallbackActivity.class);
    }

    public void onLaunchRouteDuringCallbackTest(View view) {
        launchTestThatDoesRecording(TestRouteDuringCallbackActivity.class);
    }

    public void onLaunchDynamicWorkloadTest(View view) {
        launchTestActivity(DynamicWorkloadActivity.class);
    }

    public void onLaunchColdStartLatencyTest(View view) {
        launchTestActivity(TestColdStartLatencyActivity.class);
    }

    public void onLaunchRapidCycleTest(View view) {
        launchTestActivity(TestRapidCycleActivity.class);
    }

    public void onLaunchAudioWorkloadTest(View view) {
        launchTestActivity(AudioWorkloadTestActivity.class);
    }

    public void onLaunchAudioWorkloadTestRunner(View view) {
        launchTestActivity(AudioWorkloadTestRunnerActivity.class);
    }

    public void onLaunchReverseJniTest(View view) {
        launchTestActivity(ReverseJniActivity.class);
    }
}
