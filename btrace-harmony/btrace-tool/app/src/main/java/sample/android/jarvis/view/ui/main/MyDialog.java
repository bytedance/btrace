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

import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;
import android.os.Bundle;
import android.widget.Button;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import sample.android.jarvis.R;

public class MyDialog extends Dialog {

    private final Context mContext;
    private final String mTitle;
    private final String mMessage;
    private final String mPositiveButtonText;
    private final String mNegativeButtonText;
    private final OnClickListener mPositiveButtonListener;
    private final OnClickListener mNegativeButtonListener;

    public MyDialog(Context context, String title, String message, String positiveButtonText, String negativeButtonText, OnClickListener positiveButtonListener, OnClickListener negativeButtonListener) {
        super(context, R.style.Theme_AppCompat_DayNight_Dialog_Alert);
        mContext = context;
        mTitle = title;
        mMessage = message;
        mPositiveButtonText = positiveButtonText;
        mNegativeButtonText = negativeButtonText;
        mPositiveButtonListener = positiveButtonListener;
        mNegativeButtonListener = negativeButtonListener;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.my_custom_dialog);

        // 设置对话框标题
        TextView titleTextView = findViewById(R.id.dialog_title);
        titleTextView.setText(mTitle);

        // 设置对话框消息
        TextView messageTextView = findViewById(R.id.dialog_message);
        messageTextView.setText(mMessage);

        // 设置确认按钮
        Button positiveButton = findViewById(R.id.positive_button);
        positiveButton.setText(mPositiveButtonText);
        positiveButton.setOnClickListener(v -> {
            if (mPositiveButtonListener != null) {
                mPositiveButtonListener.onClick(this, DialogInterface.BUTTON_POSITIVE);
            }
            dismiss();
        });

        // 设置取消按钮
        Button negativeButton = findViewById(R.id.negative_button);
        negativeButton.setText(mNegativeButtonText);
        negativeButton.setOnClickListener(v -> {
            if (mNegativeButtonListener != null) {
                mNegativeButtonListener.onClick(this, DialogInterface.BUTTON_NEGATIVE);
            }
            dismiss();
        });
    }
}