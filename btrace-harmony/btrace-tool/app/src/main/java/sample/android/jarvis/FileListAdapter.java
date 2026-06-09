/*
 * Copyright (c) 2026 Bytedance Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package sample.android.jarvis;

import android.content.Context;
import android.os.Process;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.TextView;

import com.bytedance.jarvis.core.util.ByteFormatter;

import java.io.File;

public class FileListAdapter extends BaseAdapter {
    private final Context context;
    private File[] files;

    public FileListAdapter(Context context) {
        this.context = context;
    }

    public void update() {
        File dir = new File(context.getExternalFilesDir(""), "cprf/" + Process.myPid());
        File[] files = dir.listFiles();
        if (files != null) {
            this.files = files;
        }
    }

    @Override
    public int getCount() {
        return files == null ? 0 : files.length;
    }

    @Override
    public File getItem(int position) {
        return files[position];
    }

    @Override
    public long getItemId(int position) {
        return position;
    }

    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
        if (convertView == null) {
            convertView = LayoutInflater.from(context).inflate(R.layout.item_file_adpter, parent, false);
        }
        File item = getItem(position);
        TextView nameView = convertView.findViewById(R.id.file_name);
        TextView sizeView = convertView.findViewById(R.id.file_size);
        nameView.setText(item.getName());
        sizeView.setText(ByteFormatter.format(item.length()));
        return convertView;
    }
}
