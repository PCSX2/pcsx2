/* ====================================================================
 * Copyright (c) 2012-2013 Lounge Katt Entertainment.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by Lounge Katt" unless
 *    otherwise displayed by public repository entries.
 *
 * 4. The names "Lounge Katt", "TwistedUmbrella", and "LiveLog"  
 *    must not be used to endorse or promote products derived from this 
 *    software without prior written permission. For written permission,
 *    please contact admin@loungekatt.com.
 *
 * 5. Products derived from this software may not be called "LiveLog"
 *    nor may "LiveLog" appear in their names without prior written
 *    permission of Lounge Katt Entertainment.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by Lounge Katt" unless
 *    otherwise displayed by tagged repository entries.
 *
 * THIS SOFTWARE IS PROVIDED BY Lounge Katt ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * The license and distribution terms for any publicly available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution license
 * [including the GNU Public License.] Content not subject to these terms is
 * subject to to the terms and conditions of the Apache License, Version 2.0.
 */

package com.reicast.emulator;

import java.util.ArrayList;
import java.util.HashMap;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.graphics.PorterDuff;
import android.os.Build;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.webkit.CookieManager;
import android.webkit.CookieSyncManager;
import android.webkit.WebSettings;
import android.webkit.WebSettings.PluginState;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.webkit.WebViewDatabase;
import android.widget.BaseAdapter;
import android.widget.ImageView;
import android.widget.RelativeLayout;
import android.widget.TextView;

import com.nostra13.universalimageloader.core.DisplayImageOptions;
import com.nostra13.universalimageloader.core.ImageLoader;
import com.nostra13.universalimageloader.core.ImageLoaderConfiguration;
import com.nostra13.universalimageloader.core.assist.ImageScaleType;
import com.reicast.loungekatt.R;

public class GitAdapter extends BaseAdapter {

	private Activity activity;
	private ArrayList<HashMap<String, String>> data;
	private LayoutInflater inflater = null;
	private DisplayImageOptions options;

	public GitAdapter(Activity a, ArrayList<HashMap<String, String>> d) {
		this.activity = a;
		this.data = d;
		this.inflater = (LayoutInflater) activity
				.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
		ImageLoaderConfiguration configicon = new ImageLoaderConfiguration.Builder(
				activity).memoryCacheExtraOptions(96, 96).build();
		this.options = new DisplayImageOptions.Builder()
				.showStubImage(R.drawable.ic_github)
				.showImageForEmptyUri(R.drawable.ic_github)
				.imageScaleType(ImageScaleType.EXACTLY_STRETCHED).build();

		ImageLoader.getInstance().init(configicon);

	}

	public int getCount() {
		return this.data.size();
	}

	public Object getItem(int position) {
		return position;
	}

	public long getItemId(int position) {
		return position;
	}

	public View getView(final int position, View convertView, ViewGroup parent) {
		View vi = convertView;
		if (convertView == null)
			vi = this.inflater.inflate(R.layout.change_item, null);
		TextView dateText = (TextView) vi.findViewById(R.id.date);
		TextView committerText = (TextView) vi.findViewById(R.id.committer);
		TextView titleText = (TextView) vi.findViewById(R.id.title);
		ImageView avatarIcon = (ImageView) vi.findViewById(R.id.avatar);

		final HashMap<String, String> commit = this.data.get(position);
		final String date = commit.get("Date");
		final String committer = commit.get("Committer");
		final String title = commit.get("Title");
		final String message = commit.get("Message");
		final String sha = commit.get("Sha");
		final String url = commit.get("Url");
		final String author = commit.get("Author");
		final String avatar = commit.get("Avatar");
		final String current = commit.get("Build");

		RelativeLayout item = (RelativeLayout) vi.findViewById(R.id.change);
		if (current != null && !current.equals("") && current.equals(sha)) {
			item.getBackground().setColorFilter(0xFF00FF00,
					PorterDuff.Mode.MULTIPLY);
		} else {
			item.getBackground().setColorFilter(null);
		}

		dateText.setText(date);
		committerText.setText(committer);
		titleText.setText(title);
		ImageLoader.getInstance()
				.displayImage(avatar, avatarIcon, this.options);

		vi.setOnClickListener(new OnClickListener() {
			public void onClick(View v) {
				System.gc();
				String output = message + "\n\n" + " - " + author;
				displayCommit(sha, output, url, v.getContext());
			}
		});
		// Handle clicking individual item from list

		return vi;
	}

	public static void displayCommit(String title, String message, String url,
			Context context) {
		final AlertDialog.Builder builder = new AlertDialog.Builder(context);
		builder.setCancelable(true);
		builder.setTitle(title);
		builder.setMessage(message);
		LayoutInflater infalter = LayoutInflater.from(context);
		final View popWebView = infalter.inflate(R.layout.webview, null);
		WebView mWebView = (WebView) popWebView.findViewById(R.id.webframe);
		mWebView = configureWebview(url, context, mWebView);
		builder.setView(popWebView);
		builder.setPositiveButton("Close",
				new DialogInterface.OnClickListener() {
					public void onClick(DialogInterface dialog, int which) {
						dialog.dismiss();
						return;
					}
				});
		builder.create().show();
	}

	public static WebView configureWebview(String url, Context context,
			WebView mWebView) {
		mWebView.getSettings().setSupportZoom(true);
		mWebView.getSettings().setBuiltInZoomControls(true);
		if (Integer.parseInt(Build.VERSION.SDK) >= Build.VERSION_CODES.HONEYCOMB) {
			mWebView.getSettings().setDisplayZoomControls(false);
		}
		mWebView.setInitialScale(1);
		if (Integer.parseInt(Build.VERSION.SDK) >= Build.VERSION_CODES.ECLAIR) {
			mWebView.getSettings().setUseWideViewPort(true);
		}
		if (Integer.parseInt(Build.VERSION.SDK) >= Build.VERSION_CODES.ECLAIR_MR1) {
			mWebView.getSettings().setLoadWithOverviewMode(true);
		}
		mWebView.getSettings().setJavaScriptEnabled(true);
		mWebView.getSettings().setPluginState(PluginState.ON);
		mWebView.getSettings().setCacheMode(WebSettings.LOAD_NO_CACHE);
		mWebView.clearHistory();
		mWebView.clearFormData();
		mWebView.clearCache(true);
		WebViewDatabase mDatabase = WebViewDatabase.getInstance(context);
		mDatabase.clearUsernamePassword();
		CookieSyncManager.createInstance(context);
		CookieManager cookieManager = CookieManager.getInstance();
		cookieManager.removeAllCookie();
		CookieSyncManager.getInstance().stopSync();
		mWebView.getSettings().setSavePassword(false);
		mWebView.setWebViewClient(new WebViewClient() {
			@Override
			public boolean shouldOverrideUrlLoading(WebView view, String url) {
				view.loadUrl(url);
				return true;
			}
		});
		mWebView.loadUrl(url);
		return mWebView;
	}
}