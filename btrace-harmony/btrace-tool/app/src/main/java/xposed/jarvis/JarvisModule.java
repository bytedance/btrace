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

package xposed.jarvis;

import android.app.Application;
import android.content.Context;

import com.bytedance.common.utility.io.IOUtils;
import com.bytedance.jarvis.core.Jarvis;

import org.json.JSONArray;
import org.json.JSONObject;
import org.json.JSONTokener;

import java.io.File;

import de.robv.android.xposed.IXposedHookLoadPackage;
import de.robv.android.xposed.XC_MethodHook;
import de.robv.android.xposed.XC_MethodReplacement;
import de.robv.android.xposed.XposedBridge;
import de.robv.android.xposed.XposedHelpers;
import de.robv.android.xposed.callbacks.XC_LoadPackage;

public class JarvisModule implements IXposedHookLoadPackage {
    private static Context context;
    private static ClassLoader classLoader;

    @Override
    public void handleLoadPackage(XC_LoadPackage.LoadPackageParam loadPackageParam) throws Throwable {
        if (loadPackageParam.processName.contains(":")) {
            XLog.i("ignore process: " + loadPackageParam.processName);
            return;
        }
        classLoader = loadPackageParam.classLoader;
        XLog.i("start handleLoadPackage: " + loadPackageParam.processName);
        try {
            processLoadPackage();
        } catch (Throwable e) {
            XLog.e("handleLoadPackage error", e);
            throw new RuntimeException(e);
        }
        XLog.i("end handleLoadPackage: " + loadPackageParam.processName);
    }

    private static String copyToInternal(String path) {
        File dir = context.getFilesDir();
        File dir2 = new File(dir, "jarvis-runtime");
        boolean mkdirs = dir2.mkdirs();
        File src = new File(path);
        File dst = new File(dir2, src.getName());
        IOUtils.copyFile(src, dst);
        XLog.i("copyToInternal: " + dst.getAbsolutePath());
        return dst.getAbsolutePath();
    }

    private void processLoadPackage() {
        XposedHelpers.findAndHookMethod(Runtime.class, "loadLibrary0", ClassLoader.class, Class.class, String.class, new XC_MethodReplacement() {
            @Override
            protected Object replaceHookedMethod(MethodHookParam methodHookParam) throws Throwable {
                String library = (String) methodHookParam.args[2];
                if (!library.equals("jarvis-trace")) {
                    return XposedBridge.invokeOriginalMethod(methodHookParam.method, methodHookParam.thisObject, methodHookParam.args);
                }
                Runtime.getRuntime().load(copyToInternal("/data/local/tmp/libshadowhook.so"));
                Runtime.getRuntime().load(copyToInternal("/data/local/tmp/libbytehook.so"));
                Runtime.getRuntime().load(copyToInternal("/data/local/tmp/libjarvis-trace.so"));
                return null;
            }
        });
        XposedHelpers.findAndHookMethod(Application.class, "attach", Context.class, new XC_MethodHook() {
            @Override
            protected void afterHookedMethod(MethodHookParam param) throws Throwable {
                if (context != null) {
                    XLog.i("ignore attach: " + param.thisObject);
                    return;
                }
                context = (Context) param.thisObject;
                try {
                    new JarvisInside((Application) param.thisObject).init();
                } catch (Throwable e) {
                    XLog.e("init jarvis error", e);
                }
            }
        });
        try {
            XposedHelpers.findAndHookMethod("com.google.gson.Gson", classLoader, "fromJson", "java.lang.String", "java.lang.Class", new XC_DurationMethodHook() {
                @Override
                protected void afterHookedMethod(MethodHookParam param, long duration) throws Throwable {
                    Jarvis.traceJSON((String) param.args[0], (int) duration);
                }
            });
            XposedHelpers.findAndHookMethod("com.google.gson.Gson", classLoader, "fromJson", "java.lang.String", "java.lang.reflect.Type", new XC_DurationMethodHook() {

                @Override
                protected void afterHookedMethod(MethodHookParam param, long duration) throws Throwable {
                    Jarvis.traceJSON((String) param.args[0], (int) duration);
                }
            });
            XposedHelpers.findAndHookMethod("com.google.gson.Gson", classLoader, "toJson", "java.lang.Object", "java.lang.reflect.Type", new XC_DurationMethodHook() {

                @Override
                protected void afterHookedMethod(MethodHookParam param, long duration) throws Throwable {
                    Jarvis.traceJSON((String) param.getResult(), (int) duration);
                }
            });
        } catch (Throwable e) {
            XLog.e("process gson error", e);
        }
        XposedHelpers.findAndHookConstructor(JSONObject.class, String.class, new XC_DurationMethodHook() {
            @Override
            protected void afterHookedMethod(MethodHookParam param, long duration) throws Throwable {
                Jarvis.traceJSON((String) param.args[0], (int) duration);
            }
        });
        XposedHelpers.findAndHookConstructor(JSONObject.class, JSONTokener.class, new XC_DurationMethodHook() {
            @Override
            protected void afterHookedMethod(MethodHookParam param, long duration) throws Throwable {
                Jarvis.traceJSON(JSONs.asString((JSONTokener) param.args[0]), (int) duration);
            }
        });
        XposedHelpers.findAndHookConstructor(JSONArray.class, String.class, new XC_DurationMethodHook() {
            @Override
            protected void afterHookedMethod(MethodHookParam param, long duration) throws Throwable {
                Jarvis.traceJSON((String) param.args[0], (int) duration);
            }
        });
        XposedHelpers.findAndHookConstructor(JSONArray.class, JSONTokener.class, new XC_DurationMethodHook() {
            @Override
            protected void afterHookedMethod(MethodHookParam param, long duration) throws Throwable {
                Jarvis.traceJSON(JSONs.asString((JSONTokener) param.args[0]), (int) duration);
            }
        });
        XposedHelpers.findAndHookMethod(JSONObject.class, "toString", new XC_DurationMethodHook() {
            @Override
            protected void afterHookedMethod(MethodHookParam param, long duration) throws Throwable {
                Jarvis.traceJSON((String) param.getResult(), (int) duration);
            }
        });
        XposedHelpers.findAndHookMethod(JSONArray.class, "toString", new XC_DurationMethodHook() {
            @Override
            protected void afterHookedMethod(MethodHookParam param, long duration) throws Throwable {
                Jarvis.traceJSON((String) param.getResult(), (int) duration);
            }
        });
    }
}
