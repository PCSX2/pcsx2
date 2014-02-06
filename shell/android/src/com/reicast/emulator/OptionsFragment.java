package com.reicast.emulator;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

import android.app.Activity;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.os.Environment;
import android.preference.PreferenceManager;
import android.support.v4.app.Fragment;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.Spinner;

import com.reicast.loungekatt.R;

public class OptionsFragment extends Fragment {

	Activity parentActivity;
	Button mainBrowse;
	Button gameBrowse;
	Button mainLocale;
	OnClickListener mCallback;

	private SharedPreferences mPrefs;
	private File sdcard = Environment.getExternalStorageDirectory();
	private String home_directory = sdcard + "/dc";
	private String game_directory = sdcard + "/dc";
	
	private String localized = "R";

	// Container Activity must implement this interface
	public interface OnClickListener {
		public void onMainBrowseSelected(String path_entry, boolean games);
	}

	@Override
	public void onAttach(Activity activity) {
		super.onAttach(activity);

		// This makes sure that the container activity has implemented
		// the callback interface. If not, it throws an exception
		try {
			mCallback = (OnClickListener) activity;
		} catch (ClassCastException e) {
			throw new ClassCastException(activity.toString()
					+ " must implement OnClickListener");
		}
	}

	@Override
	public View onCreateView(LayoutInflater inflater, ViewGroup container,
			Bundle savedInstanceState) {
		// Inflate the layout for this fragment
		return inflater.inflate(R.layout.options_fragment, container, false);
	}

	@Override
	public void onViewCreated(View view, Bundle savedInstanceState) {
		// setContentView(R.layout.activity_main);

		parentActivity = getActivity();

		mPrefs = PreferenceManager.getDefaultSharedPreferences(parentActivity);
		home_directory = mPrefs.getString("home_directory", home_directory);

		mainBrowse = (Button) getView().findViewById(R.id.browse_main_path);

		final EditText editBrowse = (EditText) getView().findViewById(
				R.id.main_path);
		editBrowse.setText(home_directory);

		mainBrowse.setOnClickListener(new View.OnClickListener() {
			public void onClick(View view) {
				if (editBrowse.getText() != null) {
					home_directory = editBrowse.getText().toString();
					//mPrefs.edit().putString("home_directory", home_directory).commit();
				}
				mCallback.onMainBrowseSelected(home_directory, false);
			}
		});

		editBrowse.addTextChangedListener(new TextWatcher() {
			public void afterTextChanged(Editable s) {
				if (editBrowse.getText() != null) {
					home_directory = editBrowse.getText().toString();
					mPrefs.edit().putString("home_directory", home_directory)
							.commit();
				}
			}

			public void beforeTextChanged(CharSequence s, int start, int count,
					int after) {
			}

			public void onTextChanged(CharSequence s, int start, int before,
					int count) {
			}
		});

		gameBrowse = (Button) getView().findViewById(R.id.browse_game_path);

		final EditText editGames = (EditText) getView().findViewById(
				R.id.game_path);
		game_directory = mPrefs.getString("game_directory", game_directory);
		editGames.setText(game_directory);

		gameBrowse.setOnClickListener(new View.OnClickListener() {
			public void onClick(View view) {
				if (editBrowse.getText() != null) {
					game_directory = editGames.getText().toString();
					//mPrefs.edit().putString("game_directory", game_directory).commit();
				}
				mCallback.onMainBrowseSelected(game_directory, true);
			}
		});

		editGames.addTextChangedListener(new TextWatcher() {
			public void afterTextChanged(Editable s) {
				if (editBrowse.getText() != null) {
					game_directory = editGames.getText().toString();
					mPrefs.edit().putString("game_directory", game_directory)
					.commit();
				}
			}

			public void beforeTextChanged(CharSequence s, int start, int count,
					int after) {
			}

			public void onTextChanged(CharSequence s, int start, int before,
					int count) {
			}
		});

		String[] locales = parentActivity.getResources().getStringArray(R.array.region);
		localized = mPrefs.getString("localized", localized);

		Spinner locale_spnr = (Spinner) getView().findViewById(
				R.id.locale_spinner);
		ArrayAdapter<String> localeAdapter = new ArrayAdapter<String>(
				parentActivity, android.R.layout.simple_spinner_item, locales);
		localeAdapter
				.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
		locale_spnr.setAdapter(localeAdapter);
		locale_spnr.setSelection(3, true);
		locale_spnr.setOnItemSelectedListener(new OnItemSelectedListener() {

			public void onItemSelected(AdapterView<?> parent, View view,
					int pos, long id) {
				String selection = parent.getItemAtPosition(pos).toString();
				String bios = selection.substring(0, selection.indexOf(" - "));
				if (!bios.equals(localized))
					flashBios(bios);
			}

			public void onNothingSelected(AdapterView<?> arg0) {
				
			}

		});
	}
	
	private void flashBios(String localized) {
		File bios = new File(home_directory, "data/dc_boot.bin");
		File local = new File(home_directory, "data/dc_flash[" + localized
				+ "].bin");
		File flash = new File(home_directory, "data/dc_flash.bin");

		try {
			if (!MainActivity.isBiosExisting()) {
				bios.createNewFile();
				OutputStream fo = new FileOutputStream(bios);
				InputStream file = parentActivity.getAssets()
						.open("dc_boot.bin");

				byte[] buffer = new byte[4096];
				int len = 0;
				while ((len = file.read(buffer)) != -1) {
					fo.write(buffer, 0, len);
				}
				fo.close();
				file.close();
			}
		} catch (IOException ioe) {
			ioe.printStackTrace();
		}

		try {
			flash.createNewFile();
			OutputStream fo = new FileOutputStream(flash);
			InputStream file = parentActivity.getAssets()
					.open("dc_flash[" + localized + "].bin");

			byte[] buffer = new byte[4096];
			int len = 0;
			while ((len = file.read(buffer)) != -1) {
				fo.write(buffer, 0, len);
			}
			fo.close();
			file.close();
		} catch (IOException ioe) {
			ioe.printStackTrace();
		}

		if (local.exists()) {
			if (flash.exists()) {
				flash.delete();
			}
			local.renameTo(flash);
		}
		this.localized = localized;
		mPrefs.edit().putString("localized", localized).commit();
	}
}
