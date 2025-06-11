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
//
// @author lupengfei
//

#pragma once

#include <cstdint>
#include <string>

namespace rheatrace {

#define RUNTIME_INSTANCE_SYMBOL  "_ZN3art7Runtime9instance_E"
#define HEAP_SET_ALLOC_LISTENER "_ZN3art2gc4Heap21SetAllocationListenerEPNS0_18AllocationListenerE"
#define HEAP_REMOVE_ALLOC_LISTENER "_ZN3art2gc4Heap24RemoveAllocationListenerEv"
#define CLASS_PRETTY_DESCRIPTOR "_ZN3art6mirror5Class16PrettyDescriptorEv"
#define StackVisitor_WALK_VISITOR "_ZN3art12StackVisitor9WalkStackILNS0_16CountTransitionsE0EEEvb"
#define StackVisitor_GET_DEXPC "_ZNK3art12StackVisitor8GetDexPcEb"
#define ArtMethod_METHOD_NAME "_ZN3art9ArtMethod12PrettyMethodEb"
#define Monitor_TRANSLATE_LOCATION "_ZN3art7Monitor17TranslateLocationEPNS_9ArtMethodEjPPKcPi"

#define ThreadList_RUN_CHECKPOINT "_ZN3art10ThreadList13RunCheckpointEPNS_7ClosureES2_"
#define ThreadList_RUN_CHECKPOINT_15 "_ZN3art10ThreadList13RunCheckpointEPNS_7ClosureES2_b"
#define Thread_RESET_QUICK_ALLOC_ENTRY_POINTS_FOR_THREAD "_ZN3art6Thread35ResetQuickAllocEntryPointsForThreadEv"
#define Thread_RESET_QUICK_ALLOC_ENTRY_POINTS_FOR_THREAD_BOOL "_ZN3art6Thread35ResetQuickAllocEntryPointsForThreadEb"
#define SET_QUICK_ALLOC_ENTRY_POINTS_INSTRUMENTED "_ZN3art36SetQuickAllocEntryPointsInstrumentedEb"

static constexpr const char* THREAD_INIT = "_ZN3art6Thread4InitEPNS_10ThreadListEPNS_9JavaVMExtEPNS_9JNIEnvExtE";

typedef uint64_t (*GetInt64_)(void *);

typedef std::string (*GetString_)(void *);

typedef void (*SetPtr_)(void *, void *);

typedef void (*CallVoid_)(void *);

typedef void (*CallVoidBool_)(void *, bool);

}
