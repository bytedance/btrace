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

#include <stdlib.h>

#include <sys/system_properties.h>

namespace bytedance {
namespace utils {

struct Build {
  static int getAndroidSdk() {
    static auto android_sdk = ([] {
      char sdk_version_str[PROP_VALUE_MAX];
      __system_property_get("ro.build.version.sdk", sdk_version_str);
      return atoi(sdk_version_str);
    })();
    return android_sdk;
  }
};

}  // namespace utils
}  // namespace bytedance
