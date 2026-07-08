/*
 * Copyright 2023 The Android Open Source Project
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

package com.mobileer.oboetester;

import android.content.Context;
import android.util.Log;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStreamWriter;
import java.io.Writer;

public class ExternalFileWriter {
    private static final String TAG = "OboeTester";
    private Context mContext;

    public ExternalFileWriter(Context context) {
        mContext = context;
    }

    public File writeStringToExternalFile(String result, String fileName) throws IOException {
        File dir = mContext.getExternalFilesDir(null);
        File resultFile = new File(dir, fileName);
        Log.d(TAG, "EXTFILE = " + resultFile.getAbsolutePath());
        Writer writer = null;
        try {
            writer = new OutputStreamWriter(new FileOutputStream(resultFile));
            writer.write(result);
        } finally {
            if (writer != null) {
                try {
                    writer.close();
                } catch (IOException e) {
                    e.printStackTrace();
                }
            }
        }
        return resultFile;
    }
}
