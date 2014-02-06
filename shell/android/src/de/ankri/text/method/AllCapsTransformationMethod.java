/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package de.ankri.text.method;

import java.util.Locale;

import android.content.Context;
import android.graphics.Rect;
import android.util.Log;
import android.view.View;

/**
 * Transforms source text into an ALL CAPS string, locale-aware.
 * 
 * @hide
 */
public class AllCapsTransformationMethod implements TransformationMethodCompat2
{
	private static final String TAG = "AllCapsTransformationMethod";

	private boolean mEnabled;
	private Locale mLocale;

	public AllCapsTransformationMethod(Context context)
	{
		mLocale = context.getResources().getConfiguration().locale;
	}

	public CharSequence getTransformation(CharSequence source, View view)
	{
		if (mEnabled)
		{
			return source != null ? source.toString().toUpperCase(mLocale) : null;
		}
		Log.w(TAG, "Caller did not enable length changes; not transforming text");
		return source;
	}

	public void onFocusChanged(View view, CharSequence sourceText, boolean focused, int direction, Rect previouslyFocusedRect)
	{
	}

	public void setLengthChangesAllowed(boolean allowLengthChanges)
	{
		mEnabled = allowLengthChanges;
	}

}
