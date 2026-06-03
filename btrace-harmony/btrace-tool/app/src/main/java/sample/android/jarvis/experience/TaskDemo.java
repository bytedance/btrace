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

package sample.android.jarvis.experience;

import android.util.Log;

import androidx.annotation.NonNull;

import com.bytedance.jarvis.experience.metric.task.ExecutionContext;
import com.bytedance.jarvis.experience.metric.task.ITask;
import com.bytedance.jarvis.experience.metric.task.ITriggerCondition;
import com.bytedance.jarvis.experience.metric.task.TaskManager;
import com.bytedance.jarvis.experience.metric.task.config.PeriodicConfig;

import java.util.concurrent.TimeUnit;

public class TaskDemo {
    private static final String HEALTH_CHECK_TASK = "health-check";
    private static final String DATA_SYNC_TASK = "data-sync";
    private static final String REPORT_TASK = "report-gen";

    public static void run() {
        // 初始化任务管理器
        TaskManager taskManager = TaskManager.getInstance();
        taskManager.initialize(4); // 使用4个核心线程

        // 示例1：纯周期性任务 - 每5秒执行
        taskManager.registerPeriodicTask(new HealthCheckTask(), new PeriodicConfig(5000, 1000, TimeUnit.MILLISECONDS));

        // 示例2：条件触发任务 - 每10秒检查一次条件
        taskManager.registerConditionalTask(new DataSyncTask(), new NetworkAvailableCondition(), 10000);

        // 示例3：纯手动触发任务
        taskManager.registerManualTask(new ReportGenerationTask());

        // 在业务逻辑中手动触发报表生成
        new Thread(() -> {
            try {
                // 等待系统初始化...
                Thread.sleep(5000);

                // 触发报表生成
                taskManager.triggerTask(REPORT_TASK, "demo", null);

                // 更新网络检查频率
                //taskManager.updateTaskConfig(NETWORK_CHECK_TASK,
                //        new PeriodicConfig(3000) // 调整为每3秒检查
                //);

            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        }).start();

        // 添加关闭钩子
        //Runtime.getRuntime().addShutdownHook(new Thread(scheduler::shutdown));
    }

    static class HealthCheckTask implements ITask {
        @Override
        public void execute(@NonNull ExecutionContext context) {
            Log.d("shenyunlong", "Performing system health check...");
        }

        @NonNull
        @Override
        public String getTaskId() {
            return HEALTH_CHECK_TASK;
        }
    }

    static class DataSyncTask implements ITask {
        @Override
        public void execute(@NonNull ExecutionContext context) {
            Log.d("shenyunlong", "Syncing data...");
        }

        @NonNull
        @Override
        public String getTaskId() {
            return DATA_SYNC_TASK;
        }
    }

    private static class NetworkAvailableCondition implements ITriggerCondition {
        @Override
        public boolean shouldTrigger(@NonNull ExecutionContext context) {
            boolean isNetworkAvailable = checkNetworkAvailability();
            Log.d("shenyunlong", "Network available: " + isNetworkAvailable);
            return isNetworkAvailable;
        }

        private boolean checkNetworkAvailability() {
            // 实际的网络检查逻辑
            return true;
        }
    }

    private static class ReportGenerationTask implements ITask {
        @Override
        public void execute(@NonNull ExecutionContext context) {
            Log.d("shenyunlong", "Generating report...");
        }

        @NonNull
        @Override
        public String getTaskId() {
            return REPORT_TASK;
        }
    }
}
