package kr.co.iefriends.pcsx2;

import android.content.ContentResolver;
import android.content.Context;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.text.TextUtils;
import android.view.Surface;
import java.io.File;
import java.lang.ref.WeakReference;

import kr.co.iefriends.pcsx2.activities.MainActivity;
import kr.co.iefriends.pcsx2.utils.DataDirectoryManager;

public class NativeApp {
	static {
		try {
			System.loadLibrary("emucore");
			hasNoNativeBinary = false;
		} catch (UnsatisfiedLinkError e) {
			hasNoNativeBinary = true;
		}
		
		// Try to load the native tools library and fall back if we cant
		try {
			System.loadLibrary("armsx2_native_tools");
			hasNativeTools = true;
		} catch (UnsatisfiedLinkError e) {
			hasNativeTools = false;
		}
	}

	public static boolean hasNoNativeBinary;
	public static boolean hasNativeTools;


	protected static WeakReference<Context> mContext;
	private static String sDataRootOverride;
	public static Context getContext() {
		return mContext.get();
	}

	public static void initializeOnce(Context context) {
		mContext = new WeakReference<>(context);
		File externalFilesDir = null;
		if (!TextUtils.isEmpty(sDataRootOverride)) {
			externalFilesDir = new File(sDataRootOverride);
			if (externalFilesDir != null && !externalFilesDir.exists()) {
				externalFilesDir.mkdirs();
			}
		}
		if (externalFilesDir == null) {
			externalFilesDir = context.getExternalFilesDir(null);
		}
		if (externalFilesDir == null) {
			externalFilesDir = context.getDataDir();
		}
		initialize(externalFilesDir.getAbsolutePath(), android.os.Build.VERSION.SDK_INT);
	}

	public static void setDataRootOverride(String path) {
		sDataRootOverride = path;
	}

	public static void ensureResourceSubdirectoryCopied(String relativePath) {
		Context context = getContext();
		if (context == null) {
			return;
		}
		String assetPath = "resources";
		if (!TextUtils.isEmpty(relativePath)) {
			assetPath = assetPath + "/" + relativePath;
		}
		DataDirectoryManager.copyAssetAll(context, assetPath);
	}

	public static void reinitializeDataRoot(String path) {
		if (hasNoNativeBinary || TextUtils.isEmpty(path)) {
			return;
		}
		reloadDataRoot(path);
	}

	public static native void initialize(String path, int apiVer);
	private static native void reloadDataRoot(String path);
	public static native String getGameTitle(String path);
	public static native String getGameSerial();
	public static native int getGameCRC();
	public static native void reloadCheats();
	public static native boolean hasWidescreenPatch();
	public static native float getFPS();

	public static native String getPauseGameTitle();
	public static native String getPauseGameSerial();

	public static native void setPadVibration(boolean isonoff);
	public static native void setPadButton(int index, int range, boolean iskeypressed);
	public static native void resetKeyStatus();

	public static native void setAspectRatio(int type);
	public static native void setEnableCheats(boolean isonoff);
	public static native void speedhackLimitermode(int value);
	public static native void speedhackEecyclerate(int value);
	public static native void speedhackEecycleskip(int value);

	public static native void renderUpscalemultiplier(float value);
	public static native void renderMipmap(int value);
	public static native void renderHalfpixeloffset(int value);
	public static native void renderGpu(int value);
	public static native void renderPreloading(int value);


	public static native void setSetting(String section, String key, String type, String value);
	public static native String getSetting(String section, String key, String type);

	public static native void onNativeSurfaceCreated();
	public static native void onNativeSurfaceChanged(Surface surface, int w, int h);
	public static native void onNativeSurfaceDestroyed();

	public static native boolean runVMThread(String path);

	public static native void pause();
	public static native void resume();
	public static native void shutdown();

	public static native boolean saveStateToSlot(int slot);
	public static native boolean loadStateFromSlot(int slot);
	public static native String getGamePathSlot(int slot);
	public static native byte[] getImageSlot(int slot);

	public static int openContentUri(String uriString) {
		Context _context = getContext();
		if(_context != null) {
			ContentResolver _contentResolver = _context.getContentResolver();
			try {
				int pipe = uriString.indexOf('|');
				if (pipe > 0) {
					uriString = uriString.substring(0, pipe);
				}
				ParcelFileDescriptor filePfd = _contentResolver.openFileDescriptor(Uri.parse(uriString), "r");
				if (filePfd != null) {
					return filePfd.detachFd();  
				}
			} catch (Exception ignored) {}
		}
		return -1;
	}

	public static native void refreshBIOS();
	public static native boolean hasValidVm();

    public static native boolean isFullscreenUIEnabled();

    public static void onPadVibration(int padIndex, float large, float small) {
        MainActivity.requestControllerRumble(large, small);
    }
    
    // Native tools for ISO to CHD conversion (and eventually more soon)
    public static native int convertIsoToChd(String inputIsoPath);
    
    public static native void setCustomDriverPath(String path);
    public static native String getCustomDriverPath();
    public static native void setNativeLibraryDir(String path);
	public static native boolean changeDisc(String path);
	public static native String getDiskInfo(String path);
}
