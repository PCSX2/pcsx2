package kr.co.iefriends.pcsx2.activities;

import android.content.ClipData;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.view.View;
import android.widget.VideoView;

import androidx.activity.OnBackPressedCallback;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.view.WindowCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.core.view.WindowInsetsControllerCompat;

import kr.co.iefriends.pcsx2.App;
import kr.co.iefriends.pcsx2.R;

public class BootSplashActivity extends AppCompatActivity {
    private static final long HARD_TIMEOUT_MS = 6000L;

    private boolean launchedMain;
    private View rootView;

    private final Runnable timeoutRunnable = this::launchMainAndFinish;
    private final OnBackPressedCallback onBackPressedCallback =
        new OnBackPressedCallback(true) {
            @Override
            public void handleOnBackPressed() {
                launchMainAndFinish();
            }
        };

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (!App.consumeBootSplashPlayToken()) {
            launchMainAndFinish();
            return;
        }
        getOnBackPressedDispatcher().addCallback(onBackPressedCallback);
        setContentView(R.layout.activity_boot_splash);
        rootView = findViewById(R.id.boot_splash_root);
        VideoView videoView = findViewById(R.id.boot_splash_video);

        if (rootView != null) {
            rootView.setOnClickListener(v -> launchMainAndFinish());
            rootView.postDelayed(timeoutRunnable, HARD_TIMEOUT_MS);
        }
        if (videoView != null) {
            videoView.setOnClickListener(v -> launchMainAndFinish());
            Uri videoUri = Uri.parse("android.resource://" + getPackageName() + "/" + R.raw.boot_intro);
            videoView.setVideoURI(videoUri);
            videoView.setOnPreparedListener(mp -> {
                mp.setLooping(false);
                videoView.start();
            });
            videoView.setOnCompletionListener(mp -> launchMainAndFinish());
            videoView.setOnErrorListener((mp, what, extra) -> {
                launchMainAndFinish();
                return true;
            });
        } else {
            launchMainAndFinish();
        }

        applyImmersiveUi();
    }

    @Override
    protected void onDestroy() {
        if (rootView != null) {
            rootView.removeCallbacks(timeoutRunnable);
        }
        super.onDestroy();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            applyImmersiveUi();
        }
    }

    private void applyImmersiveUi() {
        WindowCompat.setDecorFitsSystemWindows(getWindow(), false);
        WindowInsetsControllerCompat controller = new WindowInsetsControllerCompat(getWindow(), getWindow().getDecorView());
        controller.hide(WindowInsetsCompat.Type.systemBars());
        controller.setSystemBarsBehavior(WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
    }

    private void launchMainAndFinish() {
        if (launchedMain) {
            return;
        }
        launchedMain = true;

        if (rootView != null) {
            rootView.removeCallbacks(timeoutRunnable);
        }

        Intent launch = new Intent(this, MainActivity.class);
        Intent source = getIntent();
        if (source != null) {
            String action = source.getAction();
            if (action != null) {
                launch.setAction(action);
            }
            if (source.getData() != null || source.getType() != null) {
                launch.setDataAndType(source.getData(), source.getType());
            }
            if (source.getCategories() != null) {
                for (String category : source.getCategories()) {
                    launch.addCategory(category);
                }
            }
            Bundle extras = source.getExtras();
            if (extras != null) {
                launch.putExtras(extras);
            }
            ClipData clipData = source.getClipData();
            if (clipData != null) {
                launch.setClipData(clipData);
            }
            int grantFlags = source.getFlags()
                    & (Intent.FLAG_GRANT_READ_URI_PERMISSION
                    | Intent.FLAG_GRANT_WRITE_URI_PERMISSION
                    | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION
                    | Intent.FLAG_GRANT_PREFIX_URI_PERMISSION);
            launch.addFlags(grantFlags);
        }
        launch.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_SINGLE_TOP);
        startActivity(launch);
        finish();
        overridePendingTransition(0, 0);
    }
}
