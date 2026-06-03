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

import androidx.appcompat.app.AlertDialog;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;
import androidx.lifecycle.ViewModelProvider;

import android.Manifest;
import android.app.Activity;
import android.content.Context;
import android.content.DialogInterface;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.os.Bundle;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;

import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.LinearLayout;
import android.widget.PopupWindow;
import android.widget.SeekBar;
import android.widget.Toast;

import com.bytedance.jarvis.core.scene.monitor.FeedbackMonitor;

import sample.android.jarvis.R;

public class TestFragment extends Fragment implements View.OnClickListener {

    private MainViewModel mViewModel;

    public static TestFragment newInstance() {
        return new TestFragment();
    }

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mViewModel = new ViewModelProvider(this).get(MainViewModel.class);
        // TODO: Use the ViewModel
    }

    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container,
                             @Nullable Bundle savedInstanceState) {
        return inflater.inflate(R.layout.fragment_main, container, false);
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        view.findViewById(R.id.back).setOnClickListener(this);
        view.findViewById(R.id.show_dialog).setOnClickListener(this);
        view.findViewById(R.id.show_dialog_fragment).setOnClickListener(this);
        view.findViewById(R.id.show_toast).setOnClickListener(this);
        view.findViewById(R.id.show_popup_window).setOnClickListener(this);
        view.findViewById(R.id.show_system_permission).setOnClickListener(this);
        view.findViewById(R.id.checkbox).setOnClickListener(this);
        view.findViewById(R.id.seekbar).setOnClickListener(this);
        view.findViewById(R.id.feedback).setOnClickListener(this);
        ((CheckBox) view.findViewById(R.id.checkbox)).setOnCheckedChangeListener((buttonView, isChecked) -> {
            Log.d("shenyunlong", ">>> onCheckedChanged " + buttonView + " -> " + isChecked);
        });
        ((SeekBar) view.findViewById(R.id.seekbar)).setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                Log.d("shenyunlong", ">>> onProgressChanged " + seekBar + " -> " + progress + "," + fromUser);
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {
                Log.d("shenyunlong", ">>> onStartTrackingTouch " + seekBar);
            }

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
                Log.d("shenyunlong", ">>> onStopTrackingTouch " + seekBar);
            }
        });
    }

    @Override
    public void onResume() {
        super.onResume();
    }

    @Override
    public void onClick(View v) {
        if (v.getId() == R.id.back) {
            Activity activity = getActivity();
            if (activity != null) {
                activity.finish();
            }
        } else if (v.getId() == R.id.show_dialog) {
            //showDialog(getActivity());
            showCustomDialog(getActivity());
        } else if (v.getId() == R.id.show_dialog_fragment) {
            showDialogFragment();
        } else if (v.getId() == R.id.show_toast) {
            Toast.makeText(getActivity(), "Hello World", Toast.LENGTH_SHORT).show();
        } else if (v.getId() == R.id.show_popup_window) {
            showPopupWindow(getActivity(), v);
        } else if (v.getId() == R.id.show_system_permission) {
            requestStoragePermission(getActivity());
        } else if (v.getId() == R.id.checkbox) {
            Toast.makeText(getActivity(), "CheckBox clicked", Toast.LENGTH_SHORT).show();
        } else if (v.getId() == R.id.seekbar) {
            Toast.makeText(getActivity(), "Seekbar clicked", Toast.LENGTH_SHORT).show();
        } else if (v.getId() == R.id.feedback) {
            FeedbackMonitor.INSTANCE.stop();
        }
    }

    private void showDialog(Context context) {
        AlertDialog.Builder dialogBuilder = new AlertDialog.Builder(context);
        // 设置对话框标题
        dialogBuilder.setTitle("Dialog Title");
        // 设置对话框消息
        dialogBuilder.setMessage("This is a dialog message.");
        // 设置确认按钮
        dialogBuilder.setPositiveButton("OK", new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int which) {
                // 处理确认按钮点击事件
                Toast.makeText(context, "OK clicked", Toast.LENGTH_SHORT).show();
            }
        });
        // 设置取消按钮
        dialogBuilder.setNegativeButton("Cancel", new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int which) {
                // 处理取消按钮点击事件
                dialog.dismiss();
            }
        });
        // 创建对话框并显示
        AlertDialog dialog = dialogBuilder.create();
        dialog.show();
    }

    private void showCustomDialog(Context context) {
        MyDialog dialog = new MyDialog(
                context,
                "Custom Dialog",
                "This is a custom dialog.",
                "Confirm",
                "Cancel",
                (d, which) -> {
                    // 处理确认按钮点击事件
                },
                (d, which) -> {
                    // 处理取消按钮点击事件
                }
        );
        dialog.show();
    }

    private void showDialogFragment() {
        MyDialogFragment dialogFragment = new MyDialogFragment();
        dialogFragment.show(getActivity().getSupportFragmentManager(), "dialog_tag");
    }

    private void showPopupWindow(Context context, View view) {
        // 创建一个 PopupWindow 实例
        LayoutInflater inflater = (LayoutInflater) context.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        View popupView = inflater.inflate(R.layout.popup_window, null);

        int width = LinearLayout.LayoutParams.MATCH_PARENT;
        int height = LinearLayout.LayoutParams.WRAP_CONTENT;
        boolean focusable = true;

        final MyPopupWindow popupWindow = new MyPopupWindow(popupView, width, height, focusable);
        // 设置 PopupWindow 的dismissListener
        popupWindow.setOutsideTouchable(true);
        popupWindow.setFocusable(true);
        popupWindow.setBackgroundDrawable(new ColorDrawable(Color.BLUE));
        // 显示 PopupWindow
        popupWindow.showAsDropDown(view, 0, 0);
        // 获取 PopupWindow 中的按钮并设置点击事件
        Button dismissButton = popupView.findViewById(R.id.dismiss_button);
        dismissButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                popupWindow.dismiss();
            }
        });
    }

    public static final int REQUEST_CODE_STORAGE_PERMISSION = 1;

    private void requestStoragePermission(Activity context) {
        // 检查是否已经获得读写存储权限
        if (ContextCompat.checkSelfPermission(context, Manifest.permission.WRITE_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED) {
            // 如果没有读写存储权限，则请求该权限
            ActivityCompat.requestPermissions(context, new String[]{Manifest.permission.WRITE_EXTERNAL_STORAGE}, REQUEST_CODE_STORAGE_PERMISSION);
        } else {
            // 已经获得读写存储权限，可以执行相关操作
            Toast.makeText(context, "Permission ok!", Toast.LENGTH_SHORT).show();
        }
    }

}