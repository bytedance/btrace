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

import android.content.res.Resources;
import android.os.Build;
import android.util.Log;
import android.util.SparseArray;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;

import androidx.annotation.Keep;

import java.lang.reflect.Field;
import java.lang.reflect.Method;

@Keep
class RenderUtils {

    private static final String TAG = "rhea:RenderUtils";

    private static final int VIEW_ATTACHED_TAG_INDEX = View.NO_ID;
    private static final int VIEW_ROOT_ATTACHED_TAG_INDEX = VIEW_ATTACHED_TAG_INDEX >>> 1;

    private static Field sRenderNodeFieldOfView = null;
    private static Field sNativeRenderNodeOfRenderNode = null; // type long
    private static Field sAttachInfoFieldOfView = null;
    private static Field sViewRootImplFieldOfAttachInfo = null;
    private static Field sThreadedRendererFieldOfAttachInfo = null;
    private static Field sRootNodeFieldOfThreadedRenderer = null;
    private static Method sGetRootNodeMethodOfThreadRenderer = null;
    private static Field sWindowAttributesFieldOfViewRootImpl = null; // type WindowManager.LayoutParams
    private static Field sAttachInfoFieldOfViewRootImpl = null;
    private static Field sViewFieldOfViewRootImpl = null;

    private static Object getRenderNode(View view) {
        if (view == null) {
            Log.e(TAG, "Trying to get render node from null view.");
            return null;
        }
        try {
            if (sRenderNodeFieldOfView == null) {
                sRenderNodeFieldOfView = View.class.getDeclaredField("mRenderNode");
                sRenderNodeFieldOfView.setAccessible(true);
            }
            return sRenderNodeFieldOfView.get(view);
        } catch (Exception e) {
            Log.e(TAG, e.toString());
            return null;
        }
    }

    private static long getNativeRenderNode(Object renderNode) {
        if (renderNode == null) {
            Log.e(TAG, "Trying to get native render node from null render node object.");
            return 0;
        }
        try {
            if (sNativeRenderNodeOfRenderNode == null) {
                sNativeRenderNodeOfRenderNode = renderNode.getClass().getDeclaredField("mNativeRenderNode");
                sNativeRenderNodeOfRenderNode.setAccessible(true);
            }
            return (long) sNativeRenderNodeOfRenderNode.get(renderNode);
        } catch (Exception e) {
            Log.e(TAG, e.toString());
            return 0;
        }
    }

    private static Object getAttachInfo(View view) {
        if (view == null) {
            Log.e(TAG, "Trying to get attach info from null view.");
            return null;
        }
        try {
            if (sAttachInfoFieldOfView == null) {
                sAttachInfoFieldOfView = View.class.getDeclaredField("mAttachInfo");
                sAttachInfoFieldOfView.setAccessible(true);
            }
            return sAttachInfoFieldOfView.get(view);
        } catch (Exception e) {
            Log.e(TAG, e.toString());
            return null;
        }
    }

    private static Object getAttachInfoFromViewRoot(Object viewRoot) {
        if (viewRoot == null) {
            Log.e(TAG, "Trying to get attach info from null view root.");
            return null;
        }
        try {
            if (sAttachInfoFieldOfViewRootImpl == null) {
                sAttachInfoFieldOfViewRootImpl = viewRoot.getClass().getDeclaredField("mAttachInfo");
                sAttachInfoFieldOfViewRootImpl.setAccessible(true);
            }
            return sAttachInfoFieldOfViewRootImpl.get(viewRoot);
        } catch (Exception e) {
            Log.e(TAG, e.toString());
            return null;
        }
    }

    private static Object getViewRootImpl(Object attachInfo) {
        if (attachInfo == null) {
            Log.e(TAG, "Trying to get view root from null attach info.");
            return null;
        }
        try {
            if (sViewRootImplFieldOfAttachInfo == null) {
                sViewRootImplFieldOfAttachInfo = attachInfo.getClass().getDeclaredField("mViewRootImpl");
                sViewRootImplFieldOfAttachInfo.setAccessible(true);
            }
            return sViewRootImplFieldOfAttachInfo.get(attachInfo);
        } catch (Exception e) {
            Log.e(TAG, e.toString());
            return null;
        }
    }

