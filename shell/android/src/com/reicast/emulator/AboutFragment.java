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

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.HashMap;

import org.apache.http.HttpEntity;
import org.apache.http.HttpResponse;
import org.apache.http.StatusLine;
import org.apache.http.client.ClientProtocolException;
import org.apache.http.client.HttpClient;
import org.apache.http.client.methods.HttpGet;
import org.apache.http.impl.client.DefaultHttpClient;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import android.app.Activity;
import android.content.pm.PackageManager.NameNotFoundException;
import android.os.AsyncTask;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.support.v4.app.Fragment;
import android.text.util.Linkify;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.ListView;
import android.widget.SlidingDrawer;
import android.widget.SlidingDrawer.OnDrawerOpenListener;
import android.widget.TextView;
import android.widget.Toast;

import com.reicast.loungekatt.R;

public class AboutFragment extends Fragment {

	SlidingDrawer slidingGithub;
	private ListView list;
	private GitAdapter adapter;
	private Handler handler;

	private Activity parentActivity;

	@Override
	public View onCreateView(LayoutInflater inflater, ViewGroup container,
			Bundle savedInstanceState) {
		// Inflate the layout for this fragment
		return inflater.inflate(R.layout.about_fragment, container, false);
	}

	@Override
	public void onViewCreated(View view, Bundle savedInstanceState) {
		parentActivity = getActivity();
		handler = new Handler();
		try {
			String versionName = parentActivity.getPackageManager()
					.getPackageInfo(parentActivity.getPackageName(), 0).versionName;
			int versionCode = parentActivity.getPackageManager()
					.getPackageInfo(parentActivity.getPackageName(), 0).versionCode;
			TextView version = (TextView) getView().findViewById(
					R.id.about_text);
			version.setText(parentActivity.getString(R.string.about_text,
					versionName + "(" + String.valueOf(versionCode) + ")"));
		} catch (NameNotFoundException e) {
			e.printStackTrace();
		}
		
		TextView website = (TextView) getView().findViewById(
				R.id.site_text);
		Linkify.addLinks(website, Linkify.ALL);
		
		TextView compiled = (TextView) getView().findViewById(
				R.id.beta_text);
		Linkify.addLinks(compiled, Linkify.ALL);

		slidingGithub = (SlidingDrawer) getView().findViewById(
				R.id.slidingGithub);
		slidingGithub.setOnDrawerOpenListener(new OnDrawerOpenListener() {
			public void onDrawerOpened() {
				String git = parentActivity.getString(R.string.git_api);
				retrieveGitTask queryGithub = new retrieveGitTask();
				if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB) {
					queryGithub.executeOnExecutor(
							AsyncTask.THREAD_POOL_EXECUTOR, git);
				} else {
					queryGithub.execute(git);
				}
			}
		});
		slidingGithub.open();
	}

	public class retrieveGitTask extends
			AsyncTask<String, Integer, ArrayList<HashMap<String, String>>> {

		private String buildId = "";

		@Override
		protected void onPreExecute() {
			try {
				InputStream file = parentActivity.getAssets().open("build");
				if (file != null) {
					BufferedReader reader = new BufferedReader(
							new InputStreamReader(file));
					buildId = reader.readLine();
					file.close();
				}
			} catch (IOException ioe) {
				ioe.printStackTrace();
			}
		}

		@Override
		protected ArrayList<HashMap<String, String>> doInBackground(
				String... paths) {
			ArrayList<HashMap<String, String>> commitList = new ArrayList<HashMap<String, String>>();
			try {
				JSONArray gitObject = getContent(paths[0]);
				for (int i = 0; i < gitObject.length(); i++) {
					JSONObject jsonObject = gitObject.getJSONObject(i);

					JSONObject commitArray = jsonObject.getJSONObject("commit");

					String date = commitArray.getJSONObject("committer")
							.getString("date").replace("T", " ")
							.replace("Z", "");
					String author = commitArray.getJSONObject("author")
							.getString("name");
					String committer = commitArray.getJSONObject("committer")
							.getString("name");

					String avatar = null;
					if (!jsonObject.getString("committer").equals("null")) {
						avatar = jsonObject.getJSONObject("committer")
								.getString("avatar_url");
						committer = committer
								+ " ("
								+ jsonObject.getJSONObject("committer")
										.getString("login") + ")";
						if (avatar.equals("null")) {
							avatar = "https://github.com/apple-touch-icon-144.png";
						}
					} else {
						avatar = "https://github.com/apple-touch-icon-144.png";
					}
					if (!jsonObject.getString("author").equals("null")) {
						author = author
								+ " ("
								+ jsonObject.getJSONObject("author").getString(
										"login") + ")";
					}
					String sha = jsonObject.getString("sha");
					String curl = jsonObject
							.getString("url")
							.replace("https://api.github.com/repos",
									"https://github.com")
							.replace("commits", "commit");

					String title = "No commit heading attached";
					String message = "No commit message attached";

					if (commitArray.getString("message").contains("\n\n")) {
						String fullOutput = commitArray.getString("message");
						title = fullOutput.substring(0,
								fullOutput.indexOf("\n\n"));
						message = fullOutput.substring(
								fullOutput.indexOf("\n\n") + 1,
								fullOutput.length());
					} else {
						title = commitArray.getString("message");
					}

					HashMap<String, String> map = new HashMap<String, String>();
					map.put("Date", date);
					map.put("Committer", committer);
					map.put("Title", title);
					map.put("Message", message);
					map.put("Sha", sha);
					map.put("Url", curl);
					map.put("Author", author);
					map.put("Avatar", avatar);
					map.put("Build", buildId);
					commitList.add(map);
				}

			} catch (JSONException e) {
				handler.post(new Runnable() {
					public void run() {
						Toast.makeText(parentActivity,
								"GitHub Native Unavailable!", Toast.LENGTH_LONG)
								.show();
						slidingGithub.close();
					}
				});
				e.printStackTrace();
			} catch (Exception e) {
				handler.post(new Runnable() {
					public void run() {
						Toast.makeText(parentActivity,
								"GitHub Native Unavailable!", Toast.LENGTH_LONG)
								.show();
						slidingGithub.close();
					}
				});
				e.printStackTrace();
			}

			return commitList;
		}

		@Override
		protected void onPostExecute(
				ArrayList<HashMap<String, String>> commitList) {
			if (commitList != null && commitList.size() > 0) {
				list = (ListView) getView().findViewById(R.id.list);
				list.setSelector(R.drawable.list_selector);
				list.setChoiceMode(ListView.CHOICE_MODE_SINGLE);
				adapter = new GitAdapter(parentActivity, commitList);
				// Set adapter as specified collection
				list.setAdapter(adapter);

				list.setOnItemClickListener(new OnItemClickListener() {
					public void onItemClick(AdapterView<?> parent, View view,
							int position, long id) {
					}
				});
			}

		}
	}

	private JSONArray getContent(String urlString) throws IOException,
			JSONException {
		StringBuilder builder = new StringBuilder();
		HttpClient client = new DefaultHttpClient();
		HttpGet httpGet = new HttpGet(urlString);
		try {
			HttpResponse response = client.execute(httpGet);
			StatusLine statusLine = response.getStatusLine();
			int statusCode = statusLine.getStatusCode();
			if (statusCode == 200) {
				HttpEntity entity = response.getEntity();
				InputStream content = entity.getContent();
				BufferedReader reader = new BufferedReader(
						new InputStreamReader(content));
				String line;
				while ((line = reader.readLine()) != null) {
					builder.append(line);
				}
			} else {
			}
		} catch (ClientProtocolException e) {
			e.printStackTrace();
		} catch (IOException e) {
			e.printStackTrace();
		}
		return new JSONArray(builder.toString());
	}
}
