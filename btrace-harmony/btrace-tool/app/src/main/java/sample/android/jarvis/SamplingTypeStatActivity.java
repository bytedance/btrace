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

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.ListView;
import android.widget.TextView;

import androidx.annotation.Nullable;

import com.bytedance.jarvis.test.SamplingTypeLog;

import java.util.List;

public class SamplingTypeStatActivity extends Activity {
    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        ListView listView = new ListView(this);
        listView.setAdapter(new SamplingTypeStatAdapter(this));
        listView.setDivider(null);
        setContentView(listView);
    }

    private static class SamplingTypeStatAdapter extends BaseAdapter {
        private final Context context;
        private final List<SamplingTypeLog.Stat> data;

        public SamplingTypeStatAdapter(Context context) {
            this.context = context;
            this.data = SamplingTypeLog.dump();
        }

        @Override
        public int getCount() {
            return data.size();
        }

        @Override
        public SamplingTypeLog.Stat getItem(int position) {
            return data.get(position);
        }

        @Override
        public long getItemId(int position) {
            return position;
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            SamplingTypeLog.Stat stat = getItem(position);
            if (convertView == null) {
                convertView = LayoutInflater.from(context).inflate(R.layout.item_stub_adpter, parent, false);
            }
            ((TextView) convertView.findViewById(R.id.type_name)).setText(stat.name);
            ((TextView) convertView.findViewById(R.id.capture_count)).setText(String.valueOf(stat.captureCount));
            ((TextView) convertView.findViewById(R.id.request_count)).setText(String.valueOf(stat.count));
            ((TextView) convertView.findViewById(R.id.capture_percent)).setText(percent(stat.captureCount, stat.count));
            if (stat.count > 0 || stat.captureCount > 0) {
                convertView.setBackgroundColor(0xff369650);
            } else {
                convertView.setBackgroundColor(0xffE55765);
            }
            return convertView;
        }

        @SuppressLint("DefaultLocale")
        private String percent(int captureCount, int count) {
            return String.format("%.2f", (float) captureCount / count * 100) + "%";
        }
    }
}
