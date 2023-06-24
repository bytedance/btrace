/*
 * Copyright (C) 2021 ByteDance Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.bytedance.rheatrace.atrace.render;

import android.annotation.SuppressLint;
import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;

import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.ArrayList;
import java.util.Collection;

public class RenderTracer {

    private static final String TAG = "rhea:RenderTracer";

    public static void onTraceStart() {
        installInflaterHook();
        tryAttachWindowViews();
    }

    @SuppressLint("PrivateApi")
    private static void installInflaterHook() {
        try {
            Class<?> systemServiceRegistryClazz = Class.forName("android.app.SystemServiceRegistry");
            Class<?> serviceFetcherClazz = Class.forName("android.app.SystemServiceRegistry$ServiceFetcher");
            LayoutInflaterFetcher.setOriginFetcher(systemServiceRegistryClazz);
            Method registerService = systemServiceRegistryClazz.getDeclaredMethod("registerService", String.class, Class.class, serviceFetcherClazz);
            registerService.setAccessible(true);
            Object serviceFetcher = Proxy.newProxyInstance(serviceFetcherClazz.getClassLoader(), new Class[]{serviceFetcherClazz}, (proxy, method, args) -> LayoutInflaterFetcher.getInflater(args[0]));
            registerService.invoke(null, Context.LAYOUT_INFLATER_SERVICE, LayoutInflater.class, serviceFetcher);
        } catch (Exception e) {
            Log.e(TAG, "install inflater hook failed, error: " + e.toString());
        }
    }

    private static void registerWindowViewAddMonitor() {

    }

    private static void tryAttachWindowViews() {
        try {
            Class<?> windowManagerGlobalClazz = Class.forName("android.view.WindowManagerGlobal");
            Method getInstance = windowManagerGlobalClazz.getDeclaredMethod("getInstance");
            getInstance.setAccessible(true);
            Object windowManagerGlobal = getInstance.invoke(null);
            Field mRoots = windowManagerGlobalClazz.getDeclaredField("mRoots");
            mRoots.setAccessible(true);
            ArrayList<Object> roots = (ArrayList<Object>) mRoots.get(windowManagerGlobal);
            MonitoredArrayList<Object> monitoredRoots = new MonitoredArrayList<>(roots, true);
            mRoots.set(windowManagerGlobal, monitoredRoots);
            Field mViews = windowManagerGlobalClazz.getDeclaredField("mViews");
            mViews.setAccessible(true);
            ArrayList<View> views = (ArrayList<View>) mViews.get(windowManagerGlobal);
            for (View view : views) {
                RenderUtils.attachViewInfo(view);
            }
        } catch (Exception e) {
            Log.e(TAG, "attach window views failed, error: " + e.toString());
        }
    }

    private static class MonitoredArrayList<E> extends ArrayList<E> {

        private final Handler mHandler = new Handler(Looper.getMainLooper());
        private final boolean mIsViewRootList;

        MonitoredArrayList(Collection<E> c, boolean isViewRootList) {
            super(c);
            mIsViewRootList = isViewRootList;
        }

        @Override
        public boolean add(E e) {
            if (mIsViewRootList) {
                mHandler.post(() -> RenderUtils.attachViewRoot(e));
            }
            return super.add(e);
        }
    }
}
