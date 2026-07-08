package kr.co.iefriends.pcsx2;

import android.content.Intent;
import android.content.res.Configuration;
import android.os.Bundle;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;

import com.facebook.react.ReactInstanceManager;
import com.facebook.react.ReactInstanceManagerBuilder;
import com.facebook.react.ReactPackage;
import com.facebook.react.ReactRootView;
import com.facebook.react.common.LifecycleState;
import com.facebook.react.modules.core.DefaultHardwareBackBtnHandler;
import com.facebook.react.shell.MainReactPackage;

public class RNActivity extends AppCompatActivity implements DefaultHardwareBackBtnHandler {
    private ReactRootView reactRootView;
    private ReactInstanceManager reactInstanceManager;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        ensureReactMounted();
    }

    private void ensureReactMounted() {
        if (reactRootView != null && reactInstanceManager != null) {
            setContentView(reactRootView);
            return;
        }

        try {
            reactRootView = new ReactRootView(this);

            ReactInstanceManagerBuilder builder = ReactInstanceManager.builder()
                    .setApplication(getApplication())
                    .setCurrentActivity(this)
                    .setUseDeveloperSupport(BuildConfig.DEBUG)
                    .setInitialLifecycleState(LifecycleState.RESUMED);

            boolean addedAnyPackage = false;
            String[] candidatePackageListClasses = new String[] {
                    "com.facebook.react.PackageList",
                    BuildConfig.APPLICATION_ID + ".PackageList",
            };
            for (String className : candidatePackageListClasses) {
                if (addedAnyPackage) break;
                try {
                    Class<?> pkgListClass = Class.forName(className);
                    Object pkgList = null;
                    try {
                        java.lang.reflect.Constructor<?> ctorApp = pkgListClass.getConstructor(android.app.Application.class);
                        pkgList = ctorApp.newInstance(getApplication());
                    } catch (NoSuchMethodException nsme) {
                        try {
                            java.lang.reflect.Constructor<?> ctorNoArg = pkgListClass.getConstructor();
                            pkgList = ctorNoArg.newInstance();
                        } catch (Throwable ignored2) { /* give up */ }
                    }
                    if (pkgList != null) {
                        java.lang.reflect.Method getPackages = pkgListClass.getMethod("getPackages");
                        @SuppressWarnings("unchecked")
                        java.util.List<ReactPackage> packages = (java.util.List<ReactPackage>) getPackages.invoke(pkgList);
                        if (packages != null && !packages.isEmpty()) {
                            for (ReactPackage p : packages) builder.addPackage(p);
                            addedAnyPackage = true;
                        }
                    }
                } catch (Throwable ignored) { }
            }

            if (!addedAnyPackage) builder.addPackage(new MainReactPackage());
            if (BuildConfig.DEBUG) {
                builder.setJSMainModulePath("index");
            } else {
                builder.setBundleAssetName("index.android.bundle");
            }

            reactInstanceManager = builder.build();

            final String appName = "ARMSX2RN";
            reactRootView.startReactApplication(reactInstanceManager, appName, null);
            setContentView(reactRootView);
        } catch (Throwable t) {
            android.widget.TextView tv = new android.widget.TextView(this);
            tv.setText("React Native is not available. Start Metro and reload in debug.");
            tv.setTextSize(16f);
            tv.setGravity(android.view.Gravity.CENTER);
            setContentView(tv);
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (reactInstanceManager != null) reactInstanceManager.onHostResume(this, this);
    }

    @Override
    protected void onPause() {
        super.onPause();
        if (reactInstanceManager != null) reactInstanceManager.onHostPause(this);
    }

    @Override
    protected void onStop() {
        super.onStop();
        if (!isChangingConfigurations()) {
            teardownReact();
        }
    }

    @Override
    protected void onDestroy() {
        teardownReact();
        super.onDestroy();
    }

    private void teardownReact() {
        try {
            if (reactRootView != null) {
                reactRootView.unmountReactApplication();
            }
            if (reactInstanceManager != null) {
                reactInstanceManager.onHostDestroy(this);
                reactInstanceManager.destroy();
            }
        } catch (Throwable ignored) {

        } finally {
            reactInstanceManager = null;
            reactRootView = null;
        }
    }

    @Override
    public void onConfigurationChanged(@NonNull Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        if (reactInstanceManager != null) {
            reactInstanceManager.onConfigurationChanged(this, newConfig);
        }
    }

    @Override
    public void onBackPressed() {
        if (reactInstanceManager != null) {
            reactInstanceManager.onBackPressed();
        } else {
            super.onBackPressed();
        }
    }

    @Override
    public void invokeDefaultOnBackPressed() {
        teardownReact();
        finish();
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (reactInstanceManager != null) {
            reactInstanceManager.onActivityResult(this, requestCode, resultCode, data);
        }
    }
}
