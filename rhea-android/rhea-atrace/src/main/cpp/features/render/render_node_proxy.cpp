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
#include <atomic>
#include <trace.h>
#include <sys/mman.h>
#include <sys/user.h>
#include <second_party/byte_hook/shadowhook.h>
#include <debug.h>
#include <cstring>
#include <functional>
#include "render_node_proxy.h"

namespace bytedance {
namespace atrace {
namespace render {

namespace {

int sApiLevel = 0;

char** RenderNode_getNamePtr(void* renderNode);

bool tracingRenderNodePrepareTree();

bool tracingRenderNodeDraw();

bool attachViewInfoToName(void* renderNode, const char* viewName, const char* xmlName, int32_t viewId);

void stopTracingRenderNodePrepareTree();

void stopTracingRenderNodeDraw();

} // anonymous namespace


bool startTracingRenderNode(int apiLevel) {
    sApiLevel = apiLevel;

    if (apiLevel < __ANDROID_API_O__) {
        return false;
    }

    tracingRenderNodePrepareTree();

    tracingRenderNodeDraw();

    return true;
}

void attachViewInfo(void* renderNode, const char* viewName, const char* xmlName, int32_t viewId) {
    if (renderNode == nullptr || viewName == nullptr) {
        ALOGE("attach view info failed, renderNode:%p, viewName:%s", renderNode, viewName);
        return;
    }
    if (xmlName == nullptr) {
        xmlName = "unknown";
    }
    attachViewInfoToName(renderNode, viewName, xmlName, viewId);
}

void stopTracingRenderNode() {
    stopTracingRenderNodePrepareTree();
    stopTracingRenderNodeDraw();
}

namespace {

struct RenderNode {
    void* vptr;
    std::atomic<int32_t> mCount; // The only member of the RenderNode parent class VirtualLightRefBase.
    char* mName;
    // ignored
};

struct RenderNode10 {
    void* vptr;
    std::atomic<int32_t> mCount; // The only member of the RenderNode parent class VirtualLightRefBase.
    int64_t mUniqueId;
    char* mName;
    // ignored
};



struct RenderNodeDrawable {
    void* vptr;
    std::atomic<int32_t> fRefCnt;
    int32_t fGenerationId;
    void* renderNode;
};

struct SkDrawableHookStubs {
    void* draw_1;
    void* draw_2;
};

struct SkRect {
    float left;
    float top;
    float right;
    float bottom;
};

using SkDrawableOnGetBounds = SkRect (*)(void*);

constexpr const float BIG_ENOUGH_FLOAT = 1024 * 1024;

int onDrawIndex = -1;
void* originOnDraw = nullptr;
void* renderNodeDrawableVtable = nullptr;
bool renderNodeDrawableVtableHooked = false;
SkDrawableHookStubs drawableHookStubs {
        nullptr,
        nullptr
};
void* RenderNode_prepareTreeImpl_hook_stub = nullptr;
void* RenderNodeDrawable_onDraw_hook_stub = nullptr;

using String8AppendFormat = int32_t (*)(void*, const char*, ...);
String8AppendFormat stringAppendFormat = nullptr;
using String8Clear = void (*)(void*);
String8Clear stringClear = nullptr;

constexpr int STD_FUNCTION_CALL_OPERATOR_VIRTUAL_INDEX = 6;
using PrepareChildFn = std::function<void(void*, void*, void*, bool)>;
using PrepareChild = void(*)(void*, void*, void*, void*, void*);
bool functionVtableHooked = false;
PrepareChild prepareChild = nullptr;
void* DisplayList_prepareListAndChildren_hook_stub = nullptr;
void* RenderNode_prepareTree_hook_stub = nullptr;


char** RenderNode_getNamePtr(void* renderNode) {
    if (renderNode == nullptr) {
        return nullptr;
    }
    if (sApiLevel <= __ANDROID_API_P__) {
        auto node = reinterpret_cast<RenderNode*>(renderNode);
        return &node->mName;
    } else {
        auto node = reinterpret_cast<RenderNode10*>(renderNode);
        return &node->mName;
    }
}

char* RenderNode_getName(void* renderNode) {
    return *RenderNode_getNamePtr(renderNode);
}

void RenderNodeDrawable_onDraw_proxy(void* drawable, void* canvas) {
    auto* renderNodeDrawable = reinterpret_cast<RenderNodeDrawable*>(drawable);
    auto* nodeName = RenderNode_getName(renderNodeDrawable->renderNode);
    ATRACE_FORMAT("draw %s", nodeName);
    if (originOnDraw != nullptr) {
        auto* onDraw = reinterpret_cast<void(*)(void*, void*)>(originOnDraw);
        onDraw(drawable, canvas);
    }
}

void ensureRenderNodeDrawableVtableHooked(void* drawable) {
    if (renderNodeDrawableVtableHooked) {
        return;
    }
    void* vptr = *reinterpret_cast<void**>(drawable);
    void** vtable = reinterpret_cast<void**>(vptr);
    auto vtbl = reinterpret_cast<size_t>(vtable);
    auto* page = reinterpret_cast<void*>(vtbl & (~static_cast<size_t>((PAGE_SIZE - 1))));
    if (mprotect(page, PAGE_SIZE, PROT_READ | PROT_WRITE) == 0) {
        originOnDraw = vtable[onDrawIndex];
        vtable[onDrawIndex] = reinterpret_cast<void*>(RenderNodeDrawable_onDraw_proxy);
        renderNodeDrawableVtableHooked = true;
        __builtin___clear_cache(reinterpret_cast<char*>(page), reinterpret_cast<char*>(page) + PAGE_SIZE);
        mprotect(page, PAGE_SIZE, PROT_READ);
    }
}

bool isRenderNodeDrawable(void* drawable) {
    if (onDrawIndex < 0) {
        return false;
    }
    void* vptr = *reinterpret_cast<void**>(drawable);
    if (renderNodeDrawableVtable != nullptr && renderNodeDrawableVtable == vptr) {
        return true;
    }
    void** vtable = reinterpret_cast<void**>(vptr);
    auto onGetBounds = reinterpret_cast<SkDrawableOnGetBounds>(vtable[onDrawIndex - 1]);
    if (onGetBounds != nullptr) {
        // Only RenderNodeDrawable would return a super large rect as bound.
        auto bounds = onGetBounds(drawable);
        bool isBoundsBigEnough = bounds.right > bounds.left && bounds.right - bounds.left > BIG_ENOUGH_FLOAT &&
                                 bounds.bottom > bounds.top && bounds.bottom - bounds.top > BIG_ENOUGH_FLOAT;
        if (isBoundsBigEnough) {
            renderNodeDrawableVtable = vptr;
        }
        return isBoundsBigEnough;
    }
    return true;
}

void SkDrawable_draw1_proxy(void* drawable, void* canvas) {
    SHADOWHOOK_STACK_SCOPE();
    if (isRenderNodeDrawable(drawable)) {
        ensureRenderNodeDrawableVtableHooked(drawable);
    }
    SHADOWHOOK_CALL_PREV(SkDrawable_draw1_proxy, drawable, canvas);
}

void SkDrawable_draw2_proxy(void* drawable, void* canvas, void* matrix) {
    SHADOWHOOK_STACK_SCOPE();
    if (isRenderNodeDrawable(drawable)) {
        ensureRenderNodeDrawableVtableHooked(drawable);
    }
    SHADOWHOOK_CALL_PREV(SkDrawable_draw2_proxy, drawable, canvas, matrix);
}

void RenderNode_prepareTreeImpl_proxy(void* renderNode, void* treeObserver, void* treeInfo, bool functorsNeedLayer) {
    SHADOWHOOK_STACK_SCOPE();
    const char* nodeName = RenderNode_getName(renderNode);
    ATRACE_FORMAT("prepare %s", nodeName);
    SHADOWHOOK_ALLOW_REENTRANT();
    SHADOWHOOK_CALL_PREV(RenderNode_prepareTreeImpl_proxy, renderNode, treeObserver, treeInfo, functorsNeedLayer);
}

void RenderNode_prepareTreeImpl_mi_proxy(void* renderNode, void* treeObserver, void* treeInfo, bool functorsNeedLayer, int i) {
    SHADOWHOOK_STACK_SCOPE();
    const char* nodeName = RenderNode_getName(renderNode);
    ATRACE_FORMAT("prepare %s", nodeName);
    SHADOWHOOK_ALLOW_REENTRANT();
    SHADOWHOOK_CALL_PREV(RenderNode_prepareTreeImpl_mi_proxy, renderNode, treeObserver, treeInfo, functorsNeedLayer, i);
}

void RenderNodeDrawable_forceDraw_proxy(void* drawable, void* canvas) {
    SHADOWHOOK_STACK_SCOPE();
    auto* renderNodeDrawable = reinterpret_cast<RenderNodeDrawable*>(drawable);
    auto* nodeName = RenderNode_getName(renderNodeDrawable->renderNode);
    ATRACE_FORMAT("render %s", nodeName);
    SHADOWHOOK_ALLOW_REENTRANT();
    SHADOWHOOK_CALL_PREV(RenderNodeDrawable_forceDraw_proxy, drawable, canvas);
}

void RenderNode_prepareTree_proxy(void *renderNode, void *treeInfo) {
    SHADOWHOOK_STACK_SCOPE();
    const char* nodeName = RenderNode_getName(renderNode);
    ATRACE_FORMAT("prepareRoot %s", nodeName);
    SHADOWHOOK_ALLOW_REENTRANT();
    SHADOWHOOK_CALL_PREV(RenderNode_prepareTree_proxy, renderNode, treeInfo);
}

void prepareTreeFunction(void* lambdaThis, void** renderNode, void* treeObserver, void* treeInfo, void* needLayer) {
    const char* nodeName = RenderNode_getName(*renderNode);
    ATRACE_FORMAT("prepare %s", nodeName);
    if (prepareChild != nullptr) {
        prepareChild(lambdaThis, renderNode, treeObserver, treeInfo, needLayer);
    }
}

void SkiaDisplayList_prepareListAndChildren_proxy(
        void* displayList, void* treeObserver, void* treeInfo, bool functorsNeedLayer,
        PrepareChildFn childFn) {
    SHADOWHOOK_STACK_SCOPE();
    if (!functionVtableHooked) {
        void** vtable = *reinterpret_cast<void***>(&childFn);
        auto vtbl = reinterpret_cast<size_t>(vtable);
        auto* page = reinterpret_cast<void*>(vtbl & (~static_cast<size_t>((PAGE_SIZE - 1))));
        if (mprotect(page, PAGE_SIZE, PROT_READ | PROT_WRITE) == 0) {
            prepareChild = reinterpret_cast<PrepareChild>(vtable[STD_FUNCTION_CALL_OPERATOR_VIRTUAL_INDEX]);
            vtable[STD_FUNCTION_CALL_OPERATOR_VIRTUAL_INDEX] = reinterpret_cast<void*>(prepareTreeFunction);
            functionVtableHooked = true;
            __builtin___clear_cache(reinterpret_cast<char*>(page), reinterpret_cast<char*>(page) + PAGE_SIZE);
            mprotect(page, PAGE_SIZE, PROT_READ);
        }
    }

    SHADOWHOOK_CALL_PREV(SkiaDisplayList_prepareListAndChildren_proxy, displayList, treeObserver, treeInfo, functorsNeedLayer, childFn);
}

bool isXiaomiOrRedmi() {
    char value[92] = { 0 };
    if (__system_property_get("ro.product.brand", value) < 1) {
        return false;
    }
    return strcmp(value, "Xiaomi") == 0 || strcmp(value, "Redmi") == 0;
}

bool tracingRenderNodePrepareTree() {
    if (RenderNode_prepareTreeImpl_hook_stub != nullptr
        || DisplayList_prepareListAndChildren_hook_stub != nullptr) {
        return true;
    }
    RenderNode_prepareTreeImpl_hook_stub = shadowhook_hook_sym_name(
            "libhwui.so",
            "_ZN7android10uirenderer10RenderNode15prepareTreeImplERNS0_12TreeObserverERNS0_8TreeInfoEb",
            reinterpret_cast<void*>(RenderNode_prepareTreeImpl_proxy),
            nullptr);
    if (RenderNode_prepareTreeImpl_hook_stub == nullptr && isXiaomiOrRedmi()) {
        RenderNode_prepareTreeImpl_hook_stub = shadowhook_hook_sym_name(
                "libhwui.so",
                "_ZN7android10uirenderer10RenderNode15prepareTreeImplERNS0_12TreeObserverERNS0_8TreeInfoEbi",
                reinterpret_cast<void*>(RenderNode_prepareTreeImpl_mi_proxy),
                nullptr);
    }
    if (RenderNode_prepareTreeImpl_hook_stub != nullptr) {
        return true;
    }

    ALOGD("hook RenderNode.prepareTreeImpl() failed, error: %s", shadowhook_to_errmsg(shadowhook_get_errno()));

    DisplayList_prepareListAndChildren_hook_stub = shadowhook_hook_sym_name(
            "libhwui.so",
            "_ZN7android10uirenderer12skiapipeline15SkiaDisplayList22prepareListAndChildrenERNS0_12TreeObserverERNS0_8TreeInfoEbNSt3__18functionIFvPNS0_10RenderNodeES4_S6_bEEE",
            reinterpret_cast<void*>(SkiaDisplayList_prepareListAndChildren_proxy),
            nullptr);
    RenderNode_prepareTree_hook_stub = shadowhook_hook_sym_name(
            "libhwui.so",
            "_ZN7android10uirenderer10RenderNode11prepareTreeERNS0_8TreeInfoE",
            reinterpret_cast<void*>(RenderNode_prepareTree_proxy),
            nullptr);
    return DisplayList_prepareListAndChildren_hook_stub != nullptr;
}

bool tracingRenderNodeDraw() {
    if (RenderNodeDrawable_onDraw_hook_stub != nullptr || renderNodeDrawableVtable != nullptr) {
        return true;
    }
    const char* forceDrawSym;
    if (sApiLevel > __ANDROID_API_Q__) {
        forceDrawSym = "_ZNK7android10uirenderer12skiapipeline18RenderNodeDrawable9forceDrawEP8SkCanvas";
    } else {
        forceDrawSym = "_ZN7android10uirenderer12skiapipeline18RenderNodeDrawable9forceDrawEP8SkCanvas";
    }
    auto stub = shadowhook_hook_sym_name(
            "libhwui.so",
            forceDrawSym,
            reinterpret_cast<void*>(RenderNodeDrawable_forceDraw_proxy),
            nullptr);
    if (stub != nullptr) {
        RenderNodeDrawable_onDraw_hook_stub = stub;
        return true;
    }
    if (sApiLevel < __ANDROID_API_P__) {
        onDrawIndex = 7;
    } else  {
        onDrawIndex = 8;
    }
    if (drawableHookStubs.draw_1 == nullptr) {
        drawableHookStubs.draw_1 = shadowhook_hook_sym_name(
                "libhwui.so",
                "_ZN10SkDrawable4drawEP8SkCanvasff",
                reinterpret_cast<void*>(SkDrawable_draw1_proxy), nullptr);

    }
    if (drawableHookStubs.draw_2 == nullptr) {
        drawableHookStubs.draw_2 = shadowhook_hook_sym_name(
                "libhwui.so",
                "_ZN10SkDrawable4drawEP8SkCanvasPK8SkMatrix",
                reinterpret_cast<void*>(SkDrawable_draw2_proxy), nullptr);
    }

    return true;
}

bool attachViewInfoToName(void* renderNode, const char* viewName, const char* xmlName, int32_t viewId) {
    if (stringAppendFormat == nullptr || stringClear == nullptr) {
        void* handle = shadowhook_dlopen("libutils.so");
        if (handle != nullptr) {
            stringAppendFormat = reinterpret_cast<String8AppendFormat>(shadowhook_dlsym(handle, "_ZN7android7String812appendFormatEPKcz"));
            stringClear = reinterpret_cast<String8Clear>(shadowhook_dlsym(handle,"_ZN7android7String85clearEv"));
            shadowhook_dlclose(handle);
        }
    }
    if (stringAppendFormat == nullptr || stringClear == nullptr) {
        ALOGE("fail to find string8 operation function, append:%p, clear:%p ", stringAppendFormat, stringClear);
        return false;
    }
    void* string8Name = reinterpret_cast<void*>(RenderNode_getNamePtr(renderNode));
    if (string8Name == nullptr) {
        return false;
    }
    stringClear(string8Name);
    if (viewId != -1) {
        stringAppendFormat(string8Name, "%s(%s@0x%0x)", viewName, xmlName, viewId);
    } else {
        stringAppendFormat(string8Name, "%s(%s@NO_ID)", viewName, xmlName);
    }
    return true;
}

void stopTracingRenderNodePrepareTree() {
    if (RenderNode_prepareTreeImpl_hook_stub != nullptr) {
        shadowhook_unhook(RenderNode_prepareTreeImpl_hook_stub);
    }
    if (DisplayList_prepareListAndChildren_hook_stub != nullptr) {
        shadowhook_unhook(DisplayList_prepareListAndChildren_hook_stub);
    }
    if (RenderNode_prepareTree_hook_stub != nullptr) {
        shadowhook_unhook(RenderNode_prepareTree_hook_stub);
    }
}

void stopTracingRenderNodeDraw() {
    if (RenderNodeDrawable_onDraw_hook_stub != nullptr) {
        shadowhook_unhook(RenderNodeDrawable_onDraw_hook_stub);
    }
    if (drawableHookStubs.draw_1 != nullptr) {
        shadowhook_unhook(drawableHookStubs.draw_1);
    }
    if (drawableHookStubs.draw_2 != nullptr) {
        shadowhook_unhook(drawableHookStubs.draw_2);
    }
    if (renderNodeDrawableVtable != nullptr && originOnDraw != nullptr && onDrawIndex >= 0) {
        void** vtable = reinterpret_cast<void**>(renderNodeDrawableVtable);
        vtable[onDrawIndex] = originOnDraw;
    }
}

} // anonymous namespace
} // namespace render
} // namespace atrace
} // namespace bytedance
