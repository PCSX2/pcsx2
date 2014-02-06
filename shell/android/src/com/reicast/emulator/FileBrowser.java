package com.reicast.emulator;

import java.io.File;
import java.io.FileOutputStream;
import java.io.FilenameFilter;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.List;
import java.util.Locale;

import org.apache.commons.lang3.StringUtils;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.SharedPreferences;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.os.Vibrator;
import android.preference.PreferenceManager;
import android.support.v4.app.Fragment;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnTouchListener;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.Toast;

import com.android.util.FileUtils;
import com.reicast.loungekatt.R;

public class FileBrowser extends Fragment {

	Vibrator vib;
	Drawable orig_bg;
	Activity parentActivity;
	boolean ImgBrowse;
	private boolean games;
	OnItemSelectedListener mCallback;

	private SharedPreferences mPrefs;
	private File sdcard = Environment.getExternalStorageDirectory();
	private String home_directory = sdcard + "/dc";
	private String game_directory = sdcard + "/dc";

	private String localized = "R";

	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);

		mPrefs = PreferenceManager.getDefaultSharedPreferences(getActivity());
		home_directory = mPrefs.getString("home_directory", home_directory);
		game_directory = mPrefs.getString("game_directory", game_directory);
		localized = mPrefs.getString("localized", localized);

		Bundle b = getArguments();
		if (b != null) {
			ImgBrowse = b.getBoolean("ImgBrowse", true);
			if (games = b.getBoolean("games_entry", false)) {
				if (b.getString("path_entry") != null) {
					home_directory = b.getString("path_entry");
				}
			} else {
				if (b.getString("path_entry") != null) {
					game_directory = b.getString("path_entry");
				}
			}
		}

	}

	// Container Activity must implement this interface
	public interface OnItemSelectedListener {
		public void onGameSelected(Uri uri);

		public void onFolderSelected(Uri uri);
	}

	@Override
	public void onAttach(Activity activity) {
		super.onAttach(activity);

		// This makes sure that the container activity has implemented
		// the callback interface. If not, it throws an exception
		try {
			mCallback = (OnItemSelectedListener) activity;
		} catch (ClassCastException e) {
			throw new ClassCastException(activity.toString()
					+ " must implement OnItemSelectedListener");
		}
	}

	@Override
	public View onCreateView(LayoutInflater inflater, ViewGroup container,
			Bundle savedInstanceState) {
		return inflater.inflate(R.layout.activity_main, container, false);
	}

	@Override
	public void onViewCreated(View view, Bundle savedInstanceState) {
		// setContentView(R.layout.activity_main);
		parentActivity = getActivity();
		try {
			File file = new File(home_directory, "data/buttons.png");
			if (!file.exists()) {
				file.createNewFile();
				OutputStream fo = new FileOutputStream(file);
				InputStream png = parentActivity.getAssets()
						.open("buttons.png");

				byte[] buffer = new byte[4096];
				int len = 0;
				while ((len = png.read(buffer)) != -1) {
					fo.write(buffer, 0, len);
				}
				fo.close();
				png.close();
			}
		} catch (IOException ioe) {
			ioe.printStackTrace();
		}

		vib = (Vibrator) parentActivity
				.getSystemService(Context.VIBRATOR_SERVICE);

		/*
		 * OnTouchListener viblist=new OnTouchListener() {
		 * 
		 * public boolean onTouch(View v, MotionEvent event) { if
		 * (event.getActionMasked()==MotionEvent.ACTION_DOWN) vib.vibrate(50);
		 * return false; } };
		 * 
		 * findViewById(R.id.config).setOnTouchListener(viblist);
		 * findViewById(R.id.about).setOnTouchListener(viblist);
		 */

		File home = new File(home_directory);
		if (!home.exists() || !home.isDirectory()) {
			Toast.makeText(getActivity(), "Please configure a home directory",
					Toast.LENGTH_LONG).show();
		}

		if (!ImgBrowse) {
			navigate(sdcard);
		} else {
			LocateGames mLocateGames = new LocateGames();
			if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB) {
				mLocateGames
						.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR, game_directory);
			} else {
				mLocateGames.execute(game_directory);
			}
		}

		String msg = null;
		if (!MainActivity.isBiosExisting())
			msg = getString(R.string.missing_bios, home_directory);
		else if (!MainActivity.isFlashExisting())
			msg = getString(R.string.missing_flash, home_directory);

		if (msg != null && ImgBrowse) {
			vib.vibrate(50);
			AlertDialog.Builder alertDialogBuilder = new AlertDialog.Builder(
					parentActivity);

			// set title
			alertDialogBuilder.setTitle(getString(R.string.require_bios));

			// set dialog message
			alertDialogBuilder
					.setMessage(msg)
					.setCancelable(false)
					.setPositiveButton("Dismiss",
							new DialogInterface.OnClickListener() {
								public void onClick(DialogInterface dialog,
										int id) {
									// if this button is clicked, close
									// current activity
									parentActivity.finish();
								}
							})
					.setNegativeButton("Options",
							new DialogInterface.OnClickListener() {
								public void onClick(DialogInterface dialog,
										int id) {
									OptionsFragment optionsFrag = new OptionsFragment();
									getActivity()
											.getSupportFragmentManager()
											.beginTransaction()
											.replace(R.id.fragment_container,
													optionsFrag,
													"OPTIONS_FRAG")
											.addToBackStack(null).commit();
								}
							});

			// create alert dialog
			AlertDialog alertDialog = alertDialogBuilder.create();

			// show it
			alertDialog.show();
		}
	}

	class LocateGames extends AsyncTask<String, Integer, List<File>> {

		@Override
		protected List<File> doInBackground(String... paths) {
			final List<File> tFileList = new ArrayList<File>();
			File storage = new File(paths[0]);
			Resources resources = parentActivity.getResources();
			// array of valid image file extensions
			String[] mediaTypes = resources.getStringArray(R.array.images);
			FilenameFilter[] filter = new FilenameFilter[mediaTypes.length];

			int i = 0;
			for (final String type : mediaTypes) {
				filter[i] = new FilenameFilter() {

					public boolean accept(File dir, String name) {
						if (dir.getName().substring(0, 1).indexOf(".") != -1
								|| name.substring(0, 1).indexOf(".") != -1) {
							return false;
						} else {
							String file = name.toLowerCase(Locale.getDefault());
							return (file.substring(file.length() - 4,
									file.length()).indexOf("." + type) != -1);
						}
					}

				};
				i++;
			}

			FileUtils fileUtils = new FileUtils();
			File[] allMatchingFiles = fileUtils.listFilesAsArray(storage,
					filter, -1);
			for (File mediaFile : allMatchingFiles) {
				tFileList.add(mediaFile);
			}
			return tFileList;
		}

		@Override
		protected void onPostExecute(List<File> games) {
			final LinearLayout list = (LinearLayout) parentActivity
					.findViewById(R.id.game_list);
			list.removeAllViews();

			String heading = parentActivity
					.getString(R.string.games_listing);
			createListHeader(heading, list, true);
			if (games != null && !games.isEmpty()) {
				for (int i = 0; i < games.size(); i++) {
					createListItem(list, games.get(i));
				}
			} else {
				Toast.makeText(parentActivity, "Please configure a games directory",
							Toast.LENGTH_LONG).show();
			}
			list.invalidate();
		}

	}

	class DirSort implements Comparator<File> {

		// Comparator interface requires defining compare method.
		public int compare(File filea, File fileb) {

			return ((filea.isFile() ? "a" : "b") + filea.getName().toLowerCase(
					Locale.getDefault()))
					.compareTo((fileb.isFile() ? "a" : "b")
							+ fileb.getName().toLowerCase(Locale.getDefault()));
		}
	}

	private void createListHeader(String header_text, View view, boolean hasBios) {
		if (hasBios) {
			final View childview = parentActivity.getLayoutInflater().inflate(
					R.layout.bios_list_item, null, false);

			((TextView) childview.findViewById(R.id.item_name))
					.setText(parentActivity.getString(R.string.boot_bios));

			childview.setTag(null);

			orig_bg = childview.getBackground();

			childview.findViewById(R.id.childview).setOnClickListener(
					new OnClickListener() {
						public void onClick(View view) {
							File f = (File) view.getTag();
							vib.vibrate(50);
							mCallback.onGameSelected(f != null ? Uri
									.fromFile(f) : Uri.EMPTY);
							vib.vibrate(250);
						}
					});

			childview.findViewById(R.id.childview).setOnTouchListener(
					new OnTouchListener() {
						@SuppressWarnings("deprecation")
						public boolean onTouch(View view, MotionEvent arg1) {
							if (arg1.getActionMasked() == MotionEvent.ACTION_DOWN) {
								view.setBackgroundColor(0xFF4F3FFF);
							} else if (arg1.getActionMasked() == MotionEvent.ACTION_CANCEL
									|| arg1.getActionMasked() == MotionEvent.ACTION_UP) {
								view.setBackgroundDrawable(orig_bg);
							}

							return false;

						}
					});
			((ViewGroup) view).addView(childview);
		}

		final View headerView = parentActivity.getLayoutInflater().inflate(
				R.layout.app_list_item, null, false);
		((ImageView) headerView.findViewById(R.id.item_icon))
				.setImageResource(R.drawable.open_folder);
		((TextView) headerView.findViewById(R.id.item_name))
				.setText(header_text);
		((ViewGroup) view).addView(headerView);

	}

	private void createListItem(LinearLayout list, final File game) {
		final String name = game.getName();
		final View childview = parentActivity.getLayoutInflater().inflate(
				R.layout.app_list_item, null, false);

		((TextView) childview.findViewById(R.id.item_name)).setText(name);

		((ImageView) childview.findViewById(R.id.item_icon))
				.setImageResource(game == null ? R.drawable.config : game
						.isDirectory() ? R.drawable.open_folder
						: name.toLowerCase(Locale.getDefault())
								.endsWith(".gdi") ? R.drawable.gdi : name
								.toLowerCase(Locale.getDefault()).endsWith(
										".cdi") ? R.drawable.cdi : name
								.toLowerCase(Locale.getDefault()).endsWith(
										".chd") ? R.drawable.chd
								: R.drawable.disk_unknown);

		childview.setTag(name);

		orig_bg = childview.getBackground();

		// vw.findViewById(R.id.childview).setBackgroundColor(0xFFFFFFFF);

		childview.findViewById(R.id.childview).setOnClickListener(
				new OnClickListener() {
					public void onClick(View view) {
						vib.vibrate(50);
						mCallback.onGameSelected(game != null ? Uri
								.fromFile(game) : Uri.EMPTY);
						vib.vibrate(250);
					}
				});

		childview.findViewById(R.id.childview).setOnTouchListener(
				new OnTouchListener() {
					@SuppressWarnings("deprecation")
					public boolean onTouch(View view, MotionEvent arg1) {
						if (arg1.getActionMasked() == MotionEvent.ACTION_DOWN) {
							view.setBackgroundColor(0xFF4F3FFF);
						} else if (arg1.getActionMasked() == MotionEvent.ACTION_CANCEL
								|| arg1.getActionMasked() == MotionEvent.ACTION_UP) {
							view.setBackgroundDrawable(orig_bg);
						}

						return false;

					}
				});
		list.addView(childview);
	}

	void navigate(final File root_sd) {
		LinearLayout v = (LinearLayout) parentActivity
				.findViewById(R.id.game_list);
		v.removeAllViews();

		ArrayList<File> list = new ArrayList<File>();

		final String heading = root_sd.getAbsolutePath();
		createListHeader(heading, v, false);

		File flist[] = root_sd.listFiles();

		File parent = root_sd.getParentFile();

		list.add(null);

		if (parent != null)
			list.add(parent);

		Arrays.sort(flist, new DirSort());

		for (int i = 0; i < flist.length; i++)
			list.add(flist[i]);

		for (int i = 0; i < list.size(); i++) {
			if (list.get(i) != null && !list.get(i).isDirectory())
				continue;
			final View childview = parentActivity.getLayoutInflater().inflate(
					R.layout.app_list_item, null, false);

			if (list.get(i) == null) {
				((TextView) childview.findViewById(R.id.item_name))
						.setText(getString(R.string.folder_select));
			} else if (list.get(i) == parent)
				((TextView) childview.findViewById(R.id.item_name))
						.setText("..");
			else
				((TextView) childview.findViewById(R.id.item_name))
						.setText(list.get(i).getName());

			((ImageView) childview.findViewById(R.id.item_icon))
					.setImageResource(list.get(i) == null ? R.drawable.config
							: list.get(i).isDirectory() ? R.drawable.open_folder
									: R.drawable.disk_unknown);

			childview.setTag(list.get(i));
			final File item = list.get(i);

			orig_bg = childview.getBackground();

			// vw.findViewById(R.id.childview).setBackgroundColor(0xFFFFFFFF);

			childview.findViewById(R.id.childview).setOnClickListener(
					new OnClickListener() {
						public void onClick(View view) {
							if (item != null && item.isDirectory()) {
								navigate(item);
								ScrollView sv = (ScrollView) parentActivity
										.findViewById(R.id.game_scroller);
								sv.scrollTo(0, 0);
								vib.vibrate(50);
							} else if (view.getTag() == null) {
								vib.vibrate(50);

								mCallback.onFolderSelected(Uri
										.fromFile(new File(heading)));
								vib.vibrate(250);

								if (games) {
									game_directory = heading;
									mPrefs.edit()
											.putString("game_directory",
													heading).commit();
								} else {
									home_directory = heading;
									mPrefs.edit()
											.putString("home_directory",
													heading).commit();
									File data_directory = new File(heading,
											"data");
									if (!data_directory.exists()
											|| !data_directory.isDirectory()) {
										data_directory.mkdirs();
									}
									JNIdc.config(heading);
								}
							}
						}
					});

			childview.findViewById(R.id.childview).setOnTouchListener(
					new OnTouchListener() {
						@SuppressWarnings("deprecation")
						public boolean onTouch(View view, MotionEvent arg1) {
							if (arg1.getActionMasked() == MotionEvent.ACTION_DOWN) {
								view.setBackgroundColor(0xFF4F3FFF);
							} else if (arg1.getActionMasked() == MotionEvent.ACTION_CANCEL
									|| arg1.getActionMasked() == MotionEvent.ACTION_UP) {
								view.setBackgroundDrawable(orig_bg);
							}

							return false;

						}
					});
			v.addView(childview);
		}
		v.invalidate();
	}
}
