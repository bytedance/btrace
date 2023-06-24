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
#pragma once

#include "render_proxy.h"
#include "render_node_proxy.h"

namespace bytedance {
namespace atrace {
namespace render {

void start() {
    auto apiLevel = android_get_device_api_level();
    if (apiLevel < __ANDROID_API_L__) {
        return;
    }
    startTracingRenderNode(apiLevel);
}

void renderNodeAttachViewInfo(void* renderNode, const char* viewName, const char* xmlName, int32_t viewId) {
    attachViewInfo(renderNode, viewName, xmlName, viewId);
}

void stop() {
    stopTracingRenderNode();
}

} // namespace render
} // namespace atrace
} // namespace bytedance
