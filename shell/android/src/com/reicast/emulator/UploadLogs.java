package com.reicast.emulator;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Build;

public class UploadLogs extends AsyncTask<String, Integer, String> {
	
	public static final String build_model = android.os.Build.MODEL;
	public static final String build_device = android.os.Build.DEVICE;
	public static final String build_board = android.os.Build.BOARD;
	public static final int build_sdk = android.os.Build.VERSION.SDK_INT;

	public static final String DN = "Donut";
	public static final String EC = "Eclair";
	public static final String FR = "Froyo";
	public static final String GB = "Gingerbread";
	public static final String HC = "Honeycomb";
	public static final String IC = "Ice Cream Sandwich";
	public static final String JB = "JellyBean";
	public static final String KK = "KitKat";
	public static final String NF = "Not Found";
	
	private String unHandledIOE;

	private Context mContext;

	public UploadLogs(Context mContext) {
		this.mContext = mContext;
	}

	@SuppressLint("NewApi")
	protected void onPreExecute() {
		
	}
	
	private String discoverCPUData() {
		String s = "MODEL: " + Build.MODEL;
		s += "\r\n";
		s += "DEVICE: " + build_device;
		s += "\r\n";
		s += "BOARD: " + build_board;
		s += "\r\n";
		if (String.valueOf(build_sdk) != null) {
			String build_version = NF;
			if (build_sdk >= 4 && build_sdk < 7) {
				build_version = DN;
			} else if (build_sdk == 7) {
				build_version = EC;
			} else if (build_sdk == 8) {
				build_version = FR;
			} else if (build_sdk >= 9 && build_sdk < 11) {
				build_version = GB;
			} else if (build_sdk >= 11 && build_sdk < 14) {
				build_version = HC;
			} else if (build_sdk >= 14 && build_sdk < 16) {
				build_version = IC;
			} else if (build_sdk >= 16 && build_sdk < 19) {
				build_version = JB;
			} else if (build_sdk >= 19) {
				build_version = KK;
			}
			s += build_version + " (" + build_sdk + ")";
		} else {
			String prop_build_version = "ro.build.version.release";
			String prop_sdk_version = "ro.build.version.sdk";
			String build_version = readOutput("/system/bin/getprop "
					+ prop_build_version);
			String sdk_version = readOutput("/system/bin/getprop "
					+ prop_sdk_version);
			s += build_version + " (" + sdk_version + ")";
		}
		return s;
	}

	public static String readOutput(String command) {
		try {
			Process p = Runtime.getRuntime().exec(command);
			InputStream is = null;
			if (p.waitFor() == 0) {
				is = p.getInputStream();
			} else {
				is = p.getErrorStream();
			}
			BufferedReader br = new BufferedReader(new InputStreamReader(is),
					2048);
			String line = br.readLine();
			br.close();
			return line;
		} catch (Exception ex) {
			return "ERROR: " + ex.getMessage();
		}
	}
	
	public void setUnhandled(String unHandledIOE) {
		this.unHandledIOE = unHandledIOE;
	}

	@Override
	protected String doInBackground(String... params) {
		String currentTime = String.valueOf(System.currentTimeMillis());
		String logOuput = params[0] + "/" + currentTime + ".txt";
		Process mLogcatProc = null;
		BufferedReader reader = null;
		final StringBuilder log = new StringBuilder();
		String separator = System.getProperty("line.separator");
		log.append(discoverCPUData());
		if (unHandledIOE != null) {
			log.append(separator);
			log.append(separator);
			log.append("Unhandled Exceptions");
			log.append(separator);
			log.append(separator);
			log.append(unHandledIOE);
		}
		try {
			mLogcatProc = Runtime.getRuntime().exec(
					new String[] { "logcat", "-d", "AndroidRuntime:E *:S" });
			reader = new BufferedReader(new InputStreamReader(
					mLogcatProc.getInputStream()));
			String line;
			log.append(separator);
			log.append(separator);
			log.append("AndroidRuntime Output");
			log.append(separator);
			log.append(separator);
			while ((line = reader.readLine()) != null) {
				log.append(line);
				log.append(separator);
			}
			reader.close();
			mLogcatProc = null;
			reader = null;
			int PID = android.os.Process.getUidForName("com.reicast.emulator");
			mLogcatProc = Runtime.getRuntime().exec(
					new String[] { "logcat", "-d", "|", "grep " + PID });
			reader = new BufferedReader(new InputStreamReader(
					mLogcatProc.getInputStream()));
			log.append(separator);
			log.append(separator);
			log.append("Application Output");
			log.append(separator);
			log.append(separator);
			while ((line = reader.readLine()) != null) {
				log.append(line);
				log.append(separator);
			}
			reader.close();
			mLogcatProc = null;
			reader = null;
			mLogcatProc = Runtime.getRuntime().exec(
					new String[] { "logcat", "-d", "reidc:V *:S" });
			reader = new BufferedReader(new InputStreamReader(
					mLogcatProc.getInputStream()));
			log.append(separator);
			log.append(separator);
			log.append("Native Interface Output");
			log.append(separator);
			log.append(separator);
			while ((line = reader.readLine()) != null) {
				log.append(line);
				log.append(separator);
			}
			reader.close();
			mLogcatProc = null;
			reader = null;
			mLogcatProc = Runtime.getRuntime().exec(
					new String[] { "logcat", "-d", "GL3JNIView:E *:S" });
			reader = new BufferedReader(new InputStreamReader(
					mLogcatProc.getInputStream()));
			log.append(separator);
			log.append(separator);
			log.append("Open GLES View Output");
			log.append(separator);
			log.append(separator);
			while ((line = reader.readLine()) != null) {
				log.append(line);
				log.append(separator);
			}
			reader.close();
			mLogcatProc = null;
			reader = null;
			mLogcatProc = Runtime.getRuntime().exec(
					new String[] { "logcat", "-d", "newdc:V *:S" });
			reader = new BufferedReader(new InputStreamReader(
					mLogcatProc.getInputStream()));
			log.append(separator);
			log.append(separator);
			log.append("Native Library Output");
			log.append(separator);
			log.append(separator);
			while ((line = reader.readLine()) != null) {
				log.append(line);
				log.append(separator);
			}
			reader.close();
			reader = null;
			File file = new File(logOuput);
			BufferedWriter writer = new BufferedWriter(new FileWriter(file));
			writer.write(log.toString());
			writer.flush();
			writer.close();
			return log.toString();
		} catch (IOException e) {

		}
		return null;
	}

	@Override
	protected void onPostExecute(String response) {
		if (response != null && !response.equals(null)) {
			if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB) {
				android.content.ClipboardManager clipboard = (android.content.ClipboardManager) mContext
						.getSystemService(Context.CLIPBOARD_SERVICE);
				android.content.ClipData clip = android.content.ClipData
						.newPlainText("logcat", response.toString());
				clipboard.setPrimaryClip(clip);
			} else {
				android.text.ClipboardManager clipboard = (android.text.ClipboardManager) mContext
						.getSystemService(Context.CLIPBOARD_SERVICE);
				clipboard.setText(response.toString());
			}
			Intent browserIntent = new Intent(
					Intent.ACTION_VIEW,
					Uri.parse("https://reicast.loungekatt.com/report"));
//			Intent browserIntent = new Intent(
//					Intent.ACTION_VIEW,
//					Uri.parse("https://github.com/LoungeKatt/reicast-emulator/issues/new"));
			mContext.startActivity(browserIntent);
		}
	}
}
