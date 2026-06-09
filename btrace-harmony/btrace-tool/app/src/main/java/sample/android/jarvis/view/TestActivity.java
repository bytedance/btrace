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

package sample.android.jarvis.view;

import static com.bytedance.jarvis.experience.metric.alloc.JavaAllocDetailMetrics.kMetricsTypeMetricsAll;
import static sample.android.jarvis.view.ui.main.TestFragment.REQUEST_CODE_STORAGE_PERMISSION;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;

import android.content.pm.PackageManager;
import android.os.Bundle;
import android.util.Log;
import android.view.KeyEvent;
import android.widget.Toast;

import com.bytedance.jarvis.experience.metric.alloc.JavaAllocDetailMetrics;

import sample.android.jarvis.R;
import sample.android.jarvis.view.ui.main.TestFragment;

public class TestActivity extends AppCompatActivity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_test);
        if (savedInstanceState == null) {
            getSupportFragmentManager().beginTransaction()
                    .replace(R.id.container, TestFragment.newInstance())
                    .commitNow();
        }
        JavaAllocDetailMetrics.getInstance().start(0x7);
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == REQUEST_CODE_STORAGE_PERMISSION) {
            if (grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                // 权限被授予，可以执行相关操作
                Toast.makeText(this, "Permission granted!", Toast.LENGTH_SHORT).show();
            } else {
                // 权限被拒绝，可以提示用户或执行其他操作
                Toast.makeText(this, "Permission denied!", Toast.LENGTH_SHORT).show();
            }
        }
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        switch (keyCode) {
            case KeyEvent.KEYCODE_HOME:
                Log.d("shenyunlong", ">>> onKeyDown HOME");
                break;
            case KeyEvent.KEYCODE_BACK:
                Log.d("shenyunlong", ">>> onKeyDown BACK");
                break;
            case KeyEvent.KEYCODE_VOLUME_UP:
                Log.d("shenyunlong", ">>> onKeyDown VOLUME_UP");
                break;
            case KeyEvent.KEYCODE_VOLUME_DOWN:
                Log.d("shenyunlong", ">>> onKeyDown VOLUME_DOWN");
                break;
            case KeyEvent.KEYCODE_POWER:
                Log.d("shenyunlong", ">>> onKeyDown POWER");
                break;
        }
        return super.onKeyDown(keyCode, event);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        Log.d("JavaAllocDetailMetrics", "stop " + JavaAllocDetailMetrics.getInstance().getCurrentData(kMetricsTypeMetricsAll, 1000));
        JavaAllocDetailMetrics.getInstance().stop();
    }
}