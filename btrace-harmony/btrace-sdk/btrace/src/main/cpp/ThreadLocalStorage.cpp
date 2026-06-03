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

//
// Created on 2025/10/11.
//

#include "ThreadLocalStorage.h"
#include "ThreadNames.h"
#include "util/thread_util.h"
#include <pthread.h>
#include <atomic>

namespace btrace {

static std::atomic<TLSRecord*> sMainTls{nullptr};

void cleanupKey(void* tls) {
    TLSRecord* r = reinterpret_cast<TLSRecord*>(tls);
//    ThreadNames::get().cleanupForCurrentThread(r->threadNameInfoPtr);
    r->~TLSRecord();
    free(tls);
}

ThreadLocalStorage& ThreadLocalStorage::get() {
    static ThreadLocalStorage inst{};
    return inst;
}

bool ThreadLocalStorage::setup() {
    if (tlsKey) {
        return true;
    }
    return pthread_key_create(&tlsKey, cleanupKey) == 0;
}

bool ThreadLocalStorage::cleanup() {
    if (!tlsKey) {
        return true;
    }
    return pthread_key_delete(tlsKey) == 0;
}

TLSRecord& ThreadLocalStorage::tlsRecord() {
    TLSRecord* ptr = static_cast<TLSRecord*>(pthread_getspecific(tlsKey));
    if (!ptr) {
        void* mem = malloc(sizeof(TLSRecord));
        ptr = new(mem) TLSRecord{};
        pthread_setspecific(tlsKey, ptr);
        if (is_main_thread()) {
            sMainTls.store(ptr, std::memory_order_relaxed);
        }
//        ptr->threadNameInfoPtr = ThreadNames::get().initForCurrentThread();
    }
    return *ptr;
}

TLSRecord* ThreadLocalStorage::getMainTLSRecord() {
    return sMainTls.load(std::memory_order_relaxed);
}

}