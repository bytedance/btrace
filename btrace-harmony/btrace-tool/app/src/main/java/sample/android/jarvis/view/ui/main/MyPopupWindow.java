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

package sample.android.jarvis.view.ui.main;

import android.view.View;
import android.widget.PopupWindow;

public class MyPopupWindow extends PopupWindow {
    public MyPopupWindow(View contentView, int width, int height, boolean focusable) {
        super(contentView, width, height, focusable);
    }

    @Override
    public void dismiss() {
        super.dismiss();
    }

    public static void test1(PopupWindow window) {
        window.dismiss();
    }

    public void test2(PopupWindow window) {
        window.dismiss();
    }

    public void test3() {
        PopupWindow popup = new PopupWindow();
        popup.dismiss();
    }

    private void test4() {
        test5();
    }

    private void test5() {
        test4();
    }
}