    private static Object getThreadedRenderer(Object attachInfo) {
        if (attachInfo == null) {
            Log.e(TAG, "Trying to get threaded renderer from null attach info.");
            return null;
        }
        try {
            if (sThreadedRendererFieldOfAttachInfo == null) {
                sThreadedRendererFieldOfAttachInfo = attachInfo.getClass().getDeclaredField("mThreadedRenderer");
                sThreadedRendererFieldOfAttachInfo.setAccessible(true);
            }
            return sThreadedRendererFieldOfAttachInfo.get(attachInfo);
        } catch (Exception e) {
            Log.e(TAG, e.toString());
            return null;
        }
    }

    private static Object getRootNode(Object threadedRenderer) {
        if (threadedRenderer == null) {
            Log.e(TAG, "Trying to get root node from null threaded renderer.");
            return null;
        }
        if (Build.VERSION.SDK_INT > Build.VERSION_CODES.P) {
            try {
                if (sGetRootNodeMethodOfThreadRenderer == null) {
                    sGetRootNodeMethodOfThreadRenderer = threadedRenderer.getClass().getDeclaredMethod("getRootNode");
                    sGetRootNodeMethodOfThreadRenderer.setAccessible(true);
                }
                return sGetRootNodeMethodOfThreadRenderer.invoke(threadedRenderer);
            } catch (Exception e) {
                Log.e(TAG, e.toString());
            }
        }
        try {
            if (sRootNodeFieldOfThreadedRenderer == null) {
                sRootNodeFieldOfThreadedRenderer = threadedRenderer.getClass().getDeclaredField("mRootNode");
                sRootNodeFieldOfThreadedRenderer.setAccessible(true);
            }
            return sRootNodeFieldOfThreadedRenderer.get(threadedRenderer);
        } catch (Exception e) {
            Log.e(TAG, e.toString());
            return null;
        }
    }

    private static WindowManager.LayoutParams getWindowAttributes(Object viewRootImpl) {
        if (viewRootImpl == null) {
            Log.e(TAG, "Trying get window attributes from null view root.");
            return null;
        }
        try {
            if (sWindowAttributesFieldOfViewRootImpl == null) {
                sWindowAttributesFieldOfViewRootImpl = viewRootImpl.getClass().getDeclaredField("mWindowAttributes");
                sWindowAttributesFieldOfViewRootImpl.setAccessible(true);
            }
            return (WindowManager.LayoutParams) sWindowAttributesFieldOfViewRootImpl.get(viewRootImpl);
        } catch (Exception e) {
            Log.e(TAG, e.toString());
            return null;
        }
    }

    private static View getRootView(Object viewRootImpl) {
        if (viewRootImpl == null) {
            Log.e(TAG, "Trying to get root view from null view root.");
            return null;
        }
        try {
            if (sViewFieldOfViewRootImpl == null) {
                sViewFieldOfViewRootImpl = viewRootImpl.getClass().getDeclaredField("mView");
                sViewFieldOfViewRootImpl.setAccessible(true);
            }
            return (View) sViewFieldOfViewRootImpl.get(viewRootImpl);
        } catch (Exception e) {
            Log.e(TAG, e.toString());
            return null;
        }
    }

    private static void attachViewInfo(View view, String xmlName) {
        Object attachedTag = view.getTag(VIEW_ATTACHED_TAG_INDEX);
        if (attachedTag != null) {
            // already attached
            return;
        }
        if (xmlName == null) {
            xmlName = "unknown";
        }
        int colonIndex = xmlName.indexOf(':');
        if (colonIndex >= 0 && !xmlName.startsWith("android:")) {
            xmlName = xmlName.substring(colonIndex + 1);
        }
        Object renderNode = getRenderNode(view);
        if (renderNode != null) {
            long nativeRenderNode = getNativeRenderNode(renderNode);
            if (nativeRenderNode != 0) {
                nAttachViewInfo(nativeRenderNode, view.getClass().getCanonicalName(), xmlName, view.getId());
                view.setTag(VIEW_ATTACHED_TAG_INDEX, new Object());
            }
        }
    }

