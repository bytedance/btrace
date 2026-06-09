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

package sample.android.jarvis.test;

import androidx.annotation.Keep;

/**
 * @author majun
 * @date 2022/6/24
 */
@Keep
public class A1 implements A {
    @Override
    public void a() {
        int a = 1;
        float b = 2;
        if (a + b > 0) {
            A2 a2 = new A2(">");
            a2.a();
        }
    }
}
