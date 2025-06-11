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

import android.content.Context;
import android.view.LayoutInflater;

import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.HashMap;

class LayoutInflaterFetcher {

    private static LayoutInflater sInflater = null;
    private static Object sOriginFetcher = null;

    static void setOriginFetcher(Class<?> systemServiceRegistryClazz) {
        try {
            Field serviceFetchersField = systemServiceRegistryClazz.getDeclaredField("SYSTEM_SERVICE_FETCHERS");
            serviceFetchersField.setAccessible(true);
            HashMap<String, Object> fetchers = (HashMap<String, Object>) serviceFetchersField.get(null);
            if (fetchers != null) {
                sOriginFetcher = fetchers.get(Context.LAYOUT_INFLATER_SERVICE);
            }
        } catch (Exception e) {
            sOriginFetcher = null;
        }
    }

    static synchronized LayoutInflater getInflater(Object contextImpl) {
        if (sInflater != null) {
            return sInflater;
        }
        Context context = getContext(contextImpl);
        if (context == null && sOriginFetcher != null) {
            return getInflaterFromOriginFetcher(sOriginFetcher, contextImpl);
        }
        sInflater = new AttachRenderLayoutInflater(context);
        return sInflater;
    }

    private static LayoutInflater getInflaterFromOriginFetcher(Object serviceFetcher, Object contextImpl) {
        try {
            Method getService = serviceFetcher.getClass().getDeclaredMethod("getService", contextImpl.getClass());
            getService.setAccessible(true);
            return (LayoutInflater) getService.invoke(serviceFetcher, contextImpl);
        } catch (Exception e) {
            return null;
        }

    }

    private static Context getContext(Object contextImpl) {
        try {
            Method getOuterContext = contextImpl.getClass().getDeclaredMethod("getOuterContext");
            getOuterContext.setAccessible(true);
            return (Context) getOuterContext.invoke(contextImpl);
        } catch (Exception e) {
            return null;
        }
    }
}
