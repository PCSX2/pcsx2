package com.reicast.loungekatt;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;

import com.reicast.emulator.MainActivity;

public class Launcher extends Activity {

	/** Called when the activity is first created. */
	@SuppressLint("NewApi")
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		
		Intent mainIntent = new Intent(this, MainActivity.class);
		startActivity(mainIntent);
		finish();
	}
}