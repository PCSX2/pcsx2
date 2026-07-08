
/*

By MoonPower (Momo-AUX1) GPLv3 License
   This file is part of ARMSX2.

   ARMSX2 is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   ARMSX2 is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with ARMSX2.  If not, see <http://www.gnu.org/licenses/>.

*/

package kr.co.iefriends.pcsx2.utils;

import android.content.Context;
import android.util.Log;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
public final class LogcatRecorder {
	private static final String TAG = "LogcatRecorder";
	private static final Object LOCK = new Object();

	private static Context sAppContext;
	private static boolean sCaptureRequested;
	private static boolean sIsRunning;
	private static Process sLogcatProcess;
	private static Thread sPumpThread;

	private LogcatRecorder() {}

	public static void initialize(Context context) {
		if (context == null)
			return;

		synchronized (LOCK) {
			if (sAppContext == null)
				sAppContext = context.getApplicationContext();
		}
	}

	public static void setEnabled(boolean enable) {
		synchronized (LOCK) {
			sCaptureRequested = enable;
			if (enable) {
				if (!sIsRunning)
					sIsRunning = startCaptureLocked(true);
			} else if (sIsRunning) {
				stopCaptureLocked();
			}
		}
	}

	static boolean isEnabled() {
		synchronized (LOCK) {
			return sCaptureRequested;
		}
	}

	public static void handleDataRootChanged() {
		synchronized (LOCK) {
			if (!sIsRunning)
				return;

			stopCaptureLocked();
			if (sCaptureRequested)
				sIsRunning = startCaptureLocked(true);
		}
	}

	public static void shutdown() {
		synchronized (LOCK) {
			sCaptureRequested = false;
			if (sIsRunning)
				stopCaptureLocked();
			sAppContext = null;
		}
	}

	private static boolean startCaptureLocked(boolean resetFile) {
		if (sAppContext == null)
			return false;

		final File dataRoot = DataDirectoryManager.getDataRoot(sAppContext);
		if (dataRoot == null) {
			Log.w(TAG, "Unable to resolve data directory; skipping logcat capture.");
			return false;
		}

		final File outFile = new File(dataRoot, "ANDROID_LOG.txt");
		ensureParent(outFile);
		if (resetFile) {
			try {
				if (outFile.exists() && !outFile.delete())
					Log.w(TAG, "Unable to clear previous ANDROID_LOG.txt contents.");
			} catch (SecurityException e) {
				Log.w(TAG, "Failed to clear existing logcat file.", e);
			}
		}

		try {
			sLogcatProcess = new ProcessBuilder("logcat", "-v", "threadtime", "*:V")
				.redirectErrorStream(true)
				.start();
		} catch (IOException e) {
			Log.e(TAG, "Failed to start logcat capture.", e);
			sLogcatProcess = null;
			return false;
		}

		sPumpThread = new Thread(() -> pumpLogcat(outFile), "ARMSX2-Logcat");
		sPumpThread.setDaemon(true);
		sPumpThread.start();
		return true;
	}

	private static void pumpLogcat(File outFile) {
		final Process proc;
		synchronized (LOCK) {
			proc = sLogcatProcess;
		}
		if (proc == null)
			return;

		try (BufferedInputStream in = new BufferedInputStream(proc.getInputStream());
		     FileOutputStream fos = new FileOutputStream(outFile, true);
		     BufferedOutputStream bos = new BufferedOutputStream(fos)) {

			byte[] buffer = new byte[8192];
			int read;
			while ((read = in.read(buffer)) != -1) {
				bos.write(buffer, 0, read);
				bos.flush();
			}
		} catch (IOException e) {
			Log.w(TAG, "Logcat capture terminated.", e);
		} finally {
			synchronized (LOCK) {
				if (sLogcatProcess != null) {
					try {
						sLogcatProcess.destroy();
					} catch (Throwable ignored) {}
					sLogcatProcess = null;
				}
				sPumpThread = null;
				sIsRunning = false;
				if (sCaptureRequested && sAppContext != null)
					sIsRunning = startCaptureLocked(false);
			}
		}
	}

	private static void stopCaptureLocked() {
		if (sLogcatProcess != null) {
			try {
				sLogcatProcess.destroy();
			} catch (Throwable ignored) {}
			sLogcatProcess = null;
		}
		if (sPumpThread != null) {
			sPumpThread.interrupt();
			sPumpThread = null;
		}
		sIsRunning = false;
	}

	private static void ensureParent(File outFile) {
		final File parent = outFile.getParentFile();
		if (parent != null && !parent.exists() && !parent.mkdirs())
			Log.w(TAG, "Unable to create directory for logcat output: " + parent);
	}
}
