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

#define LOG_TAG "Rhea.classProxy"

#include <utils/debug.h>
#include "class_linker_proxy.h"
#include "trace.h"
#include "../hook/shadowhook_util.h"


namespace bytedance {
    namespace atrace {
        namespace klass {
            // art::ClassLinker::DefineClass(art::Thread*, char const*, unsigned long, art::Handle<art::mirror::ClassLoader>, art::DexFile const&, art::DexFile::ClassDef const&)
            static const char *DEFINE_CLASS_SYM_PRE_Q = "_ZN3art11ClassLinker11DefineClassEPNS_6ThreadEPKcmNS_6HandleINS_6mirror11ClassLoaderEEERKNS_7DexFileERKNS9_8ClassDefE";
            // art::ClassLinker::DefineClass(art::Thread*, char const*, unsigned long, art::Handle<art::mirror::ClassLoader>, art::DexFile const&, art::dex::ClassDef const&)
            static const char *DEFINE_CLASS_SYM_Q_AND_AFTER = "_ZN3art11ClassLinker11DefineClassEPNS_6ThreadEPKcmNS_6HandleINS_6mirror11ClassLoaderEEERKNS_7DexFileERKNS_3dex8ClassDefE";

            static void *
            DefineClass_proxy(void *linker, void *self, const char *descriptor, size_t hash,
                              void *class_loader, const void *dex_file,
                              const void *dex_class_def) {
                SHADOWHOOK_STACK_SCOPE();
                ATRACE_FORMAT("DefineClass %s", descriptor);
                return SHADOWHOOK_CALL_PREV(DefineClass_proxy,
                                            linker,
                                            self, descriptor, hash, class_loader, dex_file,
                                            dex_class_def);
            }

            bool hook() {
                const char *sym = android_get_device_api_level() < __ANDROID_API_Q__ ?
                                  DEFINE_CLASS_SYM_PRE_Q : DEFINE_CLASS_SYM_Q_AND_AFTER;
                return bytedance::atrace::shadowhook::hook_sym_name(
                        "libart.so", sym, (void *) DefineClass_proxy);
            }
        } // namespace klass
    }  // namespace atrace
}  // namespace bytedance
