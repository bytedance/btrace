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

import android.util.Log;
import android.widget.TextView;

import com.bytedance.jarvis.common.IoUtils;

import java.io.BufferedReader;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.locks.LockSupport;

public class NetworkTest {

    public static void loadHtml(TextView textView) {
        new Thread(() -> {
            LockSupport.parkNanos(TimeUnit.SECONDS.toNanos(1));
            HttpURLConnection connection = null;
            BufferedReader reader = null;

            try {
                // 创建URL对象，这里使用一个测试API
                URL url = new URL("https://www.doubao.com/chat/");
                // 打开连接
                connection = (HttpURLConnection) url.openConnection();
                // 设置请求方法
                connection.setRequestMethod("GET");
                // 设置连接超时和读取超时时间
                connection.setConnectTimeout(8000);
                connection.setReadTimeout(8000);

                // 获取输入流
                InputStream in = connection.getInputStream();
                // 读取输入流
                reader = new BufferedReader(new InputStreamReader(in));
                StringBuilder response = new StringBuilder();
                String line;
                while ((line = reader.readLine()) != null) {
                    response.append(line);
                }

                textView.post(() -> textView.setText(response.toString()));

            } catch (Exception e) {
                textView.post(() -> textView.setText(Log.getStackTraceString(e)));
            } finally {
                IoUtils.close(reader);
                if (connection != null) {
                    connection.disconnect();
                }
            }
        }, "network").start();
    }
}
