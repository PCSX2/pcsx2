package com.reicast.emulator;

import java.io.File;
import java.lang.Thread.UncaughtExceptionHandler;

import android.annotation.SuppressLint;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.res.Configuration;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.preference.PreferenceManager;
import android.support.v4.app.Fragment;
import android.util.Log;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnTouchListener;
import android.widget.TextView;
import android.widget.Toast;

import com.jeremyfeinstein.slidingmenu.lib.SlidingMenu;
import com.jeremyfeinstein.slidingmenu.lib.SlidingMenu.OnOpenListener;
import com.jeremyfeinstein.slidingmenu.lib.app.SlidingFragmentActivity;
import com.reicast.loungekatt.R;

public class MainActivity extends SlidingFragmentActivity implements
		FileBrowser.OnItemSelectedListener, OptionsFragment.OnClickListener {

	private SharedPreferences mPrefs;
	public static boolean force_gpu;
	private static File sdcard = Environment.getExternalStorageDirectory();
	public static String home_directory = sdcard + "/dc";
	
	public static long dreamRTC = ((20 * 365 + 5) * 86400);

	private TextView menuHeading;
	
	private SlidingMenu sm;
	
	private UncaughtExceptionHandler mUEHandler;

	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.mainuilayout_fragment);
		setBehindContentView(R.layout.drawer_menu);

		mPrefs = PreferenceManager.getDefaultSharedPreferences(this);
		home_directory = mPrefs.getString("home_directory", home_directory);
		JNIdc.config(home_directory);
		
		mUEHandler = new Thread.UncaughtExceptionHandler() {
	        public void uncaughtException(Thread t, Throwable error) {
	        	if (error != null) {
	        		Log.e("com.reicast.emulator", error.getMessage());
					Toast.makeText(MainActivity.this,
		    				getString(R.string.platform),
		    				Toast.LENGTH_SHORT).show();
		    		UploadLogs mUploadLogs = new UploadLogs(MainActivity.this);
		    		mUploadLogs.setUnhandled(error.getMessage());
		    		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB) {
		    			mUploadLogs.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR,
		    					home_directory);
		    		} else {
		    			mUploadLogs.execute(home_directory);
		    		}
	        	}
	        }
	    };
	    Thread.setDefaultUncaughtExceptionHandler(mUEHandler);

		// Check that the activity is using the layout version with
		// the fragment_container FrameLayout
		if (findViewById(R.id.fragment_container) != null) {

			// However, if we're being restored from a previous state,
			// then we don't need to do anything and should return or else
			// we could end up with overlapping fragments.
			if (Build.VERSION.SDK_INT < Build.VERSION_CODES.HONEYCOMB_MR1) {
				if (savedInstanceState != null) {
					return;
				}
			}

			// Create a new Fragment to be placed in the activity layout
			FileBrowser firstFragment = new FileBrowser();
			Bundle args = new Bundle();
			args.putBoolean("ImgBrowse", true);
			args.putString("browse_entry", null);
			// specify a path for selecting folder options
			args.putBoolean("games_entry", false);
			// specify if the desired path is for games or data
			firstFragment.setArguments(args);
			// In case this activity was started with special instructions from
			// an
			// Intent, pass the Intent's extras to the fragment as arguments
			// firstFragment.setArguments(getIntent().getExtras());

			// Add the fragment to the 'fragment_container' FrameLayout
			getSupportFragmentManager()
					.beginTransaction()
					.replace(R.id.fragment_container, firstFragment,
							"MAIN_BROWSER").commit();
		}
		
		force_gpu = mPrefs.getBoolean("force_gpu", false);
		boolean enable_zhack = mPrefs.getBoolean("enable_zhack", false);
		JNIdc.zHack(enable_zhack);
		
		menuHeading = (TextView) findViewById(R.id.menu_heading);
		
		sm = getSlidingMenu();
		sm.setShadowWidthRes(R.dimen.shadow_width);
		sm.setShadowDrawable(R.drawable.shadow);
		sm.setBehindOffsetRes(R.dimen.slidingmenu_offset);
		sm.setFadeDegree(0.35f);
		sm.setTouchModeAbove(SlidingMenu.TOUCHMODE_MARGIN);
		sm.setOnOpenListener(new OnOpenListener() {
			public void onOpen() {
				findViewById(R.id.browser_menu).setOnClickListener(new OnClickListener() {
					public void onClick(View view) {
						FileBrowser browseFrag = (FileBrowser) getSupportFragmentManager()
								.findFragmentByTag("MAIN_BROWSER");
						if (browseFrag != null) {
							if (browseFrag.isVisible()) {
								return;
							}
						}
						browseFrag = new FileBrowser();
						Bundle args = new Bundle();
						args.putBoolean("ImgBrowse", true);
						args.putString("browse_entry", null);
						// specify a path for selecting folder options
						args.putBoolean("games_entry", false);
						// specify if the desired path is for games or data
						browseFrag.setArguments(args);
						getSupportFragmentManager()
								.beginTransaction()
								.replace(R.id.fragment_container, browseFrag,
										"MAIN_BROWSER").addToBackStack(null)
								.commit();
						setTitle(getString(R.string.browser));
						sm.toggle(true);
					}

				});
				findViewById(R.id.settings_menu).setOnClickListener(new OnClickListener() {
					public void onClick(View view) {
						ConfigureFragment configFrag = (ConfigureFragment) getSupportFragmentManager()
								.findFragmentByTag("CONFIG_FRAG");
						if (configFrag != null) {
							if (configFrag.isVisible()) {
								return;
							}
						}
						configFrag = new ConfigureFragment();
						getSupportFragmentManager()
								.beginTransaction()
								.replace(R.id.fragment_container, configFrag,
										"CONFIG_FRAG").addToBackStack(null)
								.commit();
						setTitle(getString(R.string.settings));
						sm.toggle(true);
					}

				});

				findViewById(R.id.paths_menu).setOnClickListener(
						new OnClickListener() {
							public void onClick(View view) {
								OptionsFragment optionsFrag = (OptionsFragment) getSupportFragmentManager()
										.findFragmentByTag("OPTIONS_FRAG");
								if (optionsFrag != null) {
									if (optionsFrag.isVisible()) {
										return;
									}
								}
								optionsFrag = new OptionsFragment();
								getSupportFragmentManager()
										.beginTransaction()
										.replace(R.id.fragment_container,
												optionsFrag, "OPTIONS_FRAG")
										.addToBackStack(null).commit();
								setTitle(getString(R.string.paths));
								sm.toggle(true);
							}

						});

				findViewById(R.id.input_menu).setOnClickListener(new OnClickListener() {
					public void onClick(View view) {
						InputFragment inputFrag = (InputFragment) getSupportFragmentManager()
								.findFragmentByTag("INPUT_FRAG");
						if (inputFrag != null) {
							if (inputFrag.isVisible()) {
								return;
							}
						}
						inputFrag = new InputFragment();
						getSupportFragmentManager()
								.beginTransaction()
								.replace(R.id.fragment_container, inputFrag,
										"INPUT_FRAG").addToBackStack(null).commit();
						setTitle(getString(R.string.input));
						sm.toggle(true);
					}

				});
				
				findViewById(R.id.about_menu).setOnClickListener(new OnClickListener() {
					public void onClick(View view) {
						AboutFragment aboutFrag = (AboutFragment) getSupportFragmentManager()
								.findFragmentByTag("ABOUT_FRAG");
						if (aboutFrag != null) {
							if (aboutFrag.isVisible()) {
								return;
							}
						}
						aboutFrag = new AboutFragment();
						getSupportFragmentManager()
								.beginTransaction()
								.replace(R.id.fragment_container, aboutFrag,
										"ABOUT_FRAG").addToBackStack(null).commit();
						setTitle(getString(R.string.about));
						sm.toggle(true);
					}

				});

				findViewById(R.id.rateme_menu).setOnTouchListener(new OnTouchListener() {
					public boolean onTouch(View v, MotionEvent event) {
						if (event.getActionMasked() == MotionEvent.ACTION_DOWN) {
							// vib.vibrate(50);
							startActivity(new Intent(Intent.ACTION_VIEW, Uri
									.parse("market://details?id="
											+ getPackageName())));
							//setTitle(getString(R.string.rateme));
							sm.toggle(true);
							return true;
						} else
							return false;
					}
				});
			}
		});
		findViewById(R.id.header_list).setOnTouchListener(new OnTouchListener() {
			public boolean onTouch(View v, MotionEvent event) {
				if (event.getActionMasked() == MotionEvent.ACTION_DOWN) {
					sm.toggle(true);
					return true;
				} else
					return false;
			}
		});
		System.gc();

	}

	public void onGameSelected(Uri uri) {
		long dreamTime = (System.currentTimeMillis() / 1000) + dreamRTC;
		ConfigureFragment config = new ConfigureFragment();
		config.executeAppendConfig("Dreamcast.RTC",
				String.valueOf(String.valueOf(dreamTime)));
		Intent inte = new Intent(Intent.ACTION_VIEW, uri, getBaseContext(),
				GL2JNIActivity.class);
		startActivity(inte);
	}

	public static boolean isBiosExisting() {
		File bios = new File(home_directory, "data/dc_boot.bin");
		return bios.exists();
	}

	public static boolean isFlashExisting() {
		File flash = new File(home_directory, "data/dc_flash.bin");
		return flash.exists();
	}

	public void onFolderSelected(Uri uri) {
		FileBrowser browserFrag = (FileBrowser) getSupportFragmentManager()
				.findFragmentByTag("MAIN_BROWSER");
		if (browserFrag != null) {
			if (browserFrag.isVisible()) {

				Log.d("reicast", "Main folder: " + uri.toString());
				// return;
			}
		}

		OptionsFragment optsFrag = new OptionsFragment();
		getSupportFragmentManager().beginTransaction()
				.replace(R.id.fragment_container, optsFrag, "OPTIONS_FRAG")
				.commit();
		return;
	}

	public void onMainBrowseSelected(String path_entry, boolean games) {
		FileBrowser firstFragment = new FileBrowser();
		Bundle args = new Bundle();
		args.putBoolean("ImgBrowse", false);
		// specify ImgBrowse option. true = images, false = folders only
		args.putString("browse_entry", path_entry);
		// specify a path for selecting folder options
		args.putBoolean("games_entry", games);
		// specify if the desired path is for games or data

		firstFragment.setArguments(args);
		// In case this activity was started with special instructions from
		// an Intent, pass the Intent's extras to the fragment as arguments
		// firstFragment.setArguments(getIntent().getExtras());

		// Add the fragment to the 'fragment_container' FrameLayout
		getSupportFragmentManager()
				.beginTransaction()
				.replace(R.id.fragment_container, firstFragment, "MAIN_BROWSER")
				.addToBackStack(null).commit();
	}

	@SuppressLint("NewApi")
	@Override
	public void setTitle(CharSequence title) {
		menuHeading.setText(title);
	}

	/**
	 * When using the ActionBarDrawerToggle, you must call it during
	 * onPostCreate() and onConfigurationChanged()...
	 */

	@Override
	public void onPostCreate(Bundle savedInstanceState) {
		super.onPostCreate(savedInstanceState);
	}

	@Override
	public void onConfigurationChanged(Configuration newConfig) {
		super.onConfigurationChanged(newConfig);
	}

	@Override
	public boolean onKeyDown(int keyCode, KeyEvent event) {
		if (keyCode == KeyEvent.KEYCODE_BACK) {
			Fragment fragment = (FileBrowser) getSupportFragmentManager()
					.findFragmentByTag("MAIN_BROWSER");
			if (fragment != null && fragment.isVisible()) {
				MainActivity.this.finish();
				return true;
			} else {
				fragment = new FileBrowser();
				Bundle args = new Bundle();
				args.putBoolean("ImgBrowse", true);
				args.putString("browse_entry", null);
				args.putBoolean("games_entry", false);
				fragment.setArguments(args);
				getSupportFragmentManager()
				.beginTransaction()
				.replace(R.id.fragment_container, fragment,
						"MAIN_BROWSER").commit();
				setTitle(getString(R.string.browser));
				return true;
			}

		}

		return super.onKeyDown(keyCode, event);
	}

	@Override
	protected void onPause() {
		super.onPause();
		InputFragment fragment = (InputFragment) getSupportFragmentManager()
				.findFragmentByTag("INPUT_FRAG");
		if (fragment != null && fragment.isVisible()) {
			if (fragment.moga != null) {
				fragment.moga.onPause();
			}
		}
	}

	@Override
	protected void onDestroy() {
		super.onDestroy();
		InputFragment fragment = (InputFragment) getSupportFragmentManager()
				.findFragmentByTag("INPUT_FRAG");
		if (fragment != null && fragment.isVisible()) {
			if (fragment.moga != null) {
				fragment.moga.onDestroy();
			}
		}
	}

	@Override
	protected void onResume() {
		super.onResume();
		InputFragment fragment = (InputFragment) getSupportFragmentManager()
				.findFragmentByTag("INPUT_FRAG");
		if (fragment != null && fragment.isVisible()) {
			if (fragment.moga != null) {
				fragment.moga.onResume();
			}
		}
	}
}