    private static void rAttachViewInfo(View view, String xml) {
        attachViewInfo(view, xml);
        if (view instanceof ViewGroup) {
            ViewGroup container = (ViewGroup) view;
            int childCount = container.getChildCount();
            for (int i = 0; i < childCount; i++) {
                rAttachViewInfo(container.getChildAt(i), xml);
            }
        }
    }

    public static void onViewInflated(View root, String xml, boolean ignoreRoot) {
        if (ignoreRoot) {
            if (root instanceof ViewGroup) {
                ViewGroup container = (ViewGroup) root;
                int childCount = container.getChildCount();
                View lastAddedChild = container.getChildAt(childCount - 1);
                rAttachViewInfo(lastAddedChild, xml);
            }
        } else {
            rAttachViewInfo(root, xml);
        }
    }

    private static void rAttachViewInfo(View view, SparseArray<String> cache) {
        int layoutId = 0;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            layoutId = view.getSourceLayoutResId();
        }
        if (layoutId == Resources.ID_NULL) {
            layoutId = 0;
        }
        String xml = null;
        if (layoutId != 0) {
            xml = cache.get(layoutId);
            if (xml == null) {
                xml = view.getResources().getResourceName(layoutId);
                cache.put(layoutId, xml);
            }
        }
        attachViewInfo(view, xml);
        if (view instanceof ViewGroup) {
            ViewGroup container = (ViewGroup) view;
            int childCount = container.getChildCount();
            for (int i = 0; i < childCount; i++) {
                View child = container.getChildAt(i);
                if (child != null && child.getParent() == container) {
                    rAttachViewInfo(child, cache);
                }
            }
        }
    }

    private static void attachRootViewInfo(View root) {
        Object attachedTag = root.getTag(VIEW_ROOT_ATTACHED_TAG_INDEX);
        if (attachedTag != null) {
            // already attached
            return;
        }
        Object attachInfo = getAttachInfo(root);
        long nativeRootRenderNode = getNativeRenderNode(getRootNode(getThreadedRenderer(attachInfo)));
        WindowManager.LayoutParams layoutParams = getWindowAttributes(getViewRootImpl(attachInfo));
        if (nativeRootRenderNode != 0 && layoutParams != null) {
            nAttachRootViewInfo(nativeRootRenderNode, layoutParams.getTitle().toString());
            root.setTag(VIEW_ROOT_ATTACHED_TAG_INDEX, new Object());
        } else {
            Log.e(TAG, "root view info attach failed, nativeRootRenderNode=" + nativeRootRenderNode + ",layoutParams=" + layoutParams);
        }
    }

    private static void attachViewRootInfo(Object viewRootImpl) {
        if (viewRootImpl == null) {
            Log.e(TAG, "trying to attach view root info from null view root impl");
        }
        Object attachInfo = getAttachInfoFromViewRoot(viewRootImpl);
        long nativeRootRenderNode = getNativeRenderNode(getRootNode(getThreadedRenderer(attachInfo)));
        WindowManager.LayoutParams layoutParams = getWindowAttributes(getViewRootImpl(attachInfo));
        if (nativeRootRenderNode != 0 && layoutParams != null) {
            nAttachRootViewInfo(nativeRootRenderNode, layoutParams.getTitle().toString());
        } else {
            Log.e(TAG, "root view info attach failed, nativeRootRenderNode=" + nativeRootRenderNode + ",layoutParams=" + layoutParams);
        }
    }

    public static void attachViewInfo(View root) {
        if (root != null) {
            attachRootViewInfo(root);
            rAttachViewInfo(root, new SparseArray<>());
        }
    }

    public static void attachViewRoot(Object viewRootImpl) {
        attachViewRootInfo(viewRootImpl);
        View rootView = getRootView(viewRootImpl);
        if (rootView != null) {
            rAttachViewInfo(rootView, new SparseArray<>());
        }
    }

    private static native void nAttachViewInfo(long nativeRenderNode, String viewName, String xmlName, int viewId);

    private static native void nAttachRootViewInfo(long nativeRootNode, String windowName);
}
